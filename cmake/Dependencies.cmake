include( FetchContent )

# Dependencies are pinned to exact tags, and are PGMEmu's: the window is built as PGMEmu's is
# (docs/decisions/0011-the-window-is-pgmemus.md), the control API and its transports as PGMEmu's
# are (docs/decisions/0018-one-control-api.md). The core needs none of them.

# The control API's requests and answers.
FetchContent_Declare( nlohmann_json
  URL      https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SYSTEM )

# MCP's HTTP transport. A single header; upstream's build is not used, as it looks for OpenSSL,
# zlib and Brotli, and the server needs none of them.
FetchContent_Declare( httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.60.1
  GIT_SHALLOW    TRUE
  SOURCE_SUBDIR  none
  SYSTEM )

FetchContent_MakeAvailable( nlohmann_json httplib )

find_package( Threads REQUIRED )
add_library( httplib INTERFACE )
target_include_directories( httplib SYSTEM INTERFACE "${httplib_SOURCE_DIR}" )
target_link_libraries( httplib INTERFACE Threads::Threads $<$<PLATFORM_ID:Windows>:ws2_32> )

if( JAG_BUILD_APP )
  # Static, so that the application carries the SDL it was built and tested against rather than
  # whichever one the system has.
  set( SDL_SHARED       OFF CACHE BOOL "" FORCE )
  set( SDL_STATIC       ON  CACHE BOOL "" FORCE )
  set( SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE )
  set( SDL_EXAMPLES     OFF CACHE BOOL "" FORCE )
  FetchContent_Declare( SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.18
    GIT_SHALLOW    TRUE
    SYSTEM )

  # Dear ImGui ships no build of its own. FetchContent only unpacks it, and the target below
  # compiles the core and the two backends the application uses: SDL3 for the platform and SDL_GPU
  # for rendering.
  FetchContent_Declare( imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9b-docking
    GIT_SHALLOW    TRUE
    SYSTEM )

  FetchContent_MakeAvailable( SDL3 imgui )

  add_library( imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp" )

  target_include_directories( imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends" )

  # The application never reaches for an ImGui function that was renamed or removed, so the
  # compatibility layer upstream still carries is left out.
  target_compile_definitions( imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS )

  target_link_libraries( imgui PUBLIC SDL3::SDL3-static )
endif( )
