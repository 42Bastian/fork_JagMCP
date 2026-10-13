add_executable( jagmcp-cli main.cpp )
target_link_libraries( jagmcp-cli PRIVATE jag_server jag::warnings )
