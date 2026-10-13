// Single port RAM from the MiSTer framework (used by Butch for CD cue tables only)
module spram #(parameter addr_width = 8, parameter data_width = 8)
(
	input                       clock,
	input      [addr_width-1:0] address,
	input      [data_width-1:0] data,
	input                       wren,
	output reg [data_width-1:0] q
);
reg [data_width-1:0] mem [0:(1<<addr_width)-1];
always @(posedge clock) begin
	if (wren) mem[address] <= data;
	q <= mem[address];
end
endmodule
