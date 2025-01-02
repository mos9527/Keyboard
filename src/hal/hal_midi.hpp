#ifndef HAL_MIDI
// MIDI
#include "midi/midi.hpp"

#ifdef __APPLE__
#include "midi/midi_coreaudio.hpp"
#endif // !__APPLE

#ifdef _WIN32
#include "midi/midi_winmm.hpp"
#ifdef WINRT
#include "midi/midi_winrt.hpp"
#ifdef MIDI2
#include "midi/midi_winmidi2.hpp"
#endif // !MIDI2
#endif // !WINRT
#endif // !_WIN32
#endif // !HAL_MIDI
