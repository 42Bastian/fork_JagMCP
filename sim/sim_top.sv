// Simulation top: the MiSTer jaguar core with its ports passed through, plus the DSP's and the
// GPU's trace signals taken by hierarchical references. Port list generated from jaguar.v.
/* verilator lint_off WIDTH */
module sim_top
(
	input           xresetl_in,
	input           cold_reset,
	input           sys_clk,
	output [9:0]    dram_a,
	output          dram_ras_n,
	output          dram_cas_n,
	output [3:0]    dram_oe_n,
	output [3:0]    dram_uw_n,
	output [3:0]    dram_lw_n,
	output [63:0]   dram_d,
	input  [63:0]   dram_q,
	input  [3:0]    dram_oe,
	input           ram_rdy,
	output          dram_go_rd,
	output          dram_rw,
	output [23:0]   dram_addr,
	output [7:0]    dram_be,
	output          dram_startwep,
	output [10:3]   dram_addrp,
	output [23:0]   abus_out,
	output          os_rom_ce_n,
	input  [7:0]    os_rom_q,
	output          cart_ce_n,
	output [1:0]    cart_oe,
	input  [31:0]   cart_q,
	output [9:0]    cart_bram_addr,
	output [15:0]   cart_bram_data,
	input  [15:0]   cart_bram_q,
	output          cart_bram_wr,
	output [9:0]    cd_bram_addr,
	output [15:0]   cd_bram_data,
	input  [15:0]   cd_bram_q,
	output          cd_bram_wr,
	output          vga_vs,
	output          vga_hs,
	output          hblank,
	output          vblank,
	output          interlaced,
	output          nextfield,      // vid.v: VC wraps at VP, for the frame collector
	output [7:0]    vga_r,
	output [7:0]    vga_g,
	output [7:0]    vga_b,
	output [15:0]   aud_16_l,
	output [15:0]   aud_16_r,
	input           xwaitl,
	output          vid_ce,
	input  [31:0]   joystick_0,
	input  [31:0]   joystick_1,
	input  [31:0]   joystick_2,
	input  [31:0]   joystick_3,
	input  [31:0]   joystick_4,
	input  [7:0]    analog_0,
	input  [7:0]    analog_1,
	input  [7:0]    analog_2,
	input  [7:0]    analog_3,
	input  [8:0]    spinner_0,
	input  [8:0]    spinner_1,
	input  [1:0]    spinner_speed,
	input           team_tap_port1,
	input           team_tap_port2,
	input  [1:0]    lightgun_mode,
	input  [1:0]    lightgun_crosshair,
	input  [24:0]   ps2_mouse,
	input           mouse_ena_1,
	input           mouse_ena_2,
	output          comlynx_tx,
	input           comlynx_rx,
	output          startcas,
	input           maxc,
	input           auto_eeprom,
	output [23:0]   addr_ch3,
	input  [9:0]    toc_addr,
	input  [15:0]   toc_data,
	input           toc_wr,
	input           toc_done,
	output [29:0]   audbus_out,
	input  [63:0]   aud_in,
	input           audwaitl,
	output          aud_ce,
	input           aud_busy,
	input           aud_sess,
	input  [63:0]   cdg_in,
	input           force_music_cd,
	input           cd_en,
	input           cd_ex,
	input           cd_latency_en,
	output          b_override,
	input           dohacks,
	output          xvclk_o,
	output          overflow,
	output          underflow,
	output          errflow,
	output          unhandled,
	input           cd_valid,
	input           cd_sector2448,
	output          aud_16_eq,
	input           turbo,
	input           vintbugfix,
	input           olpbugfix,
	input           ddreq,
	output          m68k_clk,
	output [22:0]   m68k_addr,
	output [15:0]   m68k_bus_do,
	input           gamedrive_enable,
	input           active_video,
	input           ntsc,

	// RISC trace, the DSP's (dsp_) and the GPU's (gpu_); `gpu` picks the local RAM poked
	input               gpu,            // the program is the GPU's
	output              risc_clk,       // the RISCs' clock enable (one sys_clk pulse per cycle)
	output         dsp_go,               // DSPGO
	output         dsp_wa_n,             // register file write port A (loads, divide, ...)
	output  [5:0]  dsp_wa_reg,
	output [31:0]  dsp_wa_data,
	output         dsp_wb_n,             // register file write port B (ALU results)
	output  [5:0]  dsp_wb_reg,
	output [31:0]  dsp_wb_data,
	output         dsp_exe,              // an instruction is issued (leaves the read operands stage)
	output [23:0]  dsp_pc,               // its address (program_count of the prefetcher)
	output         dsp_mreq,             // external bus cycle request (the gateway's mreq)
	output         dsp_read,
	output [23:0]  dsp_addr,
	output  [3:0]  dsp_width,
	output [31:0]  dsp_wdata,
	output  [2:0]  dsp_flags,            // N, C, Z
	output         gpu_go,
	output         gpu_wa_n,
	output  [5:0]  gpu_wa_reg,
	output [31:0]  gpu_wa_data,
	output         gpu_wb_n,
	output  [5:0]  gpu_wb_reg,
	output [31:0]  gpu_wb_data,
	output         gpu_exe,
	output [23:0]  gpu_pc,
	output         gpu_mreq,
	output         gpu_read,
	output [23:0]  gpu_addr,
	output  [3:0]  gpu_width,
	output [31:0]  gpu_wdata,
	output  [2:0]  gpu_flags,
	output              dsp_i2int,      // I2S interrupt request from the I2S block
	output              i2s_ws,         // I2S word select (Jerry as master)
	output      [15:0]  snd_l,          // I2S transmit latches
	output      [15:0]  snd_r,
	input        [5:0]  dbg_reg_addr,   // register file backdoor read, both chips'
	output      [31:0]  dsp_reg_data,
	output      [31:0]  gpu_reg_data,
	output              m68k_as_n,      // the 68000's bus cycle: AS, the data strobes, read, function code
	output              m68k_uds_n,
	output              m68k_lds_n,
	output              m68k_rw,
	output       [2:0]  m68k_fc,
	output      [15:0]  m68k_dout,      // its data out, and in (m68k_di, as it latches it)
	output      [15:0]  m68k_din,
	input               ram_poke,       // DSP or GPU RAM backdoor write, at the sys_clk edge
	input       [10:0]  ram_poke_addr,  // long word index from D_RAM or G_RAM
	input       [31:0]  ram_poke_data
);

