# Verilator simulation of the Jaguar_MiSTer core (Tom, Jerry, 68000), the reference of the core.
# usage: make [RTL=/path/to/Jaguar_MiSTer/rtl] [VCD=1]      -> obj_dir/jagsim

RTL ?= ../../Jaguar_MiSTer/rtl
OBJ := obj_dir

# defs.v is left out: it defines SIMULATION under Verilator, which selects RAM models that differ from
# the synthesized altsyncram ones (altsyncram.v models those instead).
# abus.v declares "wire fcl[0];" which Quartus reads as a one bit array and Verilator as an empty one.
# 68k.v (nuked 68000, selected by ACCURATE_CPU in jaguar.v) loads its microcode from relative paths.
# raa016a.v (Jerry's wave ROM) prints every read with $display, which would end up in the state.
PATCHED := $(OBJ)/patched/abus.v $(OBJ)/patched/68k.v $(OBJ)/patched/raa016a.v

RTL_SRC := \
	$(RTL)/jaguar.v \
	$(filter-out $(RTL)/jaguar_common/defs.v,$(wildcard $(RTL)/jaguar_common/*.v)) \
	$(filter-out $(RTL)/Tom/abus.v,$(wildcard $(RTL)/Tom/*.v)) \
	$(filter-out $(RTL)/Jerry/raa016a.v,$(wildcard $(RTL)/Jerry/*.v)) \
	$(wildcard $(RTL)/Butch/*.v) \
	$(RTL)/jag_lightgun.sv $(RTL)/eeprom_93c46_x16.v $(RTL)/tda1545a.v \
	$(RTL)/jag_controller_mux.v $(RTL)/jag_team_tap.v $(RTL)/numstick.sv $(RTL)/ps2_mouse.v

SIM_SRC := sim_top.sv altsyncram.v spram.v $(PATCHED)

VFLAGS := --cc --exe --build -j 0 --Mdir $(OBJ) --top-module sim_top -O3 --x-assign fast --x-initial fast \
	-Wno-fatal -Wno-lint -Wno-style -Wno-WIDTH -Wno-MULTIDRIVEN -Wno-UNOPTFLAT -Wno-TIMESCALEMOD \
	-I$(RTL) \
	-CFLAGS "-std=c++17 -O2" -MAKEFLAGS "OPT_FAST=-O2"

# make VCD=1 adds waveform dumping (--vcd FILE), slower
ifeq ($(VCD),1)
VFLAGS += --trace -CFLAGS -DVCD
endif

all: $(OBJ)/jagsim

$(OBJ)/patched/abus.v: $(RTL)/Tom/abus.v
	mkdir -p $(dir $@)
	sed -e 's/^wire fcl\[0\];/wire [0:0] fcl;/' $< > $@

$(OBJ)/patched/68k.v: $(RTL)/nuked-68k/68k.v
	mkdir -p $(dir $@)
	sed -e 's|"68k_\(.code\).txt"|"$(abspath $(RTL))/nuked-68k/68k_\1.txt"|' $< > $@

$(OBJ)/patched/raa016a.v: $(RTL)/Jerry/raa016a.v
	mkdir -p $(dir $@)
	sed -e '/$$display("SINEROM/d' $< > $@

$(OBJ)/jagsim: $(SIM_SRC) $(RTL_SRC) sim_main.cpp
	verilator $(VFLAGS) -o jagsim $(RTL)/defines.vh $(SIM_SRC) $(RTL_SRC) sim_main.cpp

clean:
	rm -rf $(OBJ)

.PHONY: all clean
