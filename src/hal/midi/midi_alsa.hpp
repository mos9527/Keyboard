#ifndef HAL_MIDI_ALSA_IMPL
#define HAL_MIDI_ALSA_IMPL
#include "midi.hpp"
// https://github.com/musescore/MuseScore/blob/master/src/framework/midi/internal/platform/lin/alsamidiinport.cpp
// https://github.com/musescore/MuseScore/blob/master/src/framework/midi/internal/platform/lin/alsamidioutport.cpp
namespace midi {
    struct inputContext_ALSA;
    static void alsa_input_thread_function(inputContext_ALSA* ctx);
    struct inputContext_ALSA : public inputContext {
    private:
        snd_seq_t* handle = nullptr;
        int port_id = -1;
        int device_index = -1;
        std::string device_id;
        int last_alsa_error = 0;
        std::thread input_thread;
        std::atomic<bool> keep_running{false};
        void process_event(snd_seq_event_t* ev) {
            if (ev->type >= SND_SEQ_EVENT_NOTEON && ev->type <= SND_SEQ_EVENT_SONGPOS) {
                uint8_t status = 0; 
                uint8_t b1 = 0;     
                uint8_t b2 = 0;
                if (ev->type == SND_SEQ_EVENT_NOTEON) {
                    status = 0x90 | (ev->data.note.channel & 0x0F);
                    b1 = ev->data.note.note;
                    b2 = ev->data.note.velocity;
                } else if (ev->type == SND_SEQ_EVENT_NOTEOFF) {
                    status = 0x80 | (ev->data.note.channel & 0x0F);
                    b1 = ev->data.note.note;
                    b2 = ev->data.note.velocity;
                } else if (ev->type == SND_SEQ_EVENT_CONTROLLER) { // For CCs (like Mod wheel, Sustain pedal, etc.)
                    status = 0xB0 | (ev->data.control.channel & 0x0F);
                    b1 = ev->data.control.param; // Controller number
                    b2 = ev->data.control.value; // Controller value
                } else if (ev->type == SND_SEQ_EVENT_PITCHBEND) { // For Pitch Bend
                    status = 0xE0 | (ev->data.control.channel & 0x0F);
                    // ALSA pitch bend value is signed (-8192 to +8191).
                    // MIDI pitch bend is 0 to 16383 (14-bit), with 8192 as center.
                    unsigned short level = static_cast<unsigned short>(ev->data.control.value + 8192);
                    b1 = level & 0x7F; // LSB
                    b2 = (level >> 7) & 0x7F; // MSB
                }
                if (status != 0) { 
                    auto msg = midi1_packet(status, b1, b2);
                    std::unique_lock<std::mutex> lock(messageMutex);
                    messages.push(msg);
                    messageCV.notify_one();
                }
            } else if (ev->type == SND_SEQ_EVENT_SYSEX) {
                /* Not supported */
            }
        }
        friend void alsa_input_thread_function(inputContext_ALSA* ctx);
    public:
        inline virtual const uint32_t getIndex() const override { return device_index; }
        inline virtual const bool getStatus() const override { return handle != nullptr && last_alsa_error == 0; }
        inputContext_ALSA() = default;
        inputContext_ALSA(inputDevice_t const& device) : device_id(device.id), device_index(device.index) {
            if (last_alsa_error = snd_seq_open(&handle, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) < 0) { /* error */ handle = nullptr; return; }
            if (last_alsa_error = snd_seq_set_client_name(handle, "Keyboard") < 0) { last_alsa_error = -1; return; }
            port_id = snd_seq_create_simple_port(handle, "Input Port", SND_SEQ_PORT_CAP_WRITE, SND_SEQ_PORT_TYPE_MIDI_GENERIC);
            if (port_id < 0) { last_alsa_error = -1; return; }
            // connect to device by id
            snd_seq_addr_t addr;
            if (last_alsa_error = snd_seq_parse_address(handle, &addr, device_id.c_str()) < 0)
                return;
            if (last_alsa_error = snd_seq_connect_from(handle, port_id, addr.client, addr.port) < 0)
                return;
            keep_running = true;
            input_thread = std::thread(alsa_input_thread_function, this);
        }
        virtual ~inputContext_ALSA() {
            if (keep_running) {
                keep_running = false;
                if (input_thread.joinable()) input_thread.join();
            }
            if (handle) {
                if (port_id >= 0) snd_seq_delete_simple_port(handle, port_id);
                snd_seq_close(handle);
                handle = nullptr;
            }
        }
        inline virtual std::string getMidiErrorMessage() override {
            if (last_alsa_error == 0) return "No error.";
            return snd_strerror(last_alsa_error); 
        }
        inline virtual void getMidiInDevices(midiInputDevices_t& devices) override {
            devices.clear();
            snd_seq_t* seq_handle = nullptr;
            if (last_alsa_error = snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_INPUT, 0) < 0)
                return;

