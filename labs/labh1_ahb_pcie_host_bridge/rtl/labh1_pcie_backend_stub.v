`timescale 1ns / 1ps

module labh1_pcie_backend_stub(
    input wire          clk,
    input wire          resetn,

    input wire          req_valid,
    input wire [1:0]    req_type,
    input wire [31:0]   req_bdf,
    input wire [9:0]    req_reg,
    input wire [31:0]   req_wdata,

    output wire         req_ready,

    input wire          cpl_ready,

    output reg          cpl_valid,
    output reg [1:0]    cpl_status,
    output reg [31:0]   cpl_rdata
);

localparam [1:0] REQ_CFG_READ  = 2'd0;
localparam [1:0] REQ_CFG_WRITE = 2'd1;

/*
* Educational endpoint: BDF = 00:01.0
*/
localparam [31:0] ENDPOINT_BDF = 32'h00000800;

// Dummy wire to acknowledge unused signals
wire _unused_ok = &{1'b0, req_wdata};

reg          pending;
reg [1:0]    delay_count;

reg [1:0]    saved_type;
reg [31:0]   saved_bdf;
reg [9:0]    saved_reg;

assign req_ready = !pending && !cpl_valid;

always @(posedge clk or negedge resetn) begin
	if (!resetn) begin
		pending       <= 1'b0;
		delay_count   <= 2'd0;

		saved_type    <= 2'd0;
		saved_bdf     <= 32'd0;
		saved_reg     <= 10'd0;

		cpl_valid     <= 1'b0;
		cpl_status    <= 2'd0;
		cpl_rdata      <= 32'd0;

	end else begin
		/*
		* Completion handshake.
		*/
	        if (cpl_valid && cpl_ready) begin
			cpl_valid     <= 1'b0;
		end

		/*
		* Accept one outstanding request.
		*/
	        if (req_valid && req_ready) begin
			saved_type    <= req_type;
			saved_bdf     <= req_bdf;
			saved_reg     <= req_reg;

			pending       <= 1'b1;

			/*
			* Artificial backend latency.
			*/
		        delay_count   <= 2'd2;

		end else if (pending) begin
			if (delay_count != 2'd0) begin
				delay_count <= delay_count - 1'b1;
			end else begin
				pending     <= 1'b0;
				cpl_valid   <= 1'b1;

				/*
				* Transport-level success.
				*/
			        cpl_status  <= 2'd0;

				/*
				* Fake endpoint 00:01.0
				*/
			        if (saved_type == REQ_CFG_READ) begin

					/*
					* Configuration DWORD 0:
					* Device 0x5678 / Vendor 0x1234
					*/
				       if ((saved_bdf == ENDPOINT_BDF) && (saved_reg == 10'd0)) begin
					       cpl_rdata    <= 32'h56781234;
                                       end else begin
					       /*
					       * PCI convention for an absent
					       * function
					       */
					       cpl_rdata      <= 32'hFFFFFFFF;
				       end

			       end else if (saved_type == REQ_CFG_WRITE) begin 
				       /*
				       * H1 only proves request/completion
				       * flow.
				       */
				       cpl_rdata  <= 32'd0;

			       end else begin
				       cpl_status <= 2'd1;
				       cpl_rdata  <= 32'hFFFFFFFF;
			       end
		       end
	       end
       end
 end

 endmodule

