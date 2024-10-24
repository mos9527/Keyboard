#pragma once
namespace midi {
	namespace chords {
		using namespace std;
		typedef array<uint8_t, 12> key_t;
		struct chord_t {
			enum fmt_type {
				BASS,
				BASS_BASS,
				BASS_ROOT_BASS,
				ROOT,
				ROOT_BASS,
				NONE
			};
			const fmt_type fmt = NONE;
			const char* fmt_str = nullptr;
			const uint8_t nth_root = 0;
			inline int format_to_string(char* str, const char* bass_key, const char* nth_key) const {
				switch (fmt) {
				case BASS: return sprintf(str, fmt_str, bass_key);
				case BASS_BASS: return sprintf(str, fmt_str, bass_key, bass_key);
				case BASS_ROOT_BASS: return sprintf(str, fmt_str, bass_key, nth_key, bass_key);
				case ROOT: return sprintf(str, fmt_str, nth_key);
				case ROOT_BASS: return sprintf(str, fmt_str, nth_key, bass_key);
				case NONE:
				default:
					return sprintf(str, "%s", fmt_str);
				}
			}
		};
		class chord_arr_t {
			using container_type = array<chord_t, 8>;
			const container_type chords;
			const size_t num_chords;
		public:
			template<typename... T> constexpr explicit chord_arr_t(T... args) : chords{ args... }, num_chords(sizeof...(args)) {}
			inline container_type::const_iterator begin() const { return chords.begin(); }
			inline container_type::const_iterator end() const { return chords.begin() + num_chords; }
			inline size_t size() const { return num_chords; }
		};
		/****/
		typedef pair<key_t, chord_arr_t> chord_item_t;
		typedef array<uint8_t, 256> midi_key_states_t;
		typedef fixed_matrix<char, 256, 256> chord_names_t;
	}
	namespace chords {
		EXTERN_DATA_ARRAY(const char*, key_table_flat);
		EXTERN_DATA_ARRAY(const char*, key_table_sharp);
		EXTERN_DATA_ARRAY(const char*, interval_table);
		EXTERN_DATA_ARRAY(const chord_item_t, chord_table);
		EXTERN_DATA_ARRAY(const chord_item_t, scale_table);
	}
	namespace chords {
		const chord_item_t* find(key_t const& key, const chord_item_t* data, size_t size);
		const int format(midi_key_states_t const& state, chord_names_t& lines, const char** key_table);
	}

}