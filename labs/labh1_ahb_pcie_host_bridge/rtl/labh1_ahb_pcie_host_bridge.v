module labh1_ahb_pcie_host_bridge (
    input wire HCLK,
    input wire HRESETn,

    /*
    * AHB-Lite slave interface
    */
    input wire         HSEL,
    input wire [31:0]  HADDR,
    input wire [1:0]   HTRANS,
    input wire         HWRITE,
    input wire [2:0]   HSIZE,

    input wire [31:0]  HWDATA,
    input wire         HREADY,

    output reg [31:0]  HRDATA,
    output wire        HREADYOUT,
    output wire        HRESP,

    /*
    * Abstract PCIe request interface. 
    */

    input  wire        req_ready,

    output wire        req_valid,
    output wire [1:0]  req_type,
    output wire [31:0] req_bdf,    
    output wire [9:0]  req_reg,
    output wire [31:0] req_wdata,

    /*
    * Abstract completion interface
    */
    input wire         cpl_valid,
    input wire [1:0]   cpl_status,
    input wire [31:0]  cpl_rdata,

    output wire        cpl_ready
);

/*
* CSR offsets.
*/
localparam [15:0]   REG_VERSION            = 16'h0000;
localparam [15:0]   REG_CONTROL            = 16'h0004;
localparam [15:0]   REG_STATUS             = 16'h0008;

localparam [15:0]   REG_CFG_BDF            = 16'h0010;
localparam [15:0]   REG_CFG_REG            = 16'h0014;
localparam [15:0]   REG_CFG_WDATA          = 16'h0018;
localparam [15:0]   REG_CFG_RDATA          = 16'h001C;
localparam [15:0]   REG_CFG_COMMAND        = 16'h0020;
localparam [15:0]   REG_ERROR_STATUS       = 16'h0024;

localparam [31:0]   VERSION_VALUE          = 32'h00010000;

/* CONTROL */
localparam [31:0]   CONTROL_ENABLE         = 32'h00000001;

/* commands */
localparam [31:0]   CMD_CFG_READ           = 32'h00000001;
localparam [31:0]   CMD_CFG_WRITE          = 32'h00000002;

/* backend request types */
localparam [1:0]    REQ_CFG_READ           = 2'd0;
localparam [1:0]    REQ_CFG_WRITE          = 2'd1;

/* error-status bits */
localparam [31:0]   ERR_DISABLED           = 32'h00000001;
localparam [31:0]   ERR_BAD_COMMAND        = 32'h00000002;
localparam [31:0]   ERR_BACKEND            = 32'h00000004;
localparam [31:0]   ERR_BUSY               = 32'h00000008;
localparam [31:0]   ERR_BAD_ACCESS         = 32'h00000010;

/* request FSM */
localparam [1:0]    STATE_IDLE             = 2'd0;
localparam [1:0]    STATE_ISSUE            = 2'd1;
localparam [1:0]    STATE_WAIT_CPL         = 2'd2;

