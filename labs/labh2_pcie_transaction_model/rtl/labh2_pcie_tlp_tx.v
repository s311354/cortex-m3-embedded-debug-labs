`timescale 1ns/1ps

module labh2_pcie_tlp_tx (
    input wire                req_valid,
    input wire [1:0]          req_type,
    input wire [31:0]         req_bdf,
    input wire [9:0]          req_reg,
    input wire [31:0]         req_wdata,

    output wire               req_ready,

    input wire                tlp_ready,

    output wire               tlp_valid,
    output wire [31:0]        tlp_dw0,
    output wire [31:0]        tlp_dw1,
    output wire [31:0]        tlp_dw2,
    output wire [31:0]        tlp_dw3
);

localparam [1:0] REQ_CFG_READ        = 2'd0;
localparam [1:0] REQ_CFG_WRITE       = 2'd1;

localparam [7:0] TLP_CFG_READ        = 8'h01;
localparam [7:0] TLP_CFG_WRITE       = 8'h02;
localparam [7:0] TLP_INVALID         = 8'hFF;

wire [7:0] opcode;

assign opcode = (req_type == REQ_CFG_READ) ? TLP_CFG_READ : 
	        (req_type == REQ_CFG_WRITE) ? TLP_CFG_WRITE : TLP_INVALID;

/*
* Request handshake is preserved across TX
*/
assign req_ready = tlp_ready;

assign tlp_valid = req_valid;

assign tlp_dw0 = {
    opcode,
    14'd0,
    req_reg    
};

assign tlp_dw1 = req_bdf;
assign tlp_dw2 = req_wdata;
assign tlp_dw3 = 32'd0;

endmodule

