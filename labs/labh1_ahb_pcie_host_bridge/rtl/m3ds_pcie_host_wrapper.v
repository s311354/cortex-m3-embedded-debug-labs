module m3ds_pcie_host_wrapper
(
	input wire         HCLK,
	input wire         HRESETn,
	
	input wire         HSEL,
	input wire         HREADY,

	input wire [31:0]  HADDR,
	input wire [1:0]   HTRANS,
	input wire         HWRITE,
	input wire [2:0]   HSIZE,
	input wire [31:0]  HWDATA,

	output wire [31:0] HRDATA,
	output wire        HREADYOUT,
	output wire        HRESP
);

wire          req_valid;
wire          req_ready;

wire [1:0]    req_type;
wire [31:0]   req_bdf;
wire [9:0]    req_reg;
wire [31:0]   req_wdata;

wire          cpl_valid;
wire          cpl_ready;

wire [1:0]    cpl_status;
wire [31:0]   cpl_rdata;

labh1_ahb_pcie_host_bridge u_labh1_ahb_pcie_host_bridge
(
	.HCLK          (HCLK),
	.HRESETn       (HRESETn),

	.HSEL          (HSEL),
	.HREADY        (HREADY),

	.HADDR         (HADDR),
	.HTRANS        (HTRANS),
	.HWRITE        (HWRITE),
	.HSIZE         (HSIZE),
	.HWDATA        (HWDATA),

	.HRDATA        (HRDATA),
	.HREADYOUT     (HREADYOUT),
	.HRESP         (HRESP),

	.req_valid     (req_valid),
	.req_ready     (req_ready),

	.req_type      (req_type),
	.req_bdf       (req_bdf),
	.req_reg       (req_reg),
	.req_wdata     (req_wdata),

	.cpl_valid     (cpl_valid),
	.cpl_ready     (cpl_ready),

	.cpl_status    (cpl_status),
	.cpl_rdata     (cpl_rdata)
);

/*
* This module name is intentionally generic
* H1: fake backend
*/
labh1_pcie_backend_stub u_m3ds_pcie_backend
(
	.clk           (HCLK),
	.resetn        (HRESETn),
	
	.req_valid     (req_valid),
	.req_ready     (req_ready),

	.req_type      (req_type),
	.req_bdf       (req_bdf),
	.req_reg       (req_reg),
	.req_wdata     (req_wdata),

	.cpl_valid     (cpl_valid),
	.cpl_ready     (cpl_ready),

	.cpl_status    (cpl_status),
	.cpl_rdata     (cpl_rdata)
);

endmodule
