`timescale 1ns/1ps

module tb_labh2;

localparam [31:0] BASE = 32'hA0000000;

localparam [31:0] REG_VERSION     = BASE + 32'h0000;
localparam [31:0] REG_CONTROL     = BASE + 32'h0004;
localparam [31:0] REG_STATUS      = BASE + 32'h0008;
localparam [31:0] REG_CFG_BDF     = BASE + 32'h0010;
localparam [31:0] REG_CFG_REG     = BASE + 32'h0014;
localparam [31:0] REG_CFG_WDATA   = BASE + 32'h0018;
localparam [31:0] REG_CFG_RDATA   = BASE + 32'h001C;
localparam [31:0] REG_CFG_COMMAND = BASE + 32'h0020;

localparam [31:0] CMD_CFG_READ    = 32'h00000001;
localparam [31:0] CMD_CFG_WRITE   = 32'h00000002;

reg           HCLK;
reg           HRESETn;

reg           HSEL;
reg           HREADY;

reg [31:0]    HADDR;
reg [1:0]     HTRANS;
reg           HWRITE;
reg [2:0]     HSIZE;
reg [31:0]    HWDATA;

wire [31:0]   HRDATA;
wire          HREADYOUT;
wire          HRESP;

reg [31:0]    value;
reg [31:0]    status;

m3ds_pcie_host_wrapper dut (
    .HCLK       (HCLK),
    .HRESETn    (HRESETn),

    .HSEL       (HSEL),
    .HADDR      (HADDR),
    .HWRITE     (HWRITE),
    .HTRANS     (HTRANS),
    .HSIZE      (HSIZE),

    .HWDATA     (HWDATA),
    .HREADY     (HREADY),

    .HRDATA     (HRDATA),
    .HREADYOUT  (HREADYOUT),
    .HRESP      (HRESP)
);

always #5 HCLK = ~HCLK;

/*
 * AHB-Lite 32-bit write
 */
task ahb_write32;
	input [31:0] addr;
	input [31:0] data;

	begin
		@(negedge HCLK);
		HSEL     = 1'b1;
		HADDR    = addr;
		HTRANS   = 2'b10;
		HWRITE   = 1'b1;
		HSIZE    = 3'b010;

		/* Address phase sampled */
		@(posedge HCLK);

		@(negedge HCLK);

		HSEL     = 1'b0;
		HTRANS   = 2'b00;
		HWRITE   = 1'b0;

		/* Keep write data valid for data phase */
		HWDATA   = data;

		@(posedge HCLK);

		@(negedge HCLK);
		HWDATA   = 32'd0;
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

		@(posedge HCLK);

		@(negedge HCLK);

		data     = HRDATA;

		HSEL     = 1'b0;
		HTRANS   = 2'b00;
	end
endtask

task wait_done;
	output [31:0] final_status;

	integer timeout;
	reg [31:0] current;

	begin
		timeout = 0;
		current = 32'd0;

		while (((current & 32'h2) == 0) && (timeout < 64)) begin
			ahb_read32(REG_STATUS, current);

			timeout = timeout + 1;
		end

		final_status = current;

		if ((current & 32'h2) == 0) begin
			$display("LABH2 FAIL: completion timeout STATUS=%08x", current);
			$fatal;
		end
	end
endtask

/*
* H2 v1 CSR accesses remain zero wait state
*/
always @(posedge HCLK) begin
	if (HRESETn) begin
		if (HREADYOUT !== 1'b1) begin
			$display("LABH2 FAIL: unexpected AHB wait state");
			$fatal;
		end

		if (HRESP !== 1'b0) begin
			$display("LABH2 FAIL: unexpected HRESP");
			$fatal;
		end
	end
end

initial begin
	$dumpfile("build/labh2.vcd");
	// Focus on PCIe TLP transaction signals for Lab H2
	$dumpvars(0, dut.u_labh1_ahb_pcie_host_bridge);      // Host bridge (Lab H1)
	$dumpvars(1, dut.u_m3ds_pcie_backend);               // Backend interconnect wires (THIS LEVEL ONLY)
	$dumpvars(0, dut.u_m3ds_pcie_backend.u_tlp_tx);      // TLP transmit encoder
	$dumpvars(0, dut.u_m3ds_pcie_backend.u_endpoint);    // Endpoint model
	$dumpvars(0, dut.u_m3ds_pcie_backend.u_tlp_rx);      // TLP receive decoder
	// Include top-level AHB interface signals
	$dumpvars(1, tb_labh2);

	HCLK    = 1'b0;
	HRESETn = 1'b0;

	HSEL    = 1'b0;
	HREADY  = 1'b1;

	HADDR   = 32'b0;
	HTRANS  = 2'b0;
	HWRITE  = 1'b0;
	HSIZE   = 3'b010;
	HWDATA  = 32'd0;

	repeat (4)
	    @(negedge HCLK);

	HRESETn = 1'b1;

	/*
	* Test 1: Host Controller VERSION
	*/
        ahb_read32(REG_VERSION, value);

	if (value != 32'h00010000) begin
		$display("LABH2 FAIL: VERSION=%08x", value);
		$fatal;
	end

	/*
	* Enable Host Controller
	*/
        ahb_write32(REG_CONTROL, 32'h00000001);

	/*
	* Test 2: CFG_READ 00:01.0, offset 0x000
	*/
        ahb_write32(REG_CFG_BDF, 32'h00000800);

	ahb_write32(REG_CFG_REG, 32'h00000000);

	ahb_write32(REG_CFG_COMMAND, CMD_CFG_READ);

	wait_done(status);

	ahb_read32(REG_CFG_RDATA, value);

	if (value != 32'h56781234) begin
		$display("LABH2 FAIL: CFG_READ=%08x", value);
		$fatal;
	end

	$display("LABH2 PASS: CFG_READ 00:01.0 -> %08x", value);

	/*
	* Test 3: H2 CFG_WRITE to scratch config register
	*/
        ahb_write32(REG_CFG_REG, 32'h00000040);

	ahb_write32(REG_CFG_WDATA, 32'hA5A55A5A);

	ahb_write32(REG_CFG_COMMAND, CMD_CFG_WRITE);

	wait_done(status);

	/*
	* Read it back
	*/
        ahb_write32(REG_CFG_COMMAND, CMD_CFG_READ);

	wait_done(status);

	ahb_read32(REG_CFG_RDATA, value);

	if (value != 32'hA5A55A5A) begin
		$display("LABH2 FAIL: CFG_WRITE/READBACK=%08x", value);
		$fatal;
	end

	$display("LABH2 PASS: CFG_WRITE/READBACK -> %08x", value);

	/*
	* Test 4. absent endpoint 00:02.0
	*/
        ahb_write32(REG_CFG_BDF, 32'h00001000);

	ahb_write32(REG_CFG_REG, 32'h00000000);

	ahb_write32(REG_CFG_COMMAND, CMD_CFG_READ);

	wait_done(status);

	ahb_read32(REG_CFG_RDATA, value);

	if (value !== 32'hFFFFFFFF) begin
		$display("LABH2 FAIL: absent BDF=%08x", value);
		$fatal;
	end

	$display("LABH2 PASS: absetn BDF -> FFFFFFFF");

	$display("LABH2 PASS: PCIe transaction model verified");

	#20;
	$finish;
end

endmodule
	
