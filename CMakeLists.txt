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

set(MINGW true)
include_directories("contrib/PDCurses")
add_subdirectory("contrib/ImTui")


file(
	GLOB_RECURSE SOURCES
	"src/*.cpp" 
	"contrib/PDCurses/wincon/*.c" "contrib/PDCurses/pdcurses/*.c" 
	"contrib/ImTui/src/imtui-impl-ncurses.cpp"
)
# Add source to this project's executable.
add_executable (Keyboard ${SOURCES})

# WinRT (Windows Runtime) support
add_compile_definitions(WINRT)
# MIDI2 support
add_compile_definitions(MIDI2)

target_precompile_headers(Keyboard PUBLIC "src/pch.hpp")
target_include_directories(Keyboard PRIVATE PDCurses)
target_include_directories(Keyboard PRIVATE "contrib/ImTui/include")
target_include_directories(Keyboard PRIVATE "contrib/ImTui/third-party/imgui")
target_link_libraries(Keyboard PRIVATE imtui imgui-for-imtui)