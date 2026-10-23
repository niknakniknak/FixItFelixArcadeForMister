/* SPDX-FileCopyrightText: 2026 Chris Watson
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Date: 21-10-2026 */

module jtfelix_obj #(parameter [6:0] BUDGET=7'd48)(
    input             rst,
    input             clk,
    input             pxl_cen,
    input             flip,
    input      [ 8:0] hdump,
    input      [ 8:0] vrender,
    input             LHBL,
    input             HS,

    output     [ 9:0] ram_addr,
    input      [ 7:0] ram_dout,
    input             cmode,
    input             commit,
    output reg        pending,

    output     [15:0] rom_addr,
    output            rom_cs,
    input      [31:0] rom_data,
    input             rom_ok,

    output reg [ 9:0] pxl
);

wire [39:0] scan_dout;
wire [ 8:0] rd_addr;
wire [ 9:0] buf_dout;
wire [ 7:0] gx;
wire [ 6:0] scan_addr;
wire [10:0] obj_code;
wire [ 5:0] obj_col;
wire [ 8:0] obj_y;
wire [ 4:0] obj_pal;
wire [ 1:0] obj_hsz;
wire        obj_prio, scan_cen, obj_ok, obj_take, start, active, vbl, dma_start, late;
reg         HS_l, win;

assign start     = HS & ~HS_l;
assign active    = vrender < 9'd240;
assign vbl       = start && vrender==9'd240;
assign late      = win && pending && !vbl;
assign dma_start = ( vbl && ( pending || !cmode ) ) || late;
assign gx        = flip ? vrender[7:0] : 8'd239 - vrender[7:0];
assign rd_addr   = flip ? 9'd319 - hdump : hdump;

always @(posedge clk) begin
    if( rst ) begin
        HS_l    <= 1;
        pending <= 0;
        win     <= 0;
    end else begin
        HS_l <= HS;
        if( vbl ) win <= 1;
        if( start && vrender==9'd259 ) win <= 0;
        if( vbl || late ) pending <= 0;
        if( commit ) pending <= 1;
    end
end

always @(posedge clk) begin
    if( rst ) begin
        pxl <= 0;
    end else if( pxl_cen ) begin
        pxl <= LHBL ? buf_dout : 10'd0;
    end
end

jtfelix_obj_dma u_dma(
    .rst        ( rst       ),
    .clk        ( clk       ),
    .start      ( dma_start ),
    .ram_addr   ( ram_addr  ),
    .ram_dout   ( ram_dout  ),
    .scan_addr  ( scan_addr ),
    .scan_cen   ( scan_cen  ),
    .scan_dout  ( scan_dout )
);

jtfelix_obj_scan #(.BUDGET(BUDGET)) u_scan(
    .rst        ( rst       ),
    .clk        ( clk       ),
    .start      ( start     ),
    .active     ( active    ),
    .gx         ( gx        ),
    .ram_addr   ( scan_addr ),
    .ram_cen    ( scan_cen  ),
    .ram_dout   ( scan_dout ),
    .obj_ok     ( obj_ok    ),
    .obj_take   ( obj_take  ),
    .obj_code   ( obj_code  ),
    .obj_col    ( obj_col   ),
    .obj_y      ( obj_y     ),
    .obj_pal    ( obj_pal   ),
    .obj_hsz    ( obj_hsz   ),
    .obj_prio   ( obj_prio  )
);

jtfelix_obj_draw u_draw(
    .rst        ( rst       ),
    .clk        ( clk       ),
    .pxl_cen    ( pxl_cen   ),
    .hs         ( HS        ),
    .start      ( start     ),
    .rd_addr    ( rd_addr   ),
    .obj_ok     ( obj_ok    ),
    .obj_take   ( obj_take  ),
    .obj_code   ( obj_code  ),
    .obj_col    ( obj_col   ),
    .obj_y      ( obj_y     ),
    .obj_pal    ( obj_pal   ),
    .obj_hsz    ( obj_hsz   ),
    .obj_prio   ( obj_prio  ),
    .rom_addr   ( rom_addr  ),
    .rom_cs     ( rom_cs    ),
    .rom_data   ( rom_data  ),
    .rom_ok     ( rom_ok    ),
    .pxl        ( buf_dout  )
);

endmodule
