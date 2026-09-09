`timescale 1ns / 1ps

module m3ds_pcie_backend (
    input wire         clk,
    input wire         resetn,

    input wire         req_valid,
    input wire [1:0]   req_type,
    input wire [31:0]  req_bdf,
    input wire [9:0]   req_reg,
    input wire [31:0]  req_wdata,

    output wire        req_ready,

    input wire         cpl_ready,

    output wire        cpl_valid,
    output wire [1:0]  cpl_status,
    output wire [31:0] cpl_rdata
);

labh1_pcie_backend_stub u_labh1_pcie_backend_stub
(
	// Inputs
	.clk           (clk),
	.resetn        (resetn),
	
	.req_valid     (req_valid),
	.req_type      (req_type),
	.req_bdf       (req_bdf),
	.req_reg       (req_reg),
	.req_wdata     (req_wdata),

	// Outputs
	.req_ready     (req_ready),

	// Inputs
	.cpl_ready     (cpl_ready),

	// Outputs
	.cpl_valid     (cpl_valid),
	.cpl_status    (cpl_status),
	.cpl_rdata     (cpl_rdata)
);

endmodule
