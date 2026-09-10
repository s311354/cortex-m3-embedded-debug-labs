`timescale 1ns/1ps

module m3ds_pcie_backend (
    input wire           clk,
    input wire           resetn,

    input wire           req_valid,
    input wire [1:0]     req_type,
    input wire [31:0]    req_bdf,
    input wire [9:0]     req_reg,
    input wire [31:0]    req_wdata,

    output wire          req_ready,

    input wire           cpl_ready,

    output wire          cpl_valid,
    output wire [1:0]    cpl_status,
    output wire [31:0]   cpl_rdata
);

/*
* Request TLP channel
*/
wire                req_tlp_valid;
wire                req_tlp_ready;

wire [31:0]         req_tlp_dw0;
wire [31:0]         req_tlp_dw1;
wire [31:0]         req_tlp_dw2;
wire [31:0]         req_tlp_dw3;

/*
* Completion TLP channel
*/
wire                cpl_tlp_valid;
wire                cpl_tlp_ready;

wire [31:0]         cpl_tlp_dw0;
wire [31:0]         cpl_tlp_dw1;
wire [31:0]         cpl_tlp_dw2;
wire [31:0]         cpl_tlp_dw3;

labh2_pcie_tlp_tx u_tlp_tx (
    .req_valid (req_valid),
    .req_ready (req_ready),

    .req_type  (req_type),
    .req_bdf   (req_bdf),
    .req_reg   (req_reg),
    .req_wdata (req_wdata),

    .tlp_valid (req_tlp_valid),
    .tlp_ready (req_tlp_ready),

    .tlp_dw0   (req_tlp_dw0),
    .tlp_dw1   (req_tlp_dw1),
    .tlp_dw2   (req_tlp_dw2),
    .tlp_dw3   (req_tlp_dw3)
);

labh2_pcie_endpoint_model u_endpoint (
    .clk       (clk),
    .resetn    (resetn),

    .req_valid (req_tlp_valid),
    .req_ready (req_tlp_ready),

    .req_dw0   (req_tlp_dw0),
    .req_dw1   (req_tlp_dw1),
    .req_dw2   (req_tlp_dw2),
    .req_dw3   (req_tlp_dw3),

    .cpl_valid (cpl_tlp_valid),
    .cpl_ready (cpl_tlp_ready),

    .cpl_dw0   (cpl_tlp_dw0),
    .cpl_dw1   (cpl_tlp_dw1),
    .cpl_dw2   (cpl_tlp_dw2),
    .cpl_dw3   (cpl_tlp_dw3)
);

labh2_pcie_tlp_rx u_tlp_rx (
    .tlp_valid  (cpl_tlp_valid),
    .tlp_ready  (cpl_tlp_ready),

    .tlp_dw0    (cpl_tlp_dw0),
    .tlp_dw1    (cpl_tlp_dw1),
    .tlp_dw2    (cpl_tlp_dw2),
    .tlp_dw3    (cpl_tlp_dw3),

    .cpl_valid  (cpl_valid),
    .cpl_ready  (cpl_ready),

    .cpl_status (cpl_status),
    .cpl_rdata  (cpl_rdata)
);

endmodule
