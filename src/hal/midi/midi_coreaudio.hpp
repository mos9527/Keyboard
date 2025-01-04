#ifndef HAL_MIDI_COREAUDIO_IMPL
#define HAL_MIDI_COREAUDIO_IMPL
#include "midi.hpp"
namespace midi {

    struct inputContext_CoreAudio : public inputContext {
    private:
        MIDIClientRef client;
        MIDIPortRef inputPort;
        uint32_t index = 0;
        std::mutex messageMutex;
        std::condition_variable messageCV;

        static void MidiInProc(const MIDIPacketList* packetList, void* readProcRefCon, void* srcConnRefCon) {
            auto ctx = static_cast<inputContext_CoreAudio*>(readProcRefCon);
            std::unique_lock<std::mutex> lock(ctx->messageMutex);
            for (unsigned int i = 0; i < packetList->numPackets; ++i) {
                const MIDIPacket& packet = packetList->packet[i];
                if (packet.length >= 3) {
                    uint8_t status = packet.data[0];
                    uint8_t lo = packet.data[1];
                    uint8_t hi = packet.data[2];
                    auto message = midi1_packet(status, lo, hi);
                    ctx->messages.push(message);
                }
            }
            ctx->messageCV.notify_one();
        }

    public:
        inline virtual const uint32_t getIndex() const { return index; }
        inline virtual const bool getStatus() const { return inputPort != 0u; }
        inline inputContext_CoreAudio() : client(0), inputPort(0) {}
        inline inputContext_CoreAudio(inputDevice_t const& device) : index(device.index) {
            MIDIClientCreate(CFSTR("Keyboard CoreAudio Client"), nullptr, nullptr, &client);
            MIDIInputPortCreate(client, CFSTR("Input"), MidiInProc, this, &inputPort);
            MIDIEndpointRef endpoint = MIDIGetSource(device.index);
            MIDIPortConnectSource(inputPort, endpoint, nullptr);
        }
        inline ~inputContext_CoreAudio() {
            if (inputPort) MIDIPortDispose(inputPort);
            if (client) MIDIClientDispose(client);
        }
        inline virtual void getMidiInDevices(midiInputDevices_t& result) {
            result.clear();
            ItemCount count = MIDIGetNumberOfSources();
            for (ItemCount i = 0; i < count; ++i) {
                MIDIEndpointRef endpoint = MIDIGetSource(i);
                CFStringRef name;
                MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName, &name);
                char nameBuffer[128];
                CFStringGetCString(name, nameBuffer, sizeof(nameBuffer), kCFStringEncodingUTF8);
                result.push_back(inputDevice_t{
                    .index = static_cast<uint32_t>(i),
                    .name = std::string(nameBuffer),
                    .id = std::to_string(i)
                });
                CFRelease(name);
            }
        }
        inline virtual std::string getMidiErrorMessage() { return "Unknown Error (CoreAudio)"; }
    };

    struct outputContext_CoreAudio : public outputContext {
    private:
        MIDIClientRef client;
        MIDIPortRef outputPort;
        MIDIEndpointRef endpoint;
        uint32_t index = 0;

    public:
        inline virtual const uint32_t getIndex() const { return index; }
        inline virtual const bool getStatus() const { return outputPort != 0u; }
        inline outputContext_CoreAudio() : client(0), outputPort(0), endpoint(0) {}
        inline outputContext_CoreAudio(outputDevice_t const& device) : index(device.index) {
            MIDIClientCreate(CFSTR("Keyboard CoreAudio Client"), nullptr, nullptr, &client);
            MIDIOutputPortCreate(client, CFSTR("Output"), &outputPort);
            endpoint = MIDIGetDestination(device.index);
        }
        inline ~outputContext_CoreAudio() {
            if (outputPort) MIDIPortDispose(outputPort);
            if (client) MIDIClientDispose(client);
        }
        inline virtual void sendMessage(message_t const& message) {
            if (!getStatus()) return;
            Byte buffer[1024];
            MIDIPacketList* packetList = reinterpret_cast<MIDIPacketList*>(buffer);
            MIDIPacket* packet = MIDIPacketListInit(packetList);
            packet = MIDIPacketListAdd(packetList, sizeof(buffer), packet, 0, 3, reinterpret_cast<const Byte*>(&message));
            MIDISend(outputPort, endpoint, packetList);
        }
        inline virtual std::string getMidiErrorMessage() { return "Unknown Error (CoreAudio)"; }
        inline virtual void getMidiOutDevices(midiOutputDevices_t& result) {
            result.clear();
            ItemCount count = MIDIGetNumberOfDestinations();
            for (ItemCount i = 0; i < count; ++i) {
                MIDIEndpointRef endpoint = MIDIGetDestination(i);
                CFStringRef name;
                MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName, &name);
                char nameBuffer[128];
                CFStringGetCString(name, nameBuffer, sizeof(nameBuffer), kCFStringEncodingUTF8);
                result.push_back(outputDevice_t{
                    .index = static_cast<uint32_t>(i),
                    .name = std::string(nameBuffer),
                    .id = std::to_string(i)
                });
                CFRelease(name);
            }
        }
    };
}
#endif