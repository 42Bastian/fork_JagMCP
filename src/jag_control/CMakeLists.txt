add_library( jag_control STATIC
  src/AudioMethods.cpp
  src/CpuMethods.cpp
  src/DebugMethods.cpp
  src/Dispatcher.cpp
  src/EmuMethods.cpp
  src/InputMethods.cpp
  src/MemoryMethods.cpp
  src/Params.cpp
  src/Process.cpp
  src/Recorder.cpp
  src/Session.cpp
  src/VideoMethods.cpp )

target_include_directories( jag_control PUBLIC include )
# The version the build reports: the tag it was made from, or devel (cmake/Version.cmake).
target_compile_definitions( jag_control PRIVATE JAG_VERSION="${JAG_VERSION_STRING}" )
target_link_libraries( jag_control PUBLIC jag_core nlohmann_json::nlohmann_json PRIVATE jag::warnings )
