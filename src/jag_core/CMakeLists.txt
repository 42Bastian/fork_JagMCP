add_library( jag_core STATIC
  src/Blitter.cpp
  src/ProgramFile.cpp
  src/Eeprom.cpp
  src/Fiber.cpp
  src/Risc.cpp
  src/RiscDisassembler.cpp
  src/RiscAutomaton.cpp
  src/I2s.cpp
  src/JerryInterrupts.cpp
  src/M68000.cpp
  src/Machine.cpp
  src/PortabilityChecks.cpp
  src/MainBus.cpp
  src/Timers.cpp
  src/Tom.cpp
  src/ObjectProcessor.cpp
  src/Runner.cpp )

target_include_directories( jag_core PUBLIC include )
target_link_libraries( jag_core PRIVATE jag::warnings moira Threads::Threads )
