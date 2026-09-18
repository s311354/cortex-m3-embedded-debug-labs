`timescale 1ns/1ps

module labh3_pcie_vendor_adapter (
	input wire             host_clk,
	input wire             host_resetn,

	/* Host-side Configuration Request */
        input wire             cfg_req_valid,
	input wire [1:0]       cfg_req_type,
	input wire [31:0]      cfg_req_bdf,
	input wire [9:0]       cfg_req_reg,
	input wire [31:0]      cfg_req_wdata,

        output wire            cfg_req_ready,

	input wire             cfg_cpl_ready,

	output wire            cfg_cpl_valid,
	output wire [1:0]      cfg_cpl_status,
	output wire [31:0]     cfg_cpl_rdata,

	/* Host-side Memory Request */
	input wire             mem_req_valid,
	input wire             mem_req_write,
	input wire [31:0]      mem_req_addr,
	input wire [31:0]      mem_req_wdata,

	output wire            mem_req_ready,

	input wire             mem_cpl_ready,

	output wire            mem_cpl_valid,
	output wire [1:0]      mem_cpl_status,
	output wire [31:0]     mem_cpl_rdata
);

/*
* FPGA-VENDOR SPECIFIC IMPLEMENTATION REQUIRED HERE
*
* Required semantics:
* 
* CFG_READ:
*     non-posted
*     completion required
*
* CFG_WRITE:
*     PCIe Configuration Write is also non-posted
*     completion required
*
* MEM_READ:
*     non-posted
*     completion-with-data required
*
* MEM_WRITE:
*     posted
*     no PCIe Completion
*/

assign cfg_req_ready        = 1'b0;

assign cfg_cpl_valid        = 1'b0;
assign cfg_cpl_status       = 2'b01;
assign cfg_cpl_rdata        = 32'hFFFF_FFFF;

assign mem_req_ready        = 1'b0;

assign mem_cpl_valid        = 1'b0;
assign mem_cpl_status       = 2'b01;
assign mem_cpl_rdata        = 32'hFFFF_FFFF;


/*
* Avoid unused-input warnings in the contract-only version
*/
wire _unused_ok = &{
	1'b0,
	host_clk,
	host_resetn,

	cfg_req_valid,
	cfg_req_type,
	cfg_req_bdf,
	cfg_req_reg,
	cfg_req_wdata,

	cfg_cpl_ready,

	mem_req_valid,
	mem_req_write,
	mem_req_addr,
	mem_req_wdata,

	mem_cpl_ready
};

endmodule