// Jaguar.sv feeds the 68000 data input through the cheat engine, a register clocked by m68k_clk
reg [15:0] m68k_di = 16'h0;
always @(posedge sys_clk)
	if (m68k_clk) m68k_di <= m68k_bus_do;

jaguar jag
(
	.xresetl_in          (xresetl_in),
	.cold_reset          (cold_reset),
	.sys_clk             (sys_clk),
	.dram_a              (dram_a),
	.dram_ras_n          (dram_ras_n),
	.dram_cas_n          (dram_cas_n),
	.dram_oe_n           (dram_oe_n),
	.dram_uw_n           (dram_uw_n),
	.dram_lw_n           (dram_lw_n),
	.dram_d              (dram_d),
	.dram_q              (dram_q),
	.dram_oe             (dram_oe),
	.ram_rdy             (ram_rdy),
	.dram_go_rd          (dram_go_rd),
	.dram_rw             (dram_rw),
	.dram_addr           (dram_addr),
	.dram_be             (dram_be),
	.dram_startwep       (dram_startwep),
	.dram_addrp          (dram_addrp),
	.abus_out            (abus_out),
	.os_rom_ce_n         (os_rom_ce_n),
	.os_rom_q            (os_rom_q),
	.cart_ce_n           (cart_ce_n),
	.cart_oe             (cart_oe),
	.cart_q              (cart_q),
	.cart_bram_addr      (cart_bram_addr),
	.cart_bram_data      (cart_bram_data),
	.cart_bram_q         (cart_bram_q),
	.cart_bram_wr        (cart_bram_wr),
	.cd_bram_addr        (cd_bram_addr),
	.cd_bram_data        (cd_bram_data),
	.cd_bram_q           (cd_bram_q),
	.cd_bram_wr          (cd_bram_wr),
	.vga_vs              (vga_vs),
	.vga_hs              (vga_hs),
	.hblank              (hblank),
	.vblank              (vblank),
	.interlaced          (interlaced),
	.vga_r               (vga_r),
	.vga_g               (vga_g),
	.vga_b               (vga_b),
	.aud_16_l            (aud_16_l),
	.aud_16_r            (aud_16_r),
	.xwaitl              (xwaitl),
	.vid_ce              (vid_ce),
	.joystick_0          (joystick_0),
	.joystick_1          (joystick_1),
	.joystick_2          (joystick_2),
	.joystick_3          (joystick_3),
	.joystick_4          (joystick_4),
	.analog_0            (analog_0),
	.analog_1            (analog_1),
	.analog_2            (analog_2),
	.analog_3            (analog_3),
	.spinner_0           (spinner_0),
	.spinner_1           (spinner_1),
	.spinner_speed       (spinner_speed),
	.team_tap_port1      (team_tap_port1),
	.team_tap_port2      (team_tap_port2),
	.lightgun_mode       (lightgun_mode),
	.lightgun_crosshair  (lightgun_crosshair),
	.ps2_mouse           (ps2_mouse),
	.mouse_ena_1         (mouse_ena_1),
	.mouse_ena_2         (mouse_ena_2),
	.comlynx_tx          (comlynx_tx),
	.comlynx_rx          (comlynx_rx),
	.startcas            (startcas),
	.maxc                (maxc),
	.auto_eeprom         (auto_eeprom),
	.addr_ch3            (addr_ch3),
	.toc_addr            (toc_addr),
	.toc_data            (toc_data),
	.toc_wr              (toc_wr),
	.toc_done            (toc_done),
	.audbus_out          (audbus_out),
	.aud_in              (aud_in),
	.audwaitl            (audwaitl),
	.aud_ce              (aud_ce),
	.aud_busy            (aud_busy),
	.aud_sess            (aud_sess),
	.cdg_in              (cdg_in),
	.force_music_cd      (force_music_cd),
	.cd_en               (cd_en),
	.cd_ex               (cd_ex),
	.cd_latency_en       (cd_latency_en),
	.b_override          (b_override),
	.dohacks             (dohacks),
	.xvclk_o             (xvclk_o),
	.overflow            (overflow),
	.underflow           (underflow),
	.errflow             (errflow),
	.unhandled           (unhandled),
	.cd_valid            (cd_valid),
	.cd_sector2448       (cd_sector2448),
	.aud_16_eq           (aud_16_eq),
	.turbo               (turbo),
	.vintbugfix          (vintbugfix),
	.olpbugfix           (olpbugfix),
	.ddreq               (ddreq),
	.m68k_clk            (m68k_clk),
	.m68k_addr           (m68k_addr),
	.m68k_bus_do         (m68k_bus_do),
	.m68k_di             (m68k_di),
	.gamedrive_enable    (gamedrive_enable),
	.active_video        (active_video),
	.ntsc                (ntsc)
);

