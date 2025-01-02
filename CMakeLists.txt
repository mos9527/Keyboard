# CMakeList.txt : CMake project for keyboard, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Keyboard")
set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable (Keyboard ${SOURCES})

if (WIN32)
	set(MINGW true)
	message(STATUS "Building for Windows")
	# UTF8
	target_compile_options(Keyboard PRIVATE "/utf-8")
	# PDCurses
	include_directories("contrib/PDCurses")
	file(GLOB PDCURSES_SOURCES "contrib/PDCurses/wincon/*.c" "contrib/PDCurses/pdcurses/*.c")
	add_library(PDCurses STATIC ${PDCURSES_SOURCES})
	target_link_libraries(Keyboard PRIVATE PDCurses)
	target_include_directories(Keyboard PRIVATE PDCurses)
	# WinRT (Windows Runtime) support
	find_program(NUGET_EXE NAMES nuget)
	# Snippet inspired by https://github.com/woodemi/quick_blue/pull/10/files
	if(NOT NUGET_EXE)
		message("NuGet not found. WinRT support disabled.")
	else()
	function(nuget_install package)
		execute_process(COMMAND ${NUGET_EXE} install ${package} -Prerelease -ExcludeVersion -OutputDirectory ${CMAKE_BINARY_DIR}/packages RESULT_VARIABLE EXIT_CODE)	
		set(PROP_FILE ${CMAKE_BINARY_DIR}/packages/${package}/build/native/${package}.props)
		if (EXISTS ${PROP_FILE})
			get_property(PROPS TARGET Keyboard PROPERTY VS_PROJECT_IMPORT)			
			set_property(TARGET Keyboard PROPERTY VS_PROJECT_IMPORT ${PROPS} ${PROP_FILE})
			get_target_property(PROPS Keyboard VS_PROJECT_IMPORT)			
			message("Current MSBuild imports ${PROPS}")
			message("Restored ${package}")
			set(EXIT_CODE 0 PARENT_SCOPE)
		else()
			message(WARNING "Failed to restore ${package}") 
			set(EXIT_CODE 1 PARENT_SCOPE)
		endif()
	endfunction()
	nuget_install("Microsoft.Windows.CppWinRT")
	if (EXIT_CODE EQUAL 0)
		add_compile_definitions(WINRT)
		message("WinRT support enabled.")
		# nuget_install("Microsoft.Windows.Devices.Midi2")
		# if (EXIT_CODE EQUAL 0)
		#	add_compile_definitions(MIDI2)
		#	message("MIDI2 support enabled.")
		# endif()
	endif()
endif()
endif()

if (APPLE)
	message(STATUS "Building for macOS")
	# CoreAudio
	target_link_libraries(Keyboard PRIVATE "-framework Foundation -framework CoreMidi")
endif()

# ImTui
target_include_directories(Keyboard PRIVATE "contrib/ImTui/include")
target_include_directories(Keyboard PRIVATE "contrib/ImTui/third-party/imgui")
set(IMTUI_SUPPORT_NCURSES ON)
add_subdirectory("contrib/ImTui")

target_link_libraries(Keyboard PRIVATE imtui imtui-ncurses imgui-for-imtui)
target_precompile_headers(Keyboard PUBLIC "src/pch.hpp")