            snd_seq_client_info_t *cinfo;
            snd_seq_port_info_t *pinfo;
            snd_seq_client_info_alloca(&cinfo);
            snd_seq_port_info_alloca(&pinfo);

            snd_seq_client_info_set_client(cinfo, -1);
            uint32_t current_idx = 0;
            while (snd_seq_query_next_client(seq_handle, cinfo) >= 0) {
                int client_id = snd_seq_client_info_get_client(cinfo);
                snd_seq_port_info_set_client(pinfo, client_id);
                snd_seq_port_info_set_port(pinfo, -1);
                while (snd_seq_query_next_port(seq_handle, pinfo) >= 0) {
                    unsigned int caps = snd_seq_port_info_get_capability(pinfo);
                    unsigned int type = snd_seq_port_info_get_type(pinfo);
                    if ((caps & SND_SEQ_PORT_CAP_READ) && (caps & SND_SEQ_PORT_CAP_SUBS_READ) &&
                        (type & SND_SEQ_PORT_TYPE_MIDI_GENERIC)) {
                        inputDevice_t device;
                        device.index = current_idx++;
                        device.id = std::to_string(snd_seq_port_info_get_client(pinfo)) + ":" +
                                    std::to_string(snd_seq_port_info_get_port(pinfo));
                        device.name = std::string(snd_seq_client_info_get_name(cinfo)) + ": " +
                                      std::string(snd_seq_port_info_get_name(pinfo)) + " " + device.id;
                        devices.push_back(device);
                    }
                }
            }
            snd_seq_close(seq_handle);
            last_alsa_error = 0;
        }
    };
    static void alsa_input_thread_function(inputContext_ALSA* ctx) {
        if (!ctx || !ctx->handle) return;
        snd_seq_event_t *ev = nullptr;
        while (ctx->keep_running) {
            int ret = snd_seq_event_input(ctx->handle, &ev);
            if (!ctx->keep_running) break;
            if (ret == -EAGAIN){
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue; // continue polling
            } 
            if (ret < 0) {
                ctx->last_alsa_error = ret;
                break; 
            }
            if (ev) {
                ctx->process_event(ev);
            }
        }
    }
    struct outputContext_ALSA : public outputContext {
    private:
        snd_seq_t* handle = nullptr;
        int device_index = 0;
        int port_id = -1;
        std::string device_id;
        snd_seq_addr_t addr;
        int last_alsa_error = 0;
    public:
        inline virtual const uint32_t getIndex() const override { return device_index; }
        inline virtual const bool getStatus() const override { return handle != nullptr && last_alsa_error == 0; }
        outputContext_ALSA() = default;
        outputContext_ALSA(outputDevice_t const& device) : device_id(device.id), device_index(device.index) {
            if (last_alsa_error = snd_seq_open(&handle, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) { /* error */ handle = nullptr; return; }
            if (last_alsa_error = snd_seq_set_client_name(handle, "Keyboard") < 0) { last_alsa_error = -1; }
            port_id = snd_seq_create_simple_port(handle, "Output Port", SND_SEQ_PORT_CAP_READ, SND_SEQ_PORT_TYPE_MIDI_GENERIC);
            if (port_id < 0) { last_alsa_error = -1; }
            if (last_alsa_error = snd_seq_parse_address(handle, &addr, device_id.c_str()) < 0) return;
            if (last_alsa_error = snd_seq_connect_to(handle, port_id, addr.client, addr.port) < 0)
                return;
        }

        virtual ~outputContext_ALSA() {
            if (handle) {
                if (port_id >= 0) snd_seq_delete_simple_port(handle, port_id);
                snd_seq_close(handle);
                handle = nullptr;
            }
        }
        inline virtual void sendMessage(message_t const& message) override {
            if (!getStatus()) return;
            auto packet = midi1_packet(message); 
            snd_seq_event_t ev{};
            snd_seq_ev_set_direct(&ev);
            snd_seq_ev_set_source(&ev, port_id);
            snd_seq_ev_set_dest(&ev, SND_SEQ_ADDRESS_SUBSCRIBERS, port_id);
            uint8_t command = packet.status & 0xF0;
            uint8_t channel = packet.status & 0x0F;
            if (command == 0x90) {
                if (packet.hi == 0) {
                     snd_seq_ev_set_noteoff(&ev, channel, packet.lo, packet.hi);
                } else {
                     snd_seq_ev_set_noteon(&ev, channel, packet.lo, packet.hi);
                }
            }
            else if (command == 0x80) {
                snd_seq_ev_set_noteoff(&ev, channel, packet.lo, packet.hi);
            }
            else if (command == 0xA0) {
                snd_seq_ev_set_keypress(&ev, channel, packet.lo, packet.hi);
            }
            else if (command == 0xB0) {
                snd_seq_ev_set_controller(&ev, channel, packet.lo, packet.hi);
            }
            else if (command == 0xC0) {
                snd_seq_ev_set_pgmchange(&ev, channel, packet.lo);
            }
            else if (command == 0xD0) {
                snd_seq_ev_set_chanpress(&ev, channel, packet.lo);
            }
            else if (command == 0xE0) {
                int pitch = ( (int)packet.hi << 7 | (int)packet.lo ) - 8192;
                snd_seq_ev_set_pitchbend(&ev, channel, pitch);
            }
            else {
                return;
            }
            snd_seq_event_output_direct(handle, &ev);
        }
        inline virtual std::string getMidiErrorMessage() override {
            if (last_alsa_error == 0) return "No error.";
            return snd_strerror(last_alsa_error);
        }
        inline virtual void getMidiOutDevices(midiOutputDevices_t& devices) override {
            devices.clear();
            snd_seq_t* seq_handle = nullptr;
            if (last_alsa_error = snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0)
                return;

            snd_seq_client_info_t *cinfo;
            snd_seq_port_info_t *pinfo;
            snd_seq_client_info_alloca(&cinfo);
            snd_seq_port_info_alloca(&pinfo);

            snd_seq_client_info_set_client(cinfo, -1);
            uint32_t current_idx = 0;
            while (snd_seq_query_next_client(seq_handle, cinfo) >= 0) {
                int client_id = snd_seq_client_info_get_client(cinfo);
                snd_seq_port_info_set_client(pinfo, client_id);
                snd_seq_port_info_set_port(pinfo, -1);
                while (snd_seq_query_next_port(seq_handle, pinfo) >= 0) {
                    unsigned int caps = snd_seq_port_info_get_capability(pinfo);
                    unsigned int type = snd_seq_port_info_get_type(pinfo);

                    
                    if ((caps & SND_SEQ_PORT_CAP_WRITE) && (caps & SND_SEQ_PORT_CAP_SUBS_WRITE) &&
                        (type & SND_SEQ_PORT_TYPE_MIDI_GENERIC)) {
                        outputDevice_t device;
                        device.index = current_idx++;
                        device.id = std::to_string(snd_seq_port_info_get_client(pinfo)) + ":" +
                                    std::to_string(snd_seq_port_info_get_port(pinfo));
                        device.name = std::string(snd_seq_client_info_get_name(cinfo)) + ": " +
                                      std::string(snd_seq_port_info_get_name(pinfo)) + " " + device.id;
                        devices.push_back(device);
                    }
                }
            }
            snd_seq_close(seq_handle);
            last_alsa_error = 0;
        }
    };
} 
#endif