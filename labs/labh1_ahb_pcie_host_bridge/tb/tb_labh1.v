`timescale 1ns/1ps

module tb_labh1;

localparam [31:0]         BASE = 32'hA0000000;

localparam [31:0]         REG_VERSION     = BASE + 32'h0000;
localparam [31:0]         REG_CONTROL     = BASE + 32'h0004;
localparam [31:0]         REG_STATUS      = BASE + 32'h0008;

localparam [31:0]         REG_CFG_BDF     = BASE + 32'h0010;
localparam [31:0]         REG_CFG_REG     = BASE + 32'h0014;
localparam [31:0]         REG_CFG_RDATA   = BASE + 32'h001C;
localparam [31:0]         REG_CFG_COMMAND = BASE + 32'h0020;

reg HCLK;
reg HRESETn;

reg HSEL;
reg HREADY;

reg [31:0] HADDR;
reg [1:0]  HTRANS;
reg        HWRITE;
reg [2:0]  HSIZE;
reg [31:0] HWDATA;

wire [31:0] HRDATA;
wire        HREADYOUT;
wire        HRESP;

wire        req_valid;
wire        req_ready;
wire [1:0]  req_type;
wire [31:0] req_bdf;
wire [9:0]  req_reg;
wire [31:0] req_wdata;


wire        cpl_valid;
wire        cpl_ready;
wire [1:0]  cpl_status;
wire [31:0] cpl_rdata;

reg [31:0]  value;
reg [31:0]  status;

integer timeout;

labh1_ahb_pcie_host_bridge u_bridge (
    .HCLK         (HCLK),
    .HRESETn      (HRESETn),

    .HSEL         (HSEL),
    .HREADY       (HREADY),
    
    .HADDR        (HADDR),
    .HTRANS       (HTRANS),
    .HWRITE       (HWRITE),
    .HSIZE        (HSIZE),
    .HWDATA       (HWDATA),

    .HRDATA       (HRDATA),
    .HREADYOUT    (HREADYOUT),
    .HRESP        (HRESP),

    .req_valid    (req_valid),
    .req_ready    (req_ready),

    .req_type     (req_type),
    .req_bdf      (req_bdf),
    .req_reg      (req_reg),
    .req_wdata    (req_wdata),

    .cpl_valid    (cpl_valid),
    .cpl_ready    (cpl_ready),

    .cpl_status   (cpl_status),
    .cpl_rdata    (cpl_rdata)    
);

labh1_pcie_backend_stub u_backend (
    .clk          (HCLK),
    .resetn       (HRESETn),

    .req_valid    (req_valid),
    .req_ready    (req_ready),

    .req_type     (req_type),
    .req_bdf      (req_bdf),
    .req_reg      (req_reg),
    .req_wdata    (req_wdata),

    .cpl_valid    (cpl_valid),
    .cpl_ready    (cpl_ready),

    .cpl_status   (cpl_status),
    .cpl_rdata    (cpl_rdata)
);

always #5 HCLK = ~HCLK;

task ahb_write32;
	input [31:0] addr;
	input [31:0] data;
	
	begin
		/*
		* Address phase.
		*/
	        @(negedge HCLK);
	        HSEL     = 1'b1;
		HADDR    = addr;
		HTRANS   = 2'b10;
		HWRITE   = 1'b1;
		HSIZE    = 3'b010;

		/*
		* HWDATA belongs to the following data phase.
		*/
	        HWDATA   = data;

		@(negedge HCLK);
		HSEL     = 1'b0;
		HTRANS   = 2'b00;
		HWRITE   = 1'b0;

		/*
		* Keep write data valid until write data phase has been
		* sampled
		*/
	        HWDATA = data;

		@(negedge HCLK);
		HWDATA = 32'd0;
	end
endtask

task ahb_read32;
	input [31:0]  addr;
	output [31:0] data;

	begin
		@(negedge HCLK);
		HSEL     = 1'b1;
		HADDR    = addr;
		HTRANS   = 2'b10;
		HWRITE   = 1'b0;
		HSIZE    = 3'b010;

		/*
		* Address was sampled at the next rising edge.
		*/
	        @(negedge HCLK);
		data    = HRDATA;
		HSEL    = 1'b0;
		HTRANS  = 2'b00;

	end
endtask

initial begin
	$dumpfile("build/labh1.vcd");
	$dumpvars(0, tb_labh1);

	HCLK     = 1'b0;
	HRESETn  = 1'b0;

	HSEL     = 1'b0;
	HREADY   = 1'b1;

	HADDR    = 32'b0;
	HTRANS   = 2'b00;
	HWRITE   = 1'b0;
	HSIZE    = 3'b010;
	HWDATA   = 32'd0;

	repeat (4)
	    @(negedge HCLK);

	HRESETn = 1'b1;

	/*
	* Stage 1. read VERSION
	*/
        ahb_read32(REG_VERSION, value);

	if (value != 32'h00010000) begin
		$display("LABH1 FAIL: VERSION=%08x", value);

		$finish;
	end

	/*
	* Enable bridge
	*/
        ahb_write32(REG_CONTROL, 32'h00000001);

	/*
	* Endpoint = 00:01.0
	*/
        ahb_write32(REG_CFG_BDF, 32'h00000800);

	/*
	* Config DWORD 0.
	*/
        ahb_write32(REG_CFG_REG, 32'h00000000);

	/*
	* Launch CFG_READ.
	*/
        ahb_write32(REG_CFG_COMMAND, 32'h00000001);

	/*
	* Poll DONE
	*/
        timeout = 0;
	status = 32'd0;

	while (((status & 32'h2) == 0) && (timeout < 32)) begin
		ahb_read32(REG_STATUS, status);

		timeout = timeout +1;
	end

	if ((status & 32'h2) == 0) begin
		$display("LABH1 FAIL: timeout status=%08x", status);

		$finish;
	end

	if ((status & 32'h4) != 0) begin
		$display("LABH1 FAIL: backend error status=%08x", status);

		$finish;
	end

	/*
	* Read completion data.
	*/
	ahb_read32(REG_CFG_RDATA, value);

	if (value != 32'h56781234) begin
		$display("LABH1 FAIL: CFG_RDATA=%08x", value);

		$finish;
	end

	$display("LABH1 PASS: CG_READ 00:01.0 -> %08x", value);

        $finish;
end

endmodule

