`timescale 1ns/1ps

module labh3_ahb_pcie_mmio_bridge #(
	parameter [31:0] CPU_APERTURE_BASE = 32'h6000_0000,
	parameter [31:0] CPU_APERTURE_MASK = 32'hF000_0000,
	parameter [31:0] PCIE_BUS_BASE     = 32'h6000_0000
)(
	input wire              HCLK,
	input wire              HRESETn,

	input wire              HSEL,
	input wire [31:0]       HADDR,
	input wire [1:0]        HTRANS,
	input wire              HWRITE,
	input wire [2:0]        HSIZE,

	input wire [31:0]       HWDATA,
	input wire              HREADY,

	output wire [31:0]      HRDATA,
	output wire             HREADYOUT,
	output wire             HRESP,

	/*
	* PCIe Memory Request
	*/
	input wire              mem_req_ready,

	output wire             mem_req_valid,
	output wire             mem_req_write,
	output wire [31:0]      mem_req_addr,
	output wire [31:0]      mem_req_wdata,

	/*
	* PCIe Memory Read Completion
	*/
	input wire              mem_cpl_valid,
        input wire [1:0]        mem_cpl_status,
        input wire [31:0]       mem_cpl_rdata,

	output wire             mem_cpl_ready
);

localparam [2:0] STATE_IDLE   = 3'd0;
localparam [2:0] STATE_DPHASE = 3'd1;
localparam [2:0] STATE_ISSUE  = 3'd2;
localparam [2:0] STATE_WAIT   = 3'd3;
localparam [2:0] STATE_RESP   = 3'd4;
localparam [2:0] STATE_ERR1   = 3'd5;
localparam [2:0] STATE_ERR2   = 3'd6;

reg [2:0]  state;

reg [31:0] addr_r;
reg        write_r;
reg [2:0]  size_r;

reg [31:0] wdata_r;
reg [31:0] rdata_r;

wire address_phase;

assign address_phase = HSEL && HREADY && HTRANS[1];

wire _unused_ok = &{1'b0, HTRANS[0]};

/*
* Normal transfer completes STATE_RESP
*
* AHB ERROR response:
*
*     ERR1: HRESP = 1, HREADYOUT = 0
*     ERR2: HRESP = 1, HREADYOUT = 1
*/
assign HREADYOUT = (state == STATE_IDLE) || (state == STATE_RESP) || (state == STATE_ERR2);

assign HRESP = (state == STATE_ERR1) || (state == STATE_ERR2);

assign HRDATA = rdata_r;

/*
* Memory request must remain stable while valid = 1, ready = 0
*/
assign mem_req_valid = (state == STATE_ISSUE);

assign mem_req_write = write_r;

assign mem_req_addr = PCIE_BUS_BASE + (addr_r - CPU_APERTURE_BASE);

assign mem_req_wdata = wdata_r;

/*
* Only Memory Read expects a completion
*/
assign mem_cpl_ready = (state == STATE_WAIT);

always @(posedge HCLK or negedge HRESETn) begin
	if (!HRESETn) begin
		state    <= STATE_IDLE;

		addr_r   <= 32'd0;
		write_r  <= 1'b0;
		size_r   <= 3'd0;

		wdata_r  <= 32'd0;
		rdata_r  <= 32'd0;
	end else begin
		case (state)
			/*
			* Address phase
			*/
			STATE_IDLE:
			begin
				if (address_phase) begin
					state    <= STATE_DPHASE;
					
					addr_r   <= HADDR;
					write_r  <= HWRITE;
					size_r   <= HSIZE;
				end
			end
			/*
			* Data phase
			*/
			STATE_DPHASE:
			begin
				if ((size_r != 3'b010) || 
				    (addr_r[1:0] != 2'b00) || 
				    ((addr_r & CPU_APERTURE_MASK) != CPU_APERTURE_BASE)) begin
				        state <= STATE_ERR1;

			        end else begin
					state <= STATE_ISSUE;

					if (write_r)
						wdata_r <= HWDATA;
				end
			end
			/*
			* Backend request
			*/
		       STATE_ISSUE:
		       begin
			       if (mem_req_ready) begin
				     if (write_r) begin
					      /*
					      * PCIe Memory write is posted
					      *
					      * The AHB transfer can finish
					      * when the backend has safely
					      * accepted the request.
					      */
					      state <= STATE_RESP;
				      end else begin
					      /*
					      * Memory Read is non-posted
					      */
			  		      state <= STATE_WAIT;
				      end
			      end
		      end

		      /*
		      * Wait for memory Read completion
		      */
		      STATE_WAIT:
		      begin
		 	     if (mem_cpl_valid) begin
				     if (mem_cpl_status != 2'd0) begin
					     state <= STATE_ERR1;
				     end else begin
					     state <= STATE_RESP;
					     rdata_r <= mem_cpl_rdata;
				     end
			     end
		      end

		      /*
		      * Successful transfer completion
		      */
		      STATE_RESP:
		      begin
			    /*
			    * Support a back-to-back transfer after the
			    * previously stalled transfer completes
			    */
			   if (address_phase) begin
				   state   <= STATE_DPHASE;

				   addr_r  <= HADDR;
				   write_r <= HWRITE;
				   size_r  <= HSIZE;
			   end else begin
				   state   <= STATE_IDLE;
			   end
		     end

		     /*
		     * Two-cyle AHB ERROR
		     */
		     STATE_ERR1:
		     begin
			  state <= STATE_ERR2;
		     end

		     STATE_ERR2:
		     begin
			  state <= STATE_IDLE;
		     end

		     default:
		     begin
			  state <= STATE_IDLE;
		     end
	     endcase
     end

end
endmodule





