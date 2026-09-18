`timescale 1ns/1ps

/* verilator lint_off DECLFILENAME */
module labh3_pcie_vendor_adapter (
	input wire          host_clk,
	input wire          host_resetn,

	/* Host-side Configuration Request */
	input wire          cfg_req_valid,
	input wire [1:0]    cfg_req_type,
	input wire [31:0]   cfg_req_bdf,
	input wire [9:0]    cfg_req_reg,
	input wire [31:0]   cfg_req_wdata,

	output wire         cfg_req_ready,

	input wire          cfg_cpl_ready,

	output wire         cfg_cpl_valid,
	output wire [1:0]   cfg_cpl_status,
	output wire [31:0]  cfg_cpl_rdata,

	/* Host-side Memory Request */
	input wire          mem_req_valid,
	input wire          mem_req_write,
	input wire [31:0]   mem_req_addr,
	input wire [31:0]   mem_req_wdata,

	output wire         mem_req_ready,

	input wire          mem_cpl_ready,

	output wire         mem_cpl_valid,
	output wire [1:0]   mem_cpl_status,
	output wire [31:0]  mem_cpl_rdata
);

localparam [1:0] CFG_READ    = 2'd0;
localparam [1:0] CFG_WRITE   = 2'd1;

localparam [31:0] ENDPOINT_BDF = 32'h0000_0800;
localparam [31:0] DEVICE_ID = 32'h5678_1234;

localparam [9:0] CFG_VENDOR_DEVICE = 10'h000;

localparam [9:0] CFG_COMMAND_STATUS = 10'h004;

localparam [9:0] CFG_BAR0 = 10'h010;

localparam [31:0] BAR0_SIZE = 32'h0001_0000;
localparam [31:0] BAR0_MASK = 32'hFFFF_0000;

localparam [31:0] SCRATCH_OFFSET = 32'h0000_1000;

localparam [1:0] STATE_IDLE = 2'd0;
localparam [1:0] STATE_DELAY = 2'd1;
localparam [1:0] STATE_CPL = 2'd2;

localparam KIND_CFG = 1'b0;
localparam KIND_MEM = 1'b1;

reg [1:0] state;
reg       kind;

reg [1:0] delay_count;

reg [1:0] saved_cfg_type;
reg [31:0] saved_cfg_bdf;
reg [9:0] saved_cfg_reg;
reg [31:0] saved_cfg_wdata;

reg [31:0] saved_mem_addr;

reg [15:0] command_reg;

reg [31:0] bar0_base;
reg        bar0_probe;

reg [31:0] scratch_reg;

reg [1:0] cpl_status_r;
reg [31:0] cpl_rdata_r;

/* Configuration has arbitration priority */
assign cfg_req_ready = (state == STATE_IDLE);

assign mem_req_ready = (state == STATE_IDLE) && !cfg_req_valid;

/* Completion interfaces */
assign cfg_cpl_valid = (state == STATE_CPL) && (kind == KIND_CFG);

assign cfg_cpl_status = cpl_status_r;
assign cfg_cpl_rdata = cpl_rdata_r;

assign mem_cpl_valid = (state == STATE_CPL) && (kind == KIND_MEM);

assign mem_cpl_status = cpl_status_r;
assign mem_cpl_rdata = cpl_rdata_r;

always @(posedge host_clk or negedge host_resetn) begin
	if (!host_resetn) begin
		state           <= STATE_IDLE;
		kind            <= KIND_CFG;

		delay_count     <= 2'd0;

		saved_cfg_type  <= 2'd0;
		saved_cfg_bdf   <= 32'd0;
		saved_cfg_reg   <= 10'd0;
		saved_cfg_wdata <= 32'd0;

		saved_mem_addr  <= 32'd0;

		command_reg     <= 16'd0;

		bar0_base       <= 32'd0;
		bar0_probe      <= 1'b0;

		scratch_reg     <= 32'd0;

		cpl_status_r    <= 2'd0;
		cpl_rdata_r     <= 32'd0;
	end else begin
		case (state)
			STATE_IDLE:
			begin
				/* Configuration request */
				if (cfg_req_valid && cfg_req_ready) begin
					state            <= STATE_DELAY;
					kind             <= KIND_CFG;
					
					delay_count      <= 2'd2;
					
					saved_cfg_type   <= cfg_req_type;
					saved_cfg_bdf    <= cfg_req_bdf;
					saved_cfg_reg    <= cfg_req_reg;
					saved_cfg_wdata  <= cfg_req_wdata;
				
				/* Memory request */
				end else if (mem_req_valid && mem_req_ready) begin
					if (mem_req_write) begin
						$display("MEM WRITE addr=%08x bar0=%08x scratch=%08x", mem_req_addr, bar0_base, SCRATCH_OFFSET);
						if ((command_reg[1] == 1'b1) && (mem_req_addr == (bar0_base + SCRATCH_OFFSET))) begin
							scratch_reg <= mem_req_wdata;
						end
					end else begin
						state          <= STATE_DELAY;
						kind           <= KIND_MEM;

						delay_count    <= 2'd2;

						saved_mem_addr <= mem_req_addr;
					end
				end
			end

			STATE_DELAY:
			begin
				if (delay_count != 2'd0) begin
					delay_count <= delay_count - 1'b1;
				end else begin
					cpl_status_r  <= 2'd0;
					cpl_rdata_r   <= 32'd0;

					/* Configuration Completion */
					if (kind == KIND_CFG) begin
						/* Absent function */
						if (saved_cfg_bdf != ENDPOINT_BDF) begin
							cpl_rdata_r <= 32'hFFFF_FFFF;

						end else if (saved_cfg_type == CFG_READ) begin
							case (saved_cfg_reg)
								CFG_VENDOR_DEVICE:
									cpl_rdata_r <= DEVICE_ID;

								CFG_COMMAND_STATUS:
									cpl_rdata_r <= {
									16'd0,
									command_reg
									};
									
								CFG_BAR0:
									cpl_rdata_r <= bar0_probe ? BAR0_MASK : bar0_base;

								default:
									cpl_rdata_r <= 32'h0000_0000;

							endcase

						end else if (saved_cfg_type == CFG_WRITE) begin
							case (saved_cfg_reg)
								CFG_COMMAND_STATUS:
								begin
									command_reg <= saved_cfg_wdata[15:0];
								end

								CFG_BAR0:
								begin
									if (saved_cfg_wdata == 32'hFFFF_FFFF) begin
										bar0_probe <= 1'b1;
									end else begin
										bar0_base <= saved_cfg_wdata & BAR0_MASK;

										bar0_probe <= 1'b0;
									end
								end

								default:
								begin
								end

							endcase
						end else begin
							cpl_status_r <= 2'd1;
						end

					/* Memory Read Completion */
					end else begin
						$display("MEM WRITE addr=%08x bar0=%08x scratch=%08x", saved_mem_addr, bar0_base, SCRATCH_OFFSET);
						if ((command_reg[1] == 1'b1) && (saved_mem_addr == (bar0_base + SCRATCH_OFFSET))) begin
							cpl_status_r <= 2'd0;
							cpl_rdata_r  <= scratch_reg;
						end else begin
							cpl_status_r <= 2'd1;
							cpl_rdata_r  <= 32'hFFFF_FFFF;
						end
					end
					state <= STATE_CPL;
				end
			end

			STATE_CPL:
			begin
				if ((kind == KIND_CFG && cfg_cpl_valid && cfg_cpl_ready) ||
				    (kind == KIND_MEM && mem_cpl_valid && mem_cpl_ready)) begin
				    	state <= STATE_IDLE;
				end
			end

			default:
			begin
				state <= STATE_IDLE;
			end
		endcase
	end
end

endmodule
/* verilator lint_on DECLFILENAME */