`define DSP jag.jerry_inst.dsp_inst
`define GPU jag.tom_inst.gpu_inst
assign risc_clk      = `DSP.clk; // Tom's and Jerry's processor clock is the same (jaguar.v xpclk)
assign dsp_go        = `DSP.go;
assign dsp_wa_n      = `DSP.srcrwen_n;
assign dsp_wa_reg    = `DSP.srca;
assign dsp_wa_data   = `DSP.srcwd;
assign dsp_wb_n      = `DSP.dstrwen_n;
assign dsp_wb_reg    = `DSP.dsta;
assign dsp_wb_data   = `DSP.dstwd;
assign dsp_exe       = `DSP.ins_exec_inst.exe;
assign dsp_pc        = `DSP.ins_exec_inst.program_count;
assign dsp_mreq      = `DSP.mreq;
assign dsp_read      = `DSP.read;
assign dsp_addr      = `DSP.a;
assign dsp_width     = { 1'b0, `DSP.width };
assign dsp_wdata     = `DSP.wdata;
assign dsp_flags     = { `DSP.nega_flag, `DSP.carry_flag, `DSP.zero_flag };
assign gpu_go        = `GPU.go;
assign gpu_wa_n      = `GPU.srcrwen_n;
assign gpu_wa_reg    = `GPU.srca;
assign gpu_wa_data   = `GPU.srcwd;
assign gpu_wb_n      = `GPU.dstrwen_n;
assign gpu_wb_reg    = `GPU.dsta;
assign gpu_wb_data   = `GPU.dstwd;
assign gpu_exe       = `GPU.ins_exec_inst.exe;
assign gpu_pc        = `GPU.ins_exec_inst.program_count;
assign gpu_mreq      = `GPU.mreq_gateway_out;
assign gpu_read      = `GPU.read_gateway_out;
assign gpu_addr      = `GPU.gateway_addr_out;
assign gpu_width     = `GPU.width_gateway_out;
assign gpu_wdata     = `GPU.wdata_gateway_out[31:0];
assign gpu_flags     = { `GPU.nega_flag, `GPU.carry_flag, `GPU.zero_flag };
assign dsp_i2int     = jag.jerry_inst.i2int;
assign i2s_ws        = jag.j_xws_out;
assign snd_l         = jag.snd_l;
assign snd_r         = jag.snd_r;
assign dsp_reg_data  = `DSP.registers_inst.reg_ram_inst.altsyncram_component.mem[dbg_reg_addr];
assign gpu_reg_data  = `GPU.registers_inst.reg_ram_inst.altsyncram_component.mem[dbg_reg_addr];
assign m68k_as_n     = jag.fx68k_as_n;
assign m68k_uds_n    = jag.fx68k_uds_n;
assign m68k_lds_n    = jag.fx68k_lds_n;
assign nextfield     = jag.tom_inst.vid_inst.nextfield;
assign m68k_rw       = jag.fx68k_rw;
assign m68k_fc       = jag.fx68k_fc;
assign m68k_dout     = jag.fx68k_dout;
assign m68k_din      = m68k_di;

// The program is written into the local RAM before reset ends, so that the boot program does not
// depend on it: the DSP's two blocks of 1024 long words, $F1B000 and $F1C000, or the GPU's one at
// $F03000.
always @(posedge sys_clk)
	if (ram_poke) begin
		if (gpu)
			`GPU.gpu_ram_inst.ram_inst.altsyncram_component.mem[ram_poke_addr[9:0]] <= ram_poke_data;
		else if (ram_poke_addr[10])
			`DSP.dsp_ram_inst.program_index_1_inst.ram_inst.altsyncram_component.mem[ram_poke_addr[9:0]] <= ram_poke_data;
		else
			`DSP.dsp_ram_inst.program_index_0_inst.ram_inst.altsyncram_component.mem[ram_poke_addr[9:0]] <= ram_poke_data;
	end

endmodule
