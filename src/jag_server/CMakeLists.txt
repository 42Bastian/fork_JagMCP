add_library( jag_server STATIC
  src/JsonLinesServer.cpp
  src/McpHttpServer.cpp
  src/McpServer.cpp
  src/TcpLineServer.cpp )

target_include_directories( jag_server PUBLIC include )

target_link_libraries( jag_server
  PUBLIC  jag_control
  PRIVATE jag::warnings httplib Threads::Threads )
