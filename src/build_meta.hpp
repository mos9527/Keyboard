#include <string>
#ifndef BUILD_META
#define BUILD_META
#define PROJECT_NAME "Keyboard"
#define PROJECT_VERSION "0.0.1"
namespace build_meta {
	// XXX: Make this consteval when MSVC gets nicer
	static std::string get_feature_flag_string() {
		static std::string s;
		if (!s.length()) {
			s += "Project: " PROJECT_NAME "\n";
			s += "Version: " PROJECT_VERSION "\n";
			s += "Build: " __DATE__ " " __TIME__ "\n";		
			s += "Platform ";
#ifdef _WIN32
			s += "Windows";
#endif // _WIN32
			s += "\n";
			s += "Features: ";
#ifdef _WIN32
			s += "WINMM ";
#endif
#ifdef WINRT
			s += "WINRT ";
#endif
#ifdef MIDI2
			s += "MIDI2 ";
#endif // MIDI2
		}
		return s;
	}
}
#endif // !BUILD_META
