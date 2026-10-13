# Every recorded trace of the FPGA simulation is a test: the core must give the same trace and the
# same final state (scripts/make-references.py records them), replaying its timing automaton
# (reference.*) and stepping alone (stepped.*).
file( GLOB REFERENCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/reference/*.args" )
foreach( args ${REFERENCES} )
  get_filename_component( name ${args} NAME_WE )
  add_test( NAME reference.${name}
    COMMAND jagmcp-cli --check "${CMAKE_CURRENT_SOURCE_DIR}/reference/${name}" )
  set_tests_properties( reference.${name} PROPERTIES LABELS reference )
  add_test( NAME stepped.${name}
    COMMAND jagmcp-cli --check "${CMAKE_CURRENT_SOURCE_DIR}/reference/${name}" --step )
  set_tests_properties( stepped.${name} PROPERTIES LABELS stepped )
endforeach( )

# The control protocol (docs/spec/control-protocol.md): each transcript's requests answered as its
# .expect file says, run from this directory so that its paths are relative to it; and the network
# transports over real sockets.
foreach( transcript lines mcp )
  set( transport "" )
  if( transcript STREQUAL "mcp" )
    set( transport "--mcp" )
  endif( )
  add_test( NAME control.${transcript}
    COMMAND jagmcp-cli --batch control/${transcript}.jsonl ${transport} --expect control/${transcript}.expect
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" )
  set_tests_properties( control.${transcript} PROPERTIES LABELS control )
endforeach( )

# The version is the tag the build was made from or devel (cmake/Version.cmake); what is held is
# that CMake resolved one at all, a word without spaces or quotes.
add_test( NAME version COMMAND jagmcp-cli --version )
set_tests_properties( version PROPERTIES LABELS control
  PASS_REGULAR_EXPRESSION "^jagmcp-cli (devel|[0-9])[^ \"]*\n$" )

if( NOT WIN32 )
  add_executable( network_servers_test control/NetworkServersTest.cpp )
  target_link_libraries( network_servers_test PRIVATE jag_server httplib jag::warnings )
  add_test( NAME control.network COMMAND network_servers_test )
  set_tests_properties( control.network PROPERTIES LABELS control )
endif( )
