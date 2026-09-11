`timescale 1ns/1ps

module labh2_pcie_endpoint_model (
    input wire            clk,
    input wire            resetn,

    input wire            req_valid,
    input wire [31:0]     req_dw0,
    input wire [31:0]     req_dw1,
    input wire [31:0]     req_dw2,
    input wire [31:0]     req_dw3,

    output wire           req_ready,

    input wire            cpl_ready,

    output reg            cpl_valid,
    output reg [31:0]     cpl_dw0,
    output reg [31:0]     cpl_dw1,
    output reg [31:0]     cpl_dw2,
    output reg [31:0]     cpl_dw3
);

localparam [7:0] TLP_CFG_READ        = 8'h01;
localparam [7:0] TLP_CFG_WRITE       = 8'h02;
localparam [7:0] TLP_CPL             = 8'h80;

localparam [1:0] CPL_SUCCESS         = 2'd0;
localparam [1:0] CPL_ERROR           = 2'd1;

/*
* Golden endpoint
*/
localparam [31:0] ENDPOINT_BDF = 32'h00000800;

/*
* Edcational writable config location
*/
localparam [9:0] CFG_SCRATCH   = 10'h040;

reg [31:0] cfg_scratch;

reg        pending;
reg [1:0]  delay_count;

reg [7:0]  saved_opcode;
reg [31:0] saved_bdf;
reg [9:0]  saved_reg;
reg [31:0] saved_wdata;

assign req_ready = !pending && !cpl_valid;

wire _unused_ok = &{1'b0, req_dw3, req_dw0[23:10]};

always @(posedge clk or negedge resetn) begin
	if (!resetn) begin
		cfg_scratch       <= 32'd0;

		pending           <= 1'b0;
		delay_count       <= 2'd0;

		saved_opcode      <= 8'd0;
		saved_bdf         <= 32'd0;
		saved_reg         <= 10'd0;
		saved_wdata       <= 32'd0;

		cpl_valid         <= 1'b0;
		cpl_dw0           <= 32'd0;
		cpl_dw1           <= 32'd0;
		cpl_dw2           <= 32'd0;
		cpl_dw3           <= 32'd0;
	
	end else begin
		if (req_valid && req_ready) begin
			saved_opcode <= req_dw0[31:24];
			saved_reg    <= req_dw0[9:0];
			saved_bdf    <= req_dw1;
			saved_wdata  <= req_dw2;

			pending      <= 1'b1;

			/* Educational transaction-layer latency */
			delay_count  <= 2'd2;

		end else if (pending) begin
			if (delay_count != 2'd0) begin
				delay_count <= delay_count - 1'b1;
			end else begin
				pending    <= 1'b0;

				cpl_valid  <= 1'b1;
				cpl_dw2    <= 32'd0;
				cpl_dw3    <= 32'd0;

				/* Configuration Read */
				if (saved_opcode == TLP_CFG_READ) begin
					cpl_dw0    <= {
					    TLP_CPL,
				            22'd0,
				            CPL_SUCCESS	    
					};

					/* Preserve H1 enumeration-visible behavior:
					*  absent function returns all ones
					*/
				        if (saved_bdf != ENDPOINT_BDF) begin
						cpl_dw1 <= 32'hFFFFFFFF;
					end else begin
						case (saved_reg)
							10'h000:
								cpl_dw1 <= 32'h56781234;

							CFG_SCRATCH:
								cpl_dw1 <= cfg_scratch;

							default:
								cpl_dw1 <= 32'hFFFFFFFF;
						endcase
					end
				end else if (saved_opcode == TLP_CFG_WRITE) begin
					cpl_dw0 <= {
					    TLP_CPL,
					    22'd0,
					    CPL_SUCCESS
					};

					cpl_dw1 <= 32'd0;

					if ((saved_bdf == ENDPOINT_BDF) && (saved_reg == CFG_SCRATCH)) begin
						cfg_scratch <= saved_wdata;
					end
				end else begin
					cpl_dw0 <= {
					    TLP_CPL,
					    22'd0,
					    CPL_ERROR
					};

					cpl_dw1 <= 32'hFFFFFFFF;
				end
			end
		end

		/* completion remains valid until accepted */
		if (cpl_valid && cpl_ready) begin 
			cpl_valid <= 1'b0;
		end
	end
end
endmodule
