`timescale 1ns/1ps

module m3ds_pcie_backend (
	input wire           clk,
	input wire           resetn,

	/* Existing Configuration transaction contract */
	input wire           req_valid,
	input wire [1:0]     req_type,
	input wire [31:0]    req_bdf,
	input wire [9:0]     req_reg,
	input wire [31:0]    req_wdata,

	output wire          req_ready,

	input wire           cpl_ready,

	output wire          cpl_valid,
	output wire [1:0]    cpl_status,
	output wire [31:0]   cpl_rdata,

	/* H3 Memory transaction contract */
	input wire           mem_req_valid,
	input wire           mem_req_write,
	input wire [31:0]    mem_req_addr,
	input wire [31:0]    mem_req_wdata,

	output wire          mem_req_ready,

	input wire           mem_cpl_ready,

	output wire          mem_cpl_valid,
	output wire [1:0]    mem_cpl_status,
	output wire [31:0]   mem_cpl_rdata
);


labh3_pcie_vendor_adapter u_vendor_adapter (
	.host_clk               (clk),
	.host_resetn            (resetn),

	/* Config */
	.cfg_req_valid          (req_valid),
	.cfg_req_type           (req_type),
	.cfg_req_bdf            (req_bdf),
	.cfg_req_reg            (req_reg),
	.cfg_req_wdata          (req_wdata),

	.cfg_req_ready          (req_ready),

	.cfg_cpl_valid          (cpl_valid),

	.cfg_cpl_ready          (cpl_ready),
	.cfg_cpl_status         (cpl_status),
	.cfg_cpl_rdata          (cpl_rdata),

	/* Memory */
	.mem_req_valid          (mem_req_valid),
	.mem_req_write          (mem_req_write),
	.mem_req_addr           (mem_req_addr),
	.mem_req_wdata          (mem_req_wdata),

	.mem_req_ready          (mem_req_ready),

	.mem_cpl_valid          (mem_cpl_valid),

	.mem_cpl_ready          (mem_cpl_ready),
	.mem_cpl_status         (mem_cpl_status),
	.mem_cpl_rdata          (mem_cpl_rdata)
);

endmodule