// Dummy wire to acknowledge unused signal bits
wire _unused_ok = &{1'b0, HADDR[31:16], HTRANS[0]};

/*
* Programmer-visible registers
*/
reg [31:0]    control;

reg           busy;
reg           done;

reg [31:0]    cfg_bdf;
reg [31:0]    cfg_reg;
reg [31:0]    cfg_wdata;
reg [31:0]    cfg_rdata;
reg [31:0]    cfg_command;

reg [31:0]    error_status;

/*
* AHB data-phase information
*
* AHB address/control belongs to one phase.
*/
reg           dphase_valid;
reg           dphase_write;

reg [15:0]    dphase_addr;
reg [2:0]     dphase_size;

/*
* Backend request snapshot
*/
reg [1:0]     req_type_r;
reg [31:0]    req_bdf_r;
reg [9:0]     req_reg_r;
reg [31:0]    req_wdata_r;

reg [1:0]     state; // STATE_IDLE -> STATE_ISSUE -> STATE_WAIT_CPL -> STATE_IDLE

/*
* AHB
*/

wire ahb_address_phase;

assign ahb_address_phase = HSEL && HREADY && HTRANS[1];

/*
* labH1 CSR accesses themselves are zero-wait-state.
* 
* PCIe completion is asynchronous relative to the AHB register
* access and is observed by polling STATUS.
*/
assign HREADYOUT = 1'b1;

/*
* labH1 v1 only exercises valid, aligned 32-bit accesses.
*/
assign HRESP = 1'b0;

/*
* Backend
*/
assign req_valid = (state == STATE_ISSUE);

assign req_type  = req_type_r;
assign req_bdf   = req_bdf_r;
assign req_reg   = req_reg_r;
assign req_wdata = req_wdata_r;

assign cpl_ready = (state == STATE_WAIT_CPL);

/*
* AHB read data phase.
*/
always @(*) begin
	HRDATA = 32'h00000000;

	if (dphase_valid && !dphase_write) begin
		case (dphase_addr)

			REG_VERSION:
				HRDATA = VERSION_VALUE;

			REG_CONTROL:
				HRDATA = control;

			REG_STATUS:
				HRDATA = {
				    29'd0,
				    (error_status != 32'd0),
				    done,
				    busy
				};

			REG_CFG_BDF:
				HRDATA = cfg_bdf;

			REG_CFG_REG:
				HRDATA = cfg_reg;

			REG_CFG_WDATA:
				HRDATA = cfg_wdata;

			REG_CFG_RDATA:
				HRDATA = cfg_rdata;

			REG_CFG_COMMAND:
				HRDATA = cfg_command;

			REG_ERROR_STATUS:
				HRDATA = error_status;

			default:
				HRDATA = 32'h00000000;

		endcase

	end
end

/*
* Sequential state
*/
always @(posedge HCLK or negedge HRESETn) begin
	if (!HRESETn) begin
		state        <= STATE_IDLE;

		control      <= 32'd0;

		busy         <= 1'b0;
		done         <= 1'b0;

		cfg_bdf      <= 32'd0;
		cfg_reg      <= 32'd0;
		cfg_wdata    <= 32'd0;
		cfg_rdata    <= 32'd0;
		cfg_command  <= 32'd0;

		error_status <= 32'd0;

		dphase_valid <= 1'b0;
		dphase_write <= 1'b0;
		dphase_addr  <= 16'd0;
		dphase_size  <= 3'd0;

		req_type_r   <= 2'd0;
		req_bdf_r    <= 32'd0;
		req_reg_r    <= 10'd0;
		req_wdata_r  <= 32'd0;

	end else begin
	        /*
	        * Capture the current AHB address phase.
	        */
	        if (HREADY) begin
			dphase_valid <= ahb_address_phase;

			if (ahb_address_phase) begin
				dphase_write <= HWRITE;
				dphase_addr  <= HADDR[15:0];
				dphase_size  <= HSIZE;
			end
		end

		/*
		* Process the previous AHB write data phase.
		*/
	        if (dphase_valid && dphase_write) begin
			/*
			* H1 supports aligned 32-bit CSR accesses.
			*/
		        if ((dphase_size != 3'b010) || (dphase_addr[1:0] != 2'b00)) begin
				error_status <= error_status | ERR_BAD_ACCESS;
			end else begin
				case (dphase_addr)
					REG_CONTROL:
					begin
						control   <= HWDATA & CONTROL_ENABLE;
					end

					REG_CFG_BDF:
					begin
						cfg_bdf   <= HWDATA;
					end

					REG_CFG_REG:
					begin
						cfg_reg   <= HWDATA;
					end

					REG_CFG_WDATA:
					begin
						cfg_wdata <= HWDATA;
					end

					REG_CFG_COMMAND:
					begin
						cfg_command <= HWDATA;

						/*
						* A newly accepted command
						* invalidates the previous
						* DONE indication immediately.
						*/
						if ((busy != 1'b0) || (state != STATE_IDLE)) begin
							error_status <= error_status | ERR_BUSY;

						end else if ((HWDATA == CMD_CFG_READ) || (HWDATA == CMD_CFG_WRITE))  begin
							state         <= STATE_ISSUE;

							busy         <= 1'b1;
							done         <= 1'b0;

							error_status <= 32'd0;

							if (HWDATA == CMD_CFG_READ) begin
								req_type_r <= REQ_CFG_READ;

							end else begin
								req_type_r <= REQ_CFG_WRITE;
							end

							req_bdf_r    <= cfg_bdf;

							req_reg_r    <= cfg_reg[9:0];

							req_wdata_r  <= cfg_wdata;

						end else begin
							busy          <= 1'b0;
							done          <= 1'b1;

							error_status  <= ERR_BAD_COMMAND;
						end
					end

					default:
					begin
						error_status <= error_status | ERR_BAD_ACCESS;
					end
				endcase
			end
		end

		/*
		* Backend transaction FSM
		*/
	        case (state)
			STATE_IDLE:
			begin
			end

			STATE_ISSUE:
			begin
				if (req_ready) begin
					state <= STATE_WAIT_CPL;
				end
			end

			STATE_WAIT_CPL:
			begin
				if (cpl_valid) begin
					state    <= STATE_IDLE;

					busy    <= 1'b0;
					done    <= 1'b1;

					if (cpl_status != 2'd0) begin
						error_status       <= ERR_BACKEND;
					end else begin
						error_status       <= 32'd0;

						if (req_type_r == REQ_CFG_READ) begin
							cfg_rdata  <= cpl_rdata;
						end
					end
				end
			end

			default:
	                begin
				state    <= STATE_IDLE;
				busy     <= 1'b0;
			end
		endcase
	end
end

endmodule
