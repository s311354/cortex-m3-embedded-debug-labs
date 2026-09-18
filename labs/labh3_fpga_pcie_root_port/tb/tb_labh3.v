`timescale 1ns/1ps

module tb_labh3;

/*
* Address map
*/
localparam [31:0] CSR_BASE    = 32'hA000_0000;

localparam [31:0] MMIO_BASE   = 32'h6000_0000;

localparam [31:0] REG_VERSION     = CSR_BASE + 32'h0000;
localparam [31:0] REG_CONTROL     = CSR_BASE + 32'h0004;
localparam [31:0] REG_STATUS      = CSR_BASE + 32'h0008;
localparam [31:0] REG_CFG_BDF     = CSR_BASE + 32'h0010;
localparam [31:0] REG_CFG_REG     = CSR_BASE + 32'h0014;
localparam [31:0] REG_CFG_WDATA   = CSR_BASE + 32'h0018;
localparam [31:0] REG_CFG_RDATA   = CSR_BASE + 32'h001C;
localparam [31:0] REG_CFG_COMMAND = CSR_BASE + 32'h0020;

localparam [31:0] CMD_CFG_READ    = 32'h0000_0001;
localparam [31:0] CMD_CFG_WRITE   = 32'h0000_0002;

localparam [31:0] ENDPOINT_BDF  = 32'h0000_0800;
localparam [31:0] ABSENT_BDF    = 32'h0000_1000;

localparam [31:0] MMIO_SCRATCH  = MMIO_BASE + 32'h0000_1000;

localparam [31:0] TEST_VALUE    = 32'hA5A5_5A5A;

/*
* Clock/reset
*/
reg HCLK;
reg HRESETn;

/*
* TARGEXP0 / CSR bus
*/
reg          CSR_HSEL;
reg [31:0]   CSR_HADDR;
reg [1:0]    CSR_HTRANS;
reg          CSR_HWRITE;
reg [2:0]    CSR_HSIZE;
reg [31:0]   CSR_HWDATA;

wire [31:0]  CSR_HRDATA;
wire         CSR_HREADYOUT;
wire         CSR_HRESP;

/*
* TARGEXP1 / MMIO bus
*/
reg          MMIO_HSEL;
reg [31:0]   MMIO_HADDR;
reg [1:0]    MMIO_HTRANS;
reg          MMIO_HWRITE;
reg [2:0]    MMIO_HSIZE;
reg [31:0]   MMIO_HWDATA;

wire [31:0]  MMIO_HRDATA;
wire         MMIO_HREADYOUT;
wire         MMIO_HRESP;

/*
* Standlone bus HREADY
*/
wire CSR_HREADY = CSR_HREADYOUT;

wire MMIO_HREADY = MMIO_HREADYOUT;

reg [31:0] value;
reg [31:0] status;

integer mem_write_count;
integer mem_read_count;
integer mem_completion_count;

/*
* DUT
*/
labh3_pcie_root_port_wrapper dut (
    .HCLK           (HCLK),
    .HRESETn        (HRESETn),

    .CSR_HSEL       (CSR_HSEL),
    .CSR_HADDR      (CSR_HADDR),
    .CSR_HTRANS     (CSR_HTRANS),
    .CSR_HWRITE     (CSR_HWRITE),
    .CSR_HSIZE      (CSR_HSIZE),
    .CSR_HWDATA     (CSR_HWDATA),
    .CSR_HREADY     (CSR_HREADY),

    .CSR_HRDATA     (CSR_HRDATA),
    .CSR_HREADYOUT  (CSR_HREADYOUT),
    .CSR_HRESP      (CSR_HRESP),

    .MMIO_HSEL      (MMIO_HSEL),
    .MMIO_HADDR     (MMIO_HADDR),
    .MMIO_HTRANS    (MMIO_HTRANS),
    .MMIO_HWRITE    (MMIO_HWRITE),
    .MMIO_HSIZE     (MMIO_HSIZE),
    .MMIO_HWDATA    (MMIO_HWDATA),
    .MMIO_HREADY    (MMIO_HREADY),

    .MMIO_HRDATA    (MMIO_HRDATA),
    .MMIO_HREADYOUT (MMIO_HREADYOUT),
    .MMIO_HRESP     (MMIO_HRESP)
);

always #5 HCLK = ~HCLK;

/*
 * Transaction monitor
 */
always @(posedge HCLK) begin
    if (!HRESETn) begin
	    mem_write_count      <= 0;
	    mem_read_count       <= 0;
	    mem_completion_count <= 0;

    end else begin
	    if (dut.mem_req_valid && dut.mem_req_ready) begin
		    if (dut.mem_req_addr != 32'h6000_1000) begin
			    if (dut.mem_req_write) begin
				    $display("FAIL: MEM_WRITE address = %08x, expected 60001000", dut.mem_req_addr);
			    end else begin
				    $display("FAIL: MEM_READ address = %08x, expected 60001000", dut.mem_req_addr);
			    end
			    $fatal;
		    end

		    if (dut.mem_req_write)
			    mem_write_count <= mem_write_count + 1;
		    else
			    mem_read_count <= mem_read_count + 1;
	    end

	    if (dut.mem_cpl_valid && dut.mem_cpl_ready) begin
		    mem_completion_count <= mem_completion_count + 1;
	    end
    end			    
end


/*
* CSR AHB helpers
*/
task csr_write32;
	input [31:0] addr;
	input [31:0] data;

	begin
		@(negedge HCLK);

		CSR_HSEL     = 1'b1;
		CSR_HADDR    = addr;
		CSR_HTRANS   = 2'b10;
		CSR_HWRITE   = 1'b1;
		CSR_HSIZE    = 3'b010;

		@(posedge HCLK);

		@(negedge HCLK);

		CSR_HSEL     = 1'b0;
		CSR_HTRANS   = 2'b00;
		CSR_HWRITE   = 1'b0;

		CSR_HWDATA   = data;

		@(posedge HCLK);

		@(negedge HCLK);

		CSR_HWDATA = 32'd0;
	end
endtask

task csr_read32;
	input [31:0] addr;
	output [31:0] data;

	begin
		@(negedge HCLK);

		CSR_HSEL    = 1'b1;
		CSR_HADDR   = addr;
		CSR_HTRANS  = 2'b10;
		CSR_HWRITE  = 1'b0;
		CSR_HSIZE   = 3'b010;

		@(posedge HCLK);

		@(negedge HCLK);

		data = CSR_HRDATA;

		CSR_HSEL    = 1'b0;
		CSR_HTRANS  = 2'b00;

	end
endtask

task cfg_wait_done;
	output [31:0] final_status;

	integer timeout;
	reg [31:0] current;

	begin
		timeout = 0;
		current = 0;

		while (((current & 32'h2) == 0) && (timeout < 128)) begin
			csr_read32(REG_STATUS, current);

			timeout = timeout + 1;
		end

		final_status = current;

		if ((current & 32'h2) == 0) begin
			$display("LABH3 FAIL: CFG error STATUS=%08x", current);
			$fatal;
		end
	end
endtask

task cfg_read32;
	input [31:0] bdf;
	input [31:0] reg_addr;
	output [31:0] data;

	begin
		csr_write32(REG_CFG_BDF, bdf);
		csr_write32(REG_CFG_REG, reg_addr);

		csr_write32(REG_CFG_COMMAND, CMD_CFG_READ);

		cfg_wait_done(status);

		csr_read32(REG_CFG_RDATA, data);

	end
endtask

task cfg_write32;
	input [31:0] bdf;
	input [31:0] reg_addr;
	input [31:0] data;

	begin
		csr_write32(REG_CFG_BDF, bdf);
		csr_write32(REG_CFG_REG, reg_addr);
		csr_write32(REG_CFG_WDATA, data);

		csr_write32(REG_CFG_COMMAND, CMD_CFG_WRITE);

		cfg_wait_done(status);
	end
endtask

/*
* H3 MMIO helpers
*/
task mmio_write32;
	input [31:0] addr;
	input [31:0] data;

	integer wait_cycles;

	begin
		wait_cycles = 0;

		/* Address phase */
		@(negedge HCLK);

		MMIO_HSEL    = 1'b1;
		MMIO_HADDR   = addr;
		MMIO_HTRANS  = 2'b10;
		MMIO_HWRITE  = 1'b1;
		MMIO_HSIZE   = 3'b010;

		@(posedge HCLK);

		/* Data phase */
		@(negedge HCLK);

		MMIO_HSEL    = 1'b0;
		MMIO_HTRANS  = 2'b00;
		MMIO_HWRITE  = 1'b0;

		MMIO_HWDATA  = data;

		/* Wait for posted write acceptance */
		while (MMIO_HREADYOUT !== 1'b1) begin
			@(posedge HCLK);
			wait_cycles = wait_cycles + 1;
		end

		@(negedge HCLK);

		MMIO_HWDATA = 32'd0;

		if (MMIO_HRESP !== 1'b0) begin
			$display("LABH3 FAIL: MMIO WRITE HRESP");
			$fatal;
		end
	end
endtask

task mmio_read32;
	input [31:0] addr;
	output [31:0] data;

	integer wait_cycles;

	begin
		wait_cycles = 0;

		/* Address phase */
		@(negedge HCLK);

		MMIO_HSEL    = 1'b1;
		MMIO_HADDR   = addr;
		MMIO_HTRANS  = 2'b10;
		MMIO_HWRITE  = 1'b0;
		MMIO_HSIZE   = 3'b010;

		@(posedge HCLK);

		@(negedge HCLK);

		MMIO_HSEL   = 1'b0;
		MMIO_HTRANS = 2'b00;

		/* Memory Read must stall until completion */
		while (MMIO_HREADYOUT !== 1'b1) begin
			@(posedge HCLK);
			wait_cycles = wait_cycles + 1;
		end

		@(negedge HCLK);

		data = MMIO_HRDATA;

		if (MMIO_HRESP !== 1'b0) begin
			$display("LABH3 FAIL: MMIO READ HRESP");
			$fatal;
		end

		if (wait_cycles == 0) begin
			$display("LABH3 FAIL: MMIO read did not stall");
			$fatal;
		end
	end
endtask

/*
* Test sequence
*/
initial begin
	$dumpfile("build/labh3.vcd");

	$dumpvars(1, tb_labh3);

	$dumpvars(0, dut.u_cfg_bridge);

	$dumpvars(0, dut.u_mmio_bridge);

	$dumpvars(0, dut.u_backend);

	HCLK    = 1'b0;
	HRESETn = 1'b0;

	CSR_HSEL      = 1'b0;
	CSR_HADDR     = 32'b0;
	CSR_HTRANS    = 2'b0;
	CSR_HWRITE    = 1'b0;
	CSR_HSIZE     = 3'b010;
	CSR_HWDATA    = 32'd0;

	MMIO_HSEL     = 1'b0;
	MMIO_HADDR    = 32'd0;
	MMIO_HTRANS   = 2'b00;
	MMIO_HWRITE   = 1'b0;
	MMIO_HSIZE    = 3'b010;
	MMIO_HWDATA   = 32'd0;

	repeat (4)
	    @(negedge HCLK);

	HRESETn = 1'b1;

	/* Test 1. Host Controller CSR compatibility */
	csr_read32(REG_VERSION, value);

	if (value != 32'h0001_0000) begin
		$display("LABH3 FAIL: VERSION=%08x", value);
		$fatal;
	end

	csr_write32(REG_CONTROL, 32'h0000_0001);

	/* Test 2. Configuration Read. */
	cfg_read32(ENDPOINT_BDF, 32'h000, value);

	if (value != 32'h5678_1234) begin
		$display("LABH3 FAIL: ID=%08x", value);
		$fatal;
	end

	$display("LABH3 PASS: CFG_READ -> %08x", value);

	/* Test 3. Absent device compatibility */
	cfg_read32(ABSENT_BDF, 32'h000, value);

	if (value != 32'hFFFF_FFFF) begin
		$display("LABH3 FAIL: absent BDF=%08x", value);
		$fatal;
	end

	/* Test 4. BAR0 size probe */
	cfg_write32(ENDPOINT_BDF, 32'h010, 32'hFFFF_FFFF);

	cfg_read32(ENDPOINT_BDF, 32'h010, value);

	if (value != 32'hFFFF_0000) begin
		$display("LABH3 FAIL: BAR mask=%08x", value);
		$fatal;
	end

	/* Test 5. program BAR0 */
	cfg_write32(ENDPOINT_BDF, 32'h010, MMIO_BASE);

	cfg_read32(ENDPOINT_BDF, 32'h010, value);

	if (value != MMIO_BASE) begin
		$display("LABH3 FAIL: BAR0=%08x", value);
		$fatal;
	end

	/* Test 6. Enable Memory Space */
	cfg_write32(ENDPOINT_BDF, 32'h004, 32'h0000_0002);

	/* Test 7. CPU STORE -> PCIe Memory Write */
	mmio_write32(MMIO_SCRATCH, TEST_VALUE);

	/* Memory write is posted */
	if (mem_write_count != 1) begin
		$display("LABH3 FAIL: MEM_WRITE count=%08x", mem_write_count);
		$fatal;
	end

	/* Test 8. CPU LOAD -> PCIe Memory Read */
	mmio_read32(MMIO_SCRATCH, value);

	if (value != TEST_VALUE) begin
		$display("LABH3 FAIL: MMIO readback=%08x", value);
		$fatal;
	end

	if (mem_read_count != 1) begin
		$display("LABH3 FAIL: MEM_READ count=%0d", mem_read_count);
		$fatal;
	end

	if (mem_completion_count != 1) begin
		$display("LABH3 FAIL: MEM completion count=%0d", mem_completion_count);
		$fatal;
	end

	$display("LABH3 PASS: MMIO WRITE/READBACK -> %08x", value);

	$display("LABH3 PASS: PCIe root-port frontend verified");

	#20;

	$finish;
end


endmodule
