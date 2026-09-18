`timescale 1ns/1ps

module labh3_pcie_root_port_wrapper (
    input wire         HCLK,
    input wire         HRESETn,

    input wire         CSR_HSEL,
    input wire [31:0]  CSR_HADDR,
    input wire [1:0]   CSR_HTRANS,
    input wire         CSR_HWRITE,
    input wire [2:0]   CSR_HSIZE,

    input wire [31:0]  CSR_HWDATA,
    input wire         CSR_HREADY,

    output wire [31:0] CSR_HRDATA,
    output wire        CSR_HREADYOUT,
    output wire        CSR_HRESP,

    /*
    * TARGEXP1
    *
    * PCIe outbound MMIO aperture
    */
    input wire         MMIO_HSEL,
    input wire [31:0]  MMIO_HADDR,
    input wire [1:0]   MMIO_HTRANS,
    input wire         MMIO_HWRITE,
    input wire [2:0]   MMIO_HSIZE,

    input wire [31:0]  MMIO_HWDATA,
    input wire         MMIO_HREADY,

    output wire [31:0] MMIO_HRDATA,
    output wire        MMIO_HREADYOUT,
    output wire        MMIO_HRESP
);

/*
* Configuration transaction channel
*/
wire          cfg_req_ready;

wire          cfg_req_valid;
wire [1:0]    cfg_req_type;
wire [31:0]   cfg_req_bdf;
wire [9:0]    cfg_req_reg;
wire [31:0]   cfg_req_wdata;

wire          cfg_cpl_valid;
wire [1:0]    cfg_cpl_status;
wire [31:0]   cfg_cpl_rdata;

wire          cfg_cpl_ready;

/*
* Memory transaction channel
*/
wire          mem_req_valid;
wire          mem_req_ready;

wire          mem_req_write;
wire [31:0]   mem_req_addr;
wire [31:0]   mem_req_wdata;

wire          mem_cpl_valid;
wire          mem_cpl_ready;

wire [1:0]    mem_cpl_status;
wire [31:0]   mem_cpl_rdata;

/*
* Existing H1 Host Controller CSR frontend
*/
labh1_ahb_pcie_host_bridge u_cfg_bridge (
    .HCLK           (HCLK),
    .HRESETn        (HRESETn),

    .HSEL           (CSR_HSEL),
    .HADDR          (CSR_HADDR),
    .HTRANS         (CSR_HTRANS),
    .HWRITE         (CSR_HWRITE),
    .HSIZE          (CSR_HSIZE),

    .HWDATA         (CSR_HWDATA),
    .HREADY         (CSR_HREADY),

    .HRDATA         (CSR_HRDATA),
    .HREADYOUT      (CSR_HREADYOUT),
    .HRESP          (CSR_HRESP),

    .req_ready      (cfg_req_ready),

    .req_valid      (cfg_req_valid),
    .req_type       (cfg_req_type),
    .req_bdf        (cfg_req_bdf),
    .req_reg        (cfg_req_reg),
    .req_wdata      (cfg_req_wdata),

    .cpl_valid      (cfg_cpl_valid),
    .cpl_status     (cfg_cpl_status),
    .cpl_rdata      (cfg_cpl_rdata),

    .cpl_ready      (cfg_cpl_ready)
);


/*
* H3 outbound MMIO frontend
*/
labh3_ahb_pcie_mmio_bridge #(
	.CPU_APERTURE_BASE (32'h6000_0000),
	.CPU_APERTURE_MASK (32'hF000_0000),
	.PCIE_BUS_BASE     (32'h6000_0000)
) u_mmio_bridge (
	.HCLK         (HCLK),
	.HRESETn      (HRESETn),

	.HSEL         (MMIO_HSEL),
	.HADDR        (MMIO_HADDR),
	.HTRANS       (MMIO_HTRANS),
	.HWRITE       (MMIO_HWRITE),
	.HSIZE        (MMIO_HSIZE),

	.HWDATA       (MMIO_HWDATA),
	.HREADY       (MMIO_HREADY),

	.HRDATA       (MMIO_HRDATA),
	.HREADYOUT    (MMIO_HREADYOUT),
	.HRESP        (MMIO_HRESP),

	.mem_req_ready (mem_req_ready),

	.mem_req_valid (mem_req_valid),
	.mem_req_write (mem_req_write),
	.mem_req_addr  (mem_req_addr),
	.mem_req_wdata (mem_req_wdata),

	.mem_cpl_valid (mem_cpl_valid),
	.mem_cpl_status (mem_cpl_status),
	.mem_cpl_rdata  (mem_cpl_rdata),

	.mem_cpl_ready (mem_cpl_ready)
);



/*
* Exactly one PCIe backend
*/
m3ds_pcie_backend u_backend (
	.clk         (HCLK),
	.resetn      (HRESETn),

	.req_valid   (cfg_req_valid),
	.req_type    (cfg_req_type),
	.req_bdf     (cfg_req_bdf),
	.req_reg     (cfg_req_reg),
        .req_wdata   (cfg_req_wdata),

	.req_ready   (cfg_req_ready),

	.cpl_ready   (cfg_cpl_ready),

	.cpl_valid   (cfg_cpl_valid),
	.cpl_status  (cfg_cpl_status),
	.cpl_rdata   (cfg_cpl_rdata),

	/* Memory*/
	.mem_req_valid (mem_req_valid),
	.mem_req_write (mem_req_write),
	.mem_req_addr  (mem_req_addr),
	.mem_req_wdata (mem_req_wdata),

	.mem_req_ready (mem_req_ready),

	.mem_cpl_ready (mem_cpl_ready),

	.mem_cpl_valid (mem_cpl_valid),
	.mem_cpl_status (mem_cpl_status),
	.mem_cpl_rdata  (mem_cpl_rdata)
);

endmodule
