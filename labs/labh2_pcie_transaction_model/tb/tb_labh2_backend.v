`timescale 1ns/1ps

module tb_labh2_backend;

reg            clk;
reg            resetn;

reg            req_valid;
reg [1:0]      req_type;
reg [31:0]     req_bdf;
reg [9:0]      req_reg;
reg [31:0]     req_wdata;

wire           req_ready;

reg            cpl_ready;

wire           cpl_valid;
wire [1:0]     cpl_status;
wire [31:0]    cpl_rdata;

integer i;

m3ds_pcie_backend dut (
    .clk            (clk),
    .resetn         (resetn),

    .req_valid      (req_valid),
    .req_type       (req_type),
    .req_bdf        (req_bdf),
    .req_reg        (req_reg),
    .req_wdata      (req_wdata),

    .req_ready      (req_ready),
    
    .cpl_ready      (cpl_ready),
    
    .cpl_valid      (cpl_valid),
    .cpl_status     (cpl_status),
    .cpl_rdata      (cpl_rdata)
);

always #5 clk = ~clk;

task issue_request;
	input [1:0]      req_type_in;
	input [31:0]     bdf;
	input [9:0]      reg_offset;
	input [31:0]     wdata;

	begin
		while (!req_ready)
		    @(negedge clk);

		req_type  = req_type_in;
		req_bdf   = bdf;
		req_reg   = reg_offset;
		req_wdata = wdata;

		req_valid = 1'b1;

		/*
		* Inspect TX before request handshake
		*/
	        #1;

		if (dut.req_tlp_dw1 !== bdf) begin
			$display("LABH2 BACKEND FAIL: TLP BDF");
			$fatal();
		end

		if (dut.req_tlp_dw0[9:0] !== reg_offset) begin
			$display("LABH2 BACKEND FAIL: TLP REG");
			$fatal();
		end

		@(posedge clk);

		@(negedge clk);

		req_valid = 1'b0;
	end
endtask

initial begin
	$dumpfile("build/labh2_backend.vcd");
	$dumpvars(0, tb_labh2_backend);

	clk           = 1'b0;
	resetn        = 1'b0;

	req_valid     = 1'b0;
	req_type      = 2'd0;
	req_bdf       = 32'd0;
        req_reg       = 10'd0;
	req_wdata     = 32'd0;

	/* Intentionally block completion first */
        cpl_ready     = 1'b0;

	repeat (4)
	    @(negedge clk);

	resetn        = 1'b1;

	/* CFG_READ 00:01.0 / offset 0 */
	issue_request(2'd0,
	              32'h00000800,
	              10'h000,
	              32'd0);

	/* Request must encode as CFG_READ */
	if (dut.req_tlp_dw0[31:24] !== 8'h01) begin
		$display("LABH2 BACKEND FAIL: CFG_READ opcode");
		$fatal();
	end

	/* Wait for completion while cpl_read = 0 */
	while (!cpl_valid)
	    @(negedge clk);

	if (cpl_status !== 2'd0) begin
		$display("LABH2 BACKEND FAIL: completion status");
		$fatal();
	end

	if (cpl_rdata !== 32'h56781234) begin
		$display("LABH2 BACKEND FAIL: completion data=%08x", cpl_rdata);
		$fatal();
	end

	/* Backpressure requirement */
	for (i = 0; i < 3 ; i = i + 1) begin
		@(negedge clk);

		if (!cpl_valid) begin
			$display("LABH2 BACKEND FAIL: cpl_valid dropped");
			$fatal();
		end

		if (cpl_rdata !== 32'h56781234) begin
			$display("LABH2 BACKEND FAIL: cpl_rdata changed");
			$fatal();
		end
	end

	/* Accept completion */
	cpl_ready = 1'b1;

	@(posedge clk);
	@(negedge clk);

	if (cpl_valid) begin
		$display("LABH2 BACKEND FAIL: completion not consumed");
		$fatal();
	end

	/* Invalid abstract request */
	cpl_ready = 1'b1;

	issue_request(2'd3,
	              32'h00000800,
	              10'h000,
	              32'd0);

	while (!cpl_valid)
	    @(negedge clk);

	if (cpl_status == 2'd0) begin
		$display("LABH2 BACKEND FAIL: invalid request accepted");
		$fatal();
	end

	$display("LABH2 BACKEND PASS: TLP + handshake verified");

	#20;
	$finish;
end
endmodule


