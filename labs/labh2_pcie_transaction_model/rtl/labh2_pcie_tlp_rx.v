`timescale 1ns/1ps

module labh2_pcie_tlp_rx (
    input wire         tlp_valid,
    input wire [31:0]  tlp_dw0,
    input wire [31:0]  tlp_dw1,
    input wire [31:0]  tlp_dw2,
    input wire [31:0]  tlp_dw3,

    output wire        tlp_ready,

    input wire         cpl_ready,

    output wire        cpl_valid,
    output wire [1:0]  cpl_status,
    output wire [31:0] cpl_rdata 
);

localparam [7:0] TLP_CPL          = 8'h80;
localparam [1:0] CPL_ERROR        = 2'd1;

wire valid_completion;

assign valid_completion = (tlp_dw0[31:24] == TLP_CPL);

wire _unused_ok = &{1'b0, tlp_dw0[23:2], tlp_dw2, tlp_dw3};

/*
* Backpressure propagates back to endpoint
*/
assign tlp_ready = cpl_ready;

assign cpl_valid = tlp_valid;
assign cpl_status = valid_completion ? tlp_dw0[1:0] : CPL_ERROR;
assign cpl_rdata = tlp_dw1;

endmodule
