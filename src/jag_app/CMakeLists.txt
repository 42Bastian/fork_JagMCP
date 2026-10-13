add_executable( jag_app
  src/Application.cpp
  src/AudioOutput.cpp
  src/EmulationThread.cpp
  src/GpuTexture.cpp
  src/Main.cpp )

set_target_properties( jag_app PROPERTIES OUTPUT_NAME jagmcp )

target_link_libraries( jag_app PRIVATE jag_server imgui SDL3::SDL3-static jag::warnings )
