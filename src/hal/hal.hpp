#pragma once
// Monolithic header for Hardware Abstraction Layer (HAL)

// Windows
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <mmeapi.h>
#pragma comment(lib, "winmm.lib")
#ifdef WINRT
#pragma comment(lib, "windowsapp")
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Devices.Midi.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/Windows.Storage.Streams.h"
#pragma message("WinRT MIDI API is enabled")
#ifdef MIDI2
#include "winrt/Microsoft.Windows.Devices.Midi2.h"
#include "winrt/Microsoft.Windows.Devices.Midi2.Diagnostics.h"
#include "winrt/Microsoft.Windows.Devices.Midi2.Messages.h"
#include "winrt/Microsoft.Windows.Devices.Midi2.Initialization.h"
#pragma message("MIDI2 is enabled")
#endif // !MIDI2
#endif // !WINRT
#endif // !_WIN32
