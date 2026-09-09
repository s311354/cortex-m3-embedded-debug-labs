module m3ds_pcie_host_wrapper
(
        // Inputs
	input wire         HCLK,
	input wire         HRESETn,
	
	input wire         HSEL,
	input wire [31:0]  HADDR,
	input wire         HWRITE,
	input wire [1:0]   HTRANS,
	input wire [2:0]   HSIZE,

	input wire [31:0]  HWDATA,
	input wire         HREADY,

	// Outputs
	output wire [31:0] HRDATA,
	output wire        HREADYOUT,
	output wire        HRESP
);

/*
* Abstract PCIe request channel
* Host Bridge -> backend
*/
wire          req_valid;
wire [1:0]    req_type;
wire [31:0]   req_bdf;
wire [9:0]    req_reg;
wire [31:0]   req_wdata;

wire          req_ready;

/*
* Abstract PCIe completion channel
* backend -> Host Bridge
*/
wire          cpl_ready;

wire          cpl_valid;
wire [1:0]    cpl_status;
wire [31:0]   cpl_rdata;

/*
* Shared AHB-to-PCIe Host Bridge
*/
labh1_ahb_pcie_host_bridge u_labh1_ahb_pcie_host_bridge
(
	// Inputs
	.HCLK          (HCLK),
	.HRESETn       (HRESETn),

	.HSEL          (HSEL),
	.HADDR         (HADDR),
	.HTRANS        (HTRANS),
	.HWRITE        (HWRITE),
	.HSIZE         (HSIZE),

	.HWDATA        (HWDATA),
	.HREADY        (HREADY),

	// Outputs
	.HRDATA        (HRDATA),
	.HREADYOUT     (HREADYOUT),
	.HRESP         (HRESP),

	// Inputs
	.req_ready     (req_ready),

	// Outputs
	.req_valid     (req_valid),
	.req_type      (req_type),
	.req_bdf       (req_bdf),
	.req_reg       (req_reg),
	.req_wdata     (req_wdata),

	// Inputs
	.cpl_valid     (cpl_valid),
	.cpl_status    (cpl_status),
	.cpl_rdata     (cpl_rdata),

	// Outputs
	.cpl_ready     (cpl_ready)
);

/*
* This module name is intentionally generic
* H1: m3ds_pcie_backend -> labh1_pcie_backend_stub (fake backend)
* H2: m3ds_pcie_backend -> TLP TX -> endpoint transaction model -> TLP RX
*/
m3ds_pcie_backend u_m3ds_pcie_backend
(
	// Inputs
	.clk           (HCLK),
	.resetn        (HRESETn),
	
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
