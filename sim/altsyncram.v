// Behavioural model of the Altera altsyncram megafunction, as far as the Jaguar core uses it:
// SINGLE_PORT, ROM and BIDIR_DUAL_PORT with one clock (clock0), registered inputs and optionally
// registered outputs (outdata_reg_x = "CLOCK0" adds one more clock of latency).
// Read during write: same port returns the new data (NEW_DATA_NO_NBE_READ, the default),
// the other port returns the old data (OLD_DATA; DONT_CARE is modelled the same way).
/* verilator lint_off WIDTH */
/* verilator lint_off UNUSED */
/* verilator lint_off PINCONNECTEMPTY */
module altsyncram
#(
	parameter operation_mode = "SINGLE_PORT",
	parameter width_a = 8,
	parameter widthad_a = 8,
	parameter numwords_a = 256,
	parameter width_b = 8,
	parameter widthad_b = 8,
	parameter numwords_b = 256,
	parameter outdata_reg_a = "UNREGISTERED",
	parameter outdata_reg_b = "UNREGISTERED",
	parameter read_during_write_mode_port_a = "NEW_DATA_NO_NBE_READ",
	parameter read_during_write_mode_port_b = "NEW_DATA_NO_NBE_READ",
	parameter read_during_write_mode_mixed_ports = "OLD_DATA",
	parameter init_file = "UNUSED",
	parameter width_byteena_a = 1,
	parameter width_byteena_b = 1,
	parameter address_reg_b = "CLOCK0",
	parameter indata_reg_b = "CLOCK0",
	parameter wrcontrol_wraddress_reg_b = "CLOCK0",
	parameter clock_enable_input_a = "BYPASS",
	parameter clock_enable_input_b = "BYPASS",
	parameter clock_enable_output_a = "BYPASS",
	parameter clock_enable_output_b = "BYPASS",
	parameter intended_device_family = "Cyclone V",
	parameter lpm_hint = "UNUSED",
	parameter lpm_type = "altsyncram",
	parameter outdata_aclr_a = "NONE",
	parameter outdata_aclr_b = "NONE",
	parameter power_up_uninitialized = "FALSE"
)
(
	input                        clock0,
	input                        clock1,
	input                        wren_a,
	input                        wren_b,
	input                        rden_a,
	input                        rden_b,
	input      [widthad_a-1:0]   address_a,
	input      [widthad_b-1:0]   address_b,
	input      [width_a-1:0]     data_a,
	input      [width_b-1:0]     data_b,
	output     [width_a-1:0]     q_a,
	output     [width_b-1:0]     q_b,
	input                        aclr0,
	input                        aclr1,
	input                        addressstall_a,
	input                        addressstall_b,
	input  [width_byteena_a-1:0] byteena_a,
	input  [width_byteena_b-1:0] byteena_b,
	input                        clocken0,
	input                        clocken1,
	input                        clocken2,
	input                        clocken3,
	output                 [2:0] eccstatus
);

localparam DUAL = operation_mode == "BIDIR_DUAL_PORT";
localparam ROM = operation_mode == "ROM";

reg [width_a-1:0] mem [0:numwords_a-1];

integer i;
initial begin
	for (i = 0; i < numwords_a; i = i + 1)
		mem[i] = {width_a{1'b0}};
end

reg [width_a-1:0] qa_int = 0;
reg [width_b-1:0] qb_int = 0;
reg [width_a-1:0] qa_reg = 0;
reg [width_b-1:0] qb_reg = 0;

wire wa = wren_a & ~ROM;
wire wb = wren_b & DUAL;

always @(posedge clock0) begin
	// nonblocking reads below see the memory before this edge's writes (old data)
	if (wa)
		mem[address_a] <= data_a;
	if (wb)
		mem[address_b] <= data_b;

	if (wa && read_during_write_mode_port_a != "OLD_DATA")
		qa_int <= data_a;
	else if (wb && address_b == address_a && read_during_write_mode_mixed_ports != "OLD_DATA"
	         && read_during_write_mode_mixed_ports != "DONT_CARE")
		qa_int <= data_b;
	else
		qa_int <= mem[address_a];

	if (DUAL) begin
		if (wb && read_during_write_mode_port_b != "OLD_DATA")
			qb_int <= data_b;
		else
			qb_int <= mem[address_b];
	end

	qa_reg <= qa_int;
	qb_reg <= qb_int;
end

assign q_a = outdata_reg_a == "CLOCK0" ? qa_reg : qa_int;
assign q_b = outdata_reg_b == "CLOCK0" ? qb_reg : qb_int;
assign eccstatus = 3'b000;

endmodule
