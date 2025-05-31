#include "pch.hpp"

#include "imtui/imtui.h"
#include "imtui/imtui-impl-ncurses.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui.h"

#include "hal/hal_midi.hpp"
#include "data/midi/chords.hpp"
#include "data/midi/gm.hpp"

#include "build_meta.hpp"
#define CONFIG_FILENAME "config"
struct {
	int inputBackend = 0;
	int inputChannel = 0;	
	int inputChannelRemap = -1;
	int inputDeviceIndex = 0;
	int outputBackend = 0;
	int outputDeviceIndex = 0;
	int outputChannel = 0;
	int keyboardKeymap[256]{};
	bool keyboardDisplayFlatOrSharp = 0; // 0 flat 1 sharp
	int keyboardOffset = 64;
	void save() {
		FILE* file = fopen(CONFIG_FILENAME, "wb");
		ASSERT(file, "Failed to open file for writing");
		fwrite(this, sizeof(*this), 1, file);
		fclose(file);
	};
	void load() {
		FILE* file = fopen(CONFIG_FILENAME, "rb");
		if (file) {
			fread(this, sizeof(*this), 1, file);
			fclose(file);
		}
	};
	void reset() {
		memset(this, 0, sizeof(*this));
	};
} g_config;
/****/
typedef std::unique_ptr<midi::inputContext> midiInContext_t;
typedef std::unique_ptr<midi::outputContext> midiOutContext_t;
const char* MIDI_BACKENDS[] = {
#ifdef HAL_MIDI_WINMM_IMPL
	"WinMM (Legacy)",
#endif
#ifdef HAL_MIDI_WINRT_IMPL
	"WinRT",
#ifdef HAL_MIDI_WINMIDI2_IMPL
	"MIDI2",
#endif
#endif
#ifdef HAL_MIDI_COREAUDIO_IMPL
	"CoreAudio",
#endif
#ifdef HAL_MIDI_ALSA_IMPL
	"ALSA",
#endif
};
template<typename... T> midiInContext_t make_midi_input_context(T const&... args) {
	switch (g_config.inputBackend)
	{
#ifdef HAL_MIDI_WINRT_IMPL
#ifdef HAL_MIDI_WINMIDI2_IMPL
	case 2:
		return std::make_unique<midi::inputContext_WinMIDI2>(args...);
#endif
	case 1:
		return std::make_unique<midi::inputContext_WinRT>(args...);
#endif
	case 0:default:
#ifdef HAL_MIDI_WINMM_IMPL
		return std::make_unique<midi::inputContext_WinMM>(args...);
#endif
#ifdef HAL_MIDI_COREAUDIO_IMPL
		return std::make_unique<midi::inputContext_CoreAudio>(args...);
#endif
#ifdef HAL_MIDI_ALSA_IMPL
		return std::make_unique<midi::inputContext_ALSA>(args...);
#endif

	}
}
template<typename... T> midiOutContext_t make_midi_output_context(T const&... args) {
	switch (g_config.outputBackend)
	{
#ifdef HAL_MIDI_WINRT_IMPL
#ifdef HAL_MIDI_WINMIDI2_IMPL
	case 2:
		return std::make_unique<midi::outputContext_WinMIDI2>(args...);
#endif
	case 1:
		return std::make_unique<midi::outputContext_WinRT>(args...);
#endif
	case 0:default:
#ifdef HAL_MIDI_WINMM_IMPL
		return std::make_unique<midi::outputContext_WinMM>(args...);
#endif
#ifdef HAL_MIDI_COREAUDIO_IMPL
		return std::make_unique<midi::outputContext_CoreAudio>(args...);
#endif
#ifdef HAL_MIDI_ALSA_IMPL
		return std::make_unique<midi::outputContext_ALSA>(args...);
#endif
	}
}
/****/
midiInContext_t g_midiInContext;
midiOutContext_t g_midiOutContext;
midi::midiInputDevices_t g_midiInDevices;
midi::midiOutputDevices_t g_midiOutDevices;
struct {
	bool muted = false, solo = false, hold = false;
	bool pedalAsHold = false;
	int program;
	midi::chords::midi_key_states_t keys;
	struct {
		int pitchBend = 0x2000;
		uint8_t cc[128]{};
	} controls;
} g_midiChannelStates[midi::MAX_CHANNEL_COUNT];
const uint8_t ACTIVE_INPUT_FRAMES = 3;
std::array<int, midi::MAX_CHANNEL_COUNT> g_activeInputs;
/****/
fixed_matrix<char, 256, 256> g_chordNames;
/****/
void setup() {
	g_midiInContext = make_midi_input_context();
	g_midiInContext->getMidiInDevices(g_midiInDevices);
	if (g_midiInDevices.size())
		g_midiInContext = make_midi_input_context(g_midiInDevices[std::min(g_midiInDevices.size() - 1, (size_t)g_config.inputDeviceIndex)]);

	g_midiOutContext = make_midi_output_context();
	g_midiOutContext->getMidiOutDevices(g_midiOutDevices);
	if (g_midiOutDevices.size())
		g_midiOutContext = make_midi_output_context(g_midiOutDevices[std::min(g_midiOutDevices.size() - 1, (size_t)g_config.outputDeviceIndex)]);
	if (g_midiInContext->getStatus())
		g_midiOutContext->sendMessage(midi::programChangeMessage{ (uint8_t)g_config.outputChannel, (uint8_t)g_midiChannelStates[g_config.outputChannel].program });
}
void poll_input() {
	using namespace midi;
	if (g_midiInContext) {
		if (g_midiInContext->getStatus()) {
			while (auto pool = g_midiInContext->pollMessage()) {
				auto& message = pool.value();
				bool passthrough = true;
				std::visit(visitor{
					[&](noteOnMessage& msg) {
						if (!g_midiChannelStates[msg.channel].hold)
							g_midiChannelStates[msg.channel].keys[msg.note] = msg.velocity;
						else {
							if (msg.velocity == 0) passthrough = false;
							else g_midiChannelStates[msg.channel].keys[msg.note] = msg.velocity;
						}
						if (g_midiChannelStates[msg.channel].muted)
							passthrough = false;
					},
					[&](noteOffMessage& msg) {
						if (!g_midiChannelStates[msg.channel].hold)
							g_midiChannelStates[msg.channel].keys[msg.note] = 0;
						else
							passthrough = false;
					},
					[&](pitchBendMessage& msg) {
						g_midiChannelStates[msg.channel].controls.pitchBend = msg.level;
					},
					[&](controlChangeMessage& msg) {
						g_midiChannelStates[msg.channel].controls.cc[msg.controller] = msg.value;						
					},
					[&](programChangeMessage& msg) {
						g_midiChannelStates[msg.channel].program = msg.program;
					}
					}, message);
				if (g_midiOutContext) {
					std::visit(visitor{
						[&](auto& msg) {
							constexpr bool channel_type = requires() { msg.channel; };
							if constexpr (channel_type) {
								g_activeInputs[msg.channel] = ACTIVE_INPUT_FRAMES;
								if (msg.channel == g_config.inputChannel && g_config.inputChannelRemap >= 0)
									msg.channel = g_config.inputChannelRemap;
							}
						},
						}, message);
					if (passthrough) g_midiOutContext->sendMessage(message);
				}
			}
		}
	}
}
void draw() {
	auto draw_button_array = [&](int& value, const auto& names, const int id = 0, const int* states = nullptr) -> bool {
		bool dirty = false;
		for (int i = 0; i < extent_of(names); i++) {
			bool active = value == i;
			int styles = 0;
			if (active)
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)), styles++;
			if (states && states[i])
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered)), styles++;
			ImGui::PushID(id + i);
			if (ImGui::Button(names[i], ImVec2(4, 0))) value = i, dirty = true;
			ImGui::PopID();
			ImGui::PopStyleColor(styles);
			ImGui::SameLine();
		}
		return dirty;
		};
	auto draw_twiddle_button = [&](int& value, const int r_min, const int r_max, const int id = 0) {
		bool dirty = false;
		ImGui::PushID(id);
		if (ImGui::Button("<", ImVec2(4, 0))) value = std::max(r_min, value - 1), dirty = true;
		ImGui::PopID();
		ImGui::SameLine();
		ImGui::PushID(id + 1);
		if (ImGui::Button(">", ImVec2(4, 0))) value = std::min(r_max, value + 1), dirty = true;
		ImGui::PopID();
		return dirty;
		};
	auto const& key_table = g_config.keyboardDisplayFlatOrSharp ? midi::chords::key_table_sharp : midi::chords::key_table_flat;
	ImGui::SetNextWindowPos({ 0,0 });
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
	ImGui::Begin("keyboard");
	if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_None)) {
		ImGui::Text(build_meta::get_feature_flag_string().c_str());
		if (ImGui::Button("Save")) g_config.save();
		ImGui::SameLine();
		if (ImGui::Button("Load")) g_config.load(), setup();
		ImGui::SameLine();
		if (ImGui::Button("Reset")) g_config.reset(), setup();
	}

	if (ImGui::CollapsingHeader("Hardware", ImGuiTreeNodeFlags_None)) {
		ImGui::Text("System");
		auto draw_backend_selector = [&](auto& value, const char* title) {
			if (ImGui::BeginCombo(title, MIDI_BACKENDS[value])) {
				for (int i = 0; i < extent_of(MIDI_BACKENDS); i++) {
					bool selected = value == i;
					if (ImGui::Selectable(MIDI_BACKENDS[i], &selected)) {
						value = i;
						setup();
					}
				}
				ImGui::EndCombo();
			}
		};
		draw_backend_selector(g_config.inputBackend, "Input Backend");
		draw_backend_selector(g_config.outputBackend, "Output Backend");
		const char* channel_names[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10","11","12","13","14","15","16" };
		ImGui::Text("Input");
		if (!g_midiInContext->getStatus()) {
			static std::string errorMessage = g_midiInContext->getMidiErrorMessage();
			ImGui::TextColored(ImColor(255, 0, 0), "ERROR: %s", errorMessage.c_str());
		}
		if (ImGui::BeginCombo(
			"Input Device",
			(g_midiInDevices.size() && g_midiInContext ? g_midiInDevices[g_midiInContext->getIndex()].name.c_str() : "Select Device"),
			ImGuiComboFlags_PopupAlignLeft
		)) {
			for (auto& [index, name, id] : g_midiInDevices) {
				bool selected = g_midiInContext && g_midiInContext->getIndex() == index;
				if (ImGui::Selectable(name.c_str(), &selected))
					g_midiInContext = make_midi_input_context(g_midiInDevices[index]), g_config.inputDeviceIndex = index;
			}
			ImGui::EndCombo();
		}
		auto width = ImGui::CalcItemWidth() / 3.0f;
		ImGui::ProgressBar(g_midiChannelStates[g_config.inputChannel].controls.pitchBend / 8192.0f / 2.0f, ImVec2(width, 1), "PITCH");
		ImGui::SameLine();
		ImGui::ProgressBar(g_midiChannelStates[g_config.inputChannel].controls.cc[1] / 127.0f, ImVec2(width, 1), "MOD");
		ImGui::SameLine();
		ImGui::ProgressBar(g_midiChannelStates[g_config.inputChannel].controls.cc[64] / 127.0f, ImVec2(width, 1), "SUSTAIN");
		draw_button_array(g_config.inputChannel, channel_names, 0, g_activeInputs.data());
		ImGui::Text("Input Channel");
		ImGui::SliderInt("Remap Channel", &g_config.inputChannelRemap, -1, 15);
		ImGui::Text("Output");
		if (!g_midiOutContext->getStatus()) {
			static std::string errorMessage = g_midiOutContext->getMidiErrorMessage();
			ImGui::TextColored(ImColor(255, 0, 0), "ERROR: %s", errorMessage.c_str());
		}
		if (ImGui::BeginCombo(
			"Output Device",
			(g_midiOutDevices.size() && g_midiOutContext ? g_midiOutDevices[g_midiOutContext->getIndex()].name.c_str() : "Select Device"),
			ImGuiComboFlags_PopupAlignLeft
		)) {
			for (auto& [index, name, id] : g_midiOutDevices) {
				bool selected = g_midiOutContext && g_midiOutContext->getIndex() == index;
				if (ImGui::Selectable(name.c_str(), &selected))
					g_midiOutContext = make_midi_output_context(g_midiOutDevices[index]), g_config.outputDeviceIndex = index;
			}
			ImGui::EndCombo();
		}
		if (g_midiOutContext) {
			bool channel_changed = draw_button_array(g_config.outputChannel, channel_names, midi::MAX_CHANNEL_COUNT);
			ImGui::Text("Output Channel");
			{
				auto& program = g_midiChannelStates[g_config.outputChannel].program;
				bool program_changed = false;				
				if (ImGui::BeginCombo("Program", midi::gm::programs[program])) {
					for (int i = 0; i < midi::gm::programs_size; i++) {
						bool selected = program == i;
						if (ImGui::Selectable(midi::gm::programs[i], &selected)) {
							program = i, program_changed = true;
						}
					}
					ImGui::EndCombo();
				}
				ImGui::SameLine();
				program_changed |= draw_twiddle_button(program, 0, midi::gm::programs_size, 32);
				if (program_changed)
					g_midiOutContext->sendMessage(midi::programChangeMessage{ (uint8_t)g_config.outputChannel, (uint8_t)program });
			}
		}
		static bool sync_input_output_chn_select = true;
		ImGui::Checkbox("Sync Input/Output Channel Selection", &sync_input_output_chn_select);
		ImGui::SameLine();
		if (sync_input_output_chn_select) g_config.outputChannel = g_config.inputChannel;
		if (ImGui::Button("Refresh")) setup();
	}
	if (ImGui::CollapsingHeader("Viewer", ImGuiTreeNodeFlags_DefaultOpen)) {
		static int root_oct = 4;
		bool dirty = false;
		ImGui::Text("Octave: %d", root_oct); ImGui::SameLine();
		dirty |= draw_twiddle_button(root_oct, 0, 8);		
		static char name_pattern[256]{ "Ionian" };
		dirty |= ImGui::InputText("Pattern", name_pattern, sizeof(name_pattern));
		static std::vector<std::tuple<std::string, const midi::chords::key_t*, int>> names;
		static bool init = false;
		static bool preview = false;
		if (dirty || !init) {
			init = true;
			if (preview)  g_midiChannelStates[g_config.inputChannel].keys = {};
			char buf[256]{};
			names.clear();
			auto pb = [&](std::string str, auto ptr, int root_key) {
				if (str.find(name_pattern) == std::string::npos) return;
				names.push_back({ str, ptr, root_key });
				};
			for (int root_key = 0; root_key < 12; root_key++) {
				int root = root_key + root_oct * 12;
				for (int i = 0; i < midi::chords::scale_table_size; i++) {
					auto const& [keys, chords] = midi::chords::scale_table[i];
					for (auto& v : chords) {
						midi::chords::format_name(keys, v, (const char**)key_table, buf, root);
						pb(buf, &keys, root);
					}
				}
			}
			std::sort(names.begin(), names.end());
			int sz = names.size();
			for (int root_key = 0; root_key < 12; root_key++) {
				int root = root_key + root_oct * 12;
				for (int i = 0; i < midi::chords::chord_table_size; i++) {
					auto const& [keys, chords] = midi::chords::chord_table[i];
					for (auto& v : chords) {
						midi::chords::format_name(keys, v, (const char**)key_table, buf, root);
						pb(buf, &keys, root);
					}
				}
			}
			std::sort(names.begin() + sz, names.end());			
			g_midiChannelStates[g_config.inputChannel].keys = {};
		}
		static int currPreview = 0;
		if (currPreview >= names.size()) currPreview = 0;
		ImGui::SameLine();
		if (names.size()) {
			if (ImGui::BeginCombo("Names", std::get<0>(names[currPreview]).c_str())) {
				int idx = 0;
				for (auto& [name, ptr, root] : names) {
					if (ImGui::Selectable(name.c_str())) currPreview = idx;
					idx++;
				}
				ImGui::EndCombo();
			}
		}
		bool chkPreview = ImGui::Checkbox("Preview", &preview);
		if (preview) {
			if (names.size()) {
				auto& [name, ptr, root] = names[currPreview];
				ImGui::TextUnformatted(name.c_str());
				auto sta = midi::chords::to_key_states(*ptr, root);
				for (int i = 0; i < 256; i++)
					g_midiChannelStates[g_config.inputChannel].keys[i] =
					std::max(
						g_midiChannelStates[g_config.inputChannel].keys[i],
						sta[i]
					);
			}
		}		
		ImGui::SameLine();
		if (chkPreview) g_midiChannelStates[g_config.inputChannel].keys = {};
	}
	if (ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_DefaultOpen)) {
		const ImVec2 whiteKeySize(6, 8);
		const ImVec2 blackKeySize(4, 6);
		const ImVec4 blackKeyColor(0, 0, 0, 255);
		const ImVec4 whiteKeyColor(255, 255, 255, 255);
		const ImVec4 pressedKeyColor(0.4f, 0.4f, 0.8f, 1.0f);
		const ImVec4 pressedFullKeyColor(0.2f, 0.2f, 0.4f, 1.0f);
		const int numKeys = 88;
		const int startNote = 21;
		auto& offsetKey = g_config.keyboardOffset;
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 cpos = ImGui::GetCursorPos();
		static int activeKey = 0;
		auto draw_layer = [&](bool isBlack) {
			int offsetX = -offsetKey;
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::BeginGroup();
			for (int i = 0; i < numKeys; ++i)
			{
				const int nthWhiteKey[] = { 0,  -1 , 1, -1,  2, 3, -1,  4, -1,  5, -1,  6 };
				const int nthBlackKey[] = { -1,  0, -1,  1, -1, -1, 2,  -1, 3,  -1, 4,  -1 };
				int note = startNote + i;
				int octave = note / 12;
				int noteInOctave = note % 12;
				bool isBlackKey = nthBlackKey[noteInOctave] != -1;				
				if (!isBlack && isBlackKey) continue;
				if (isBlack && !isBlackKey) continue;
				ImVec4 keyColor = isBlackKey ? blackKeyColor : whiteKeyColor;
				ImVec4 labelColor = isBlackKey ? whiteKeyColor : blackKeyColor;
				if (g_midiChannelStates[g_config.inputChannel].keys[note] > 0)
				{
					float t = g_midiChannelStates[g_config.inputChannel].keys[note] / 127.0f;
					keyColor = ImLerp(pressedKeyColor, pressedFullKeyColor, t);
				}
				ImVec2 keySize = isBlackKey ? blackKeySize : whiteKeySize;
				ImGui::PushStyleColor(ImGuiCol_Button, keyColor);
				ImGui::PushStyleColor(ImGuiCol_Text, labelColor);
				ImGui::SetCursorPosX(pos.x + offsetX);
				ImGui::PushID(note);
				if (!isBlackKey) {
					int paddingX = ImGui::GetStyle().FramePadding.x;
					draw_list->AddRectFilled(ImVec2(paddingX + pos.x + offsetX, pos.y - blackKeySize.y), ImVec2(paddingX + pos.x + offsetX + keySize.x, pos.y), ImColor(keyColor));
				}
				std::string keyName = std::string{ key_table[noteInOctave] } + std::to_string(octave);
				if (ImGui::Button(keyName.c_str(), keySize)) {
					ImGui::PopID();
					activeKey = note;
				}
				else { ImGui::PopID(); }
				ImGui::SameLine();
				if (isBlackKey) {
					int nBlack = nthBlackKey[noteInOctave];
					if (nBlack != 1 && nBlack != 4) offsetX += whiteKeySize.x;
					else offsetX += whiteKeySize.x * 2;
				}
				else
					offsetX += whiteKeySize.x;
				ImGui::PopStyleColor(2);
			}
			ImGui::EndGroup();
			ImGui::PopStyleVar();
			};
		pos.y += blackKeySize.y;
		ImGui::SetCursorPos(pos);
		draw_layer(false);
		pos.y -= blackKeySize.y;
		pos.x += whiteKeySize.x / 2;
		ImGui::SetCursorPos(pos);
		draw_layer(true);
		ImGui::SetCursorPosX(0);
		ImGui::SetCursorPosY(cpos.y + 14);
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::SliderInt("##Start", &offsetKey, 0, whiteKeySize.x * 8 * 7 - ImGui::GetContentRegionAvailWidth());
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		static int frameActive = 0;
		{
			ImGui::Checkbox("Sharps", &g_config.keyboardDisplayFlatOrSharp);		
			auto& muted = g_midiChannelStates[g_config.outputChannel].muted;
			auto& solo = g_midiChannelStates[g_config.outputChannel].solo;
			auto& hold = g_midiChannelStates[g_config.outputChannel].hold;
			auto& pedalHold = g_midiChannelStates[g_config.inputChannel].pedalAsHold;
			ImGui::SameLine(); ImGui::Checkbox("Pedal As Hold", &pedalHold);
			auto release_all_keys = [&](int channel) {
				for (int i = 0; i < 128; i++) {
					if (g_midiChannelStates[channel].keys[i] > 0) {
						g_midiOutContext->sendMessage(midi::noteOffMessage{ (uint8_t)channel, (uint8_t)i, 0 });
						g_midiChannelStates[channel].keys[i] = 0;
					}
				}
				};
			auto set_channel_mute = [&](int channel, bool mute) {
				g_midiChannelStates[channel].muted = mute;
				if (mute) release_all_keys(channel);
				};
			ImGui::Checkbox("Mute", &muted); ImGui::SameLine();
			if (ImGui::Checkbox("Solo", &solo)) {
				if (!solo) for (int i = 0; i < midi::MAX_CHANNEL_COUNT; i++) set_channel_mute(i, false);
				else {
					for (int i = 0; i < midi::MAX_CHANNEL_COUNT; i++) set_channel_mute(i, true), g_midiChannelStates[i].solo = false;
					muted = false, solo = true;
				}
			}
			ImGui::SameLine();
			if (pedalHold) hold = g_midiChannelStates[g_config.outputChannel].controls.cc[64];
			ImGui::Checkbox("Hold", &hold);
			static bool hold_change = false;
			if (hold_change != hold) {
				hold_change = hold;
				if (!hold) release_all_keys(g_config.outputChannel);
			}
		}
	}
	if (ImGui::CollapsingHeader("Chords", ImGuiTreeNodeFlags_DefaultOpen)) {
		g_chordNames.resize(midi::chords::format(g_midiChannelStates[g_config.inputChannel].keys, g_chordNames, (const char**)key_table));
		for (auto& line : g_chordNames) {
			const char* data = line.data();
			ImGui::TextUnformatted(line.data());
		}
	}
	ImGui::End();
}
void refresh() {
	for (auto& frame : g_activeInputs) frame--, frame = std::max(0, frame);
}
void cleanup() {
	if (g_midiInContext) g_midiInContext.reset();
}
int main() {
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#endif
#ifdef HAL_MIDI_WINRT_IMPL
	winrt::init_apartment();
#ifdef HAL_MIDI_WINMIDI2_IMPL
	ASSERT(winrt::Microsoft::Windows::Devices::Midi2::Initialization::MidiServicesInitializer::EnsureServiceAvailable(), L"MIDI2 Services aren't running.");
	winrt::Microsoft::Windows::Devices::Midi2::Initialization::MidiServicesInitializer::InitializeSdkRuntime();
#endif
#endif

#ifndef NO_UI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto screen = ImTui_ImplNcurses_Init(true);
	ImTui_ImplText_Init();
	ImGui::GetStyle().ScrollbarSize = 1;
	ImGui::GetStyle().GrabMinSize = 1.0f;
	g_config.load();
	setup();
	while (true) {
		ImTui_ImplNcurses_NewFrame();
		ImTui_ImplText_NewFrame();
		ImGui::NewFrame();
		refresh();
		poll_input();
		draw();
		ImGui::Render();
		ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
		ImTui_ImplNcurses_DrawScreen();
	}
	cleanup();
	ImTui_ImplText_Shutdown();
	ImTui_ImplNcurses_Shutdown();
#else
	g_config.load();
	setup();
	while (true) {
		refresh();
		poll_input();
	}
	cleanup();
#endif // ENABLE_UI


	return 0;
	}
