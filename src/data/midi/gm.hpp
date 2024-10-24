namespace midi {
	// General MIDI Program names
	namespace gm {
		typedef std::pair<int, const char*> gm_name_t;
	}
	namespace gm {
		EXTERN_DATA_ARRAY(const char*, programs);
		EXTERN_DATA_ARRAY(gm_name_t, drum_kits);
		EXTERN_DATA_ARRAY(gm_name_t, controls);
	}
}