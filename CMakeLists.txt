cmake_minimum_required( VERSION 3.25 )

project( JagMCP
  DESCRIPTION "Emulator of the Atari Jaguar, its timing taken from the netlists of its FPGA core"
  LANGUAGES   CXX )

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )

# No `VERSION` above: the version is the tag, and cmake/Version.cmake says why.
include( Version )
jagResolveVersion( JAG_VERSION_STRING )
message( STATUS "JagMCP version: ${JAG_VERSION_STRING}" )

set( CMAKE_CXX_STANDARD          23 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS        OFF )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

option( JAG_BUILD_TESTS "Build the test suite"               ON )
option( JAG_BUILD_APP   "Build the SDL3 desktop application" ON )
option( JAG_WERROR      "Treat compiler warnings as errors"  OFF )

include( Warnings )
include( Dependencies )

# Third-party code carried in the tree (libextern/README.md)
add_subdirectory( libextern/Moira )

add_subdirectory( src/jag_core )
add_subdirectory( src/jag_control )
add_subdirectory( src/jag_server )
add_subdirectory( src/jag_cli )

# The application is the one target that needs SDL and ImGui, so a headless build and the suite
# can leave both out.
if( JAG_BUILD_APP )
  add_subdirectory( src/jag_app )
endif( )

if( JAG_BUILD_TESTS )
  enable_testing( )
  add_subdirectory( tests )
endif( )

include( Install )
