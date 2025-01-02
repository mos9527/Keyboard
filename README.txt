keyboard
---
Simple TUI chords/notes visualization tool.
* Windows/macOS support
* WinMM/WinRT/MIDI2/CoreAudio API support (BLE MIDI devices are supported w/ WinRT/MIDI2, Multi-client is supported w/ MIDI2)
* detects chords/intervals and display their name(s).
* supports (partial) MIDI passthrough to another output device

todo
---
* add option to display selected chord
* properly draw the staff

build instructions
---
* the app can be build trivally like any other CMake project. however:
	* you'll need Visual Studio (MSBuild) to support WinRT/MIDI2 APIs. otherwise you'd be limited to the legacy WinMM one	
* in this case, open the project in Visual Studio (2022+) and build it from there.
* additionally, you'll need to have Microsoft.Windows.Devices.Midi2 package installed to support MIDI2 APIs.
	* this implies you'll need NuGet installed and available on your PATH as well
		* you can get it from https://github.com/microsoft/MIDI/releases/download/dev-preview-6/Microsoft.Windows.Devices.Midi2.1.0.24194.2233-preview.6.nupkg
		* then, you can add it to your local NuGet package storage via `nuget add -Source <storage path> Microsoft.Windows.Devices.Midi2.1.0.24194.2233-preview.6.nupkg`
references
---
* https://github.com/juce-framework/JUCE/commit/9a38505dad7a5655edae320993f1926ae3979068
* https://github.com/stammen/winrtmidi/
* https://github.com/Microsoft/cppwinrt/issues/317
* https://kennykerrca.wordpress.com/2018/03/01/cppwinrt-understanding-async/
* https://kennykerrca.wordpress.com/2018/03/09/cppwinrt-producing-async-objects/
* https://devblogs.microsoft.com/oldnewthing/20210809-00/?p=105539
* https://www.recordingblogs.com/wiki/midi-system-exclusive-message
* https://www.recordingblogs.com/wiki/music-theory-index
* https://www.recordingblogs.com/wiki/status-byte-of-a-midi-message
* https://www.recordingblogs.com/wiki/midi-voice-messages
* https://www.recordingblogs.com/wiki/midi-controller-message
* https://midi.org/general-midi-2
* General MIDI 2 (February 6, 2007 Version 1.2a)
* https://microsoft.github.io/MIDI/
* https://microsoft.github.io/MIDI/sdk-winrt-messages/README.html
* https://github.com/woodemi/quick_blue/pull/10/files
