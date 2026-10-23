/* SPDX-FileCopyrightText: 2026 Chris Watson
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Date: 21-10-2026 */

module jtfelix_tiles(
    input             rst,
    input             clk,
    input             pxl_cen,
    input             hs,
    input      [ 8:0] hdump,
    input      [ 8:0] vrender,
    input             flip,
    input      [ 8:0] scry,
    input      [ 7:0] scrx,
    input             cpu_a0,
    input             bgram_we,
    input             fgram_we,
    input             fg_page,
    input             fg_cpage,
    input             fg_both,
    input             bg_page,
    input             bg_cpage,
    input             bg_both,
    output     [ 7:0] bgram_dout,
    output     [ 7:0] fgram_dout,
    output     [ 1:0] bg0c_we,
    output     [ 1:0] bg1c_we,
    output     [ 1:0] fg0c_we,
    output     [ 1:0] fg1c_we,
    input      [15:0] bg0c_dout,
    input      [15:0] bg1c_dout,
    input      [15:0] fg0c_dout,
    input      [15:0] fg1c_dout,
    output     [10:0] bgmap_addr,
    output     [10:0] fgmap_addr,
    input      [15:0] bg0_dout,
    input      [15:0] bg1_dout,
    input      [15:0] fg0_dout,
    input      [15:0] fg1_dout,
    output     [13:0] bgrom_addr,
    output            bgrom_cs,
    input      [31:0] bgrom_data,
    input             bgrom_ok,
    output     [13:0] fgrom_addr,
    output            fgrom_cs,
    input      [31:0] fgrom_data,
    input             fgrom_ok,
    output reg [ 8:0] bg_pxl,
    output reg [ 8:0] fg_pxl
);

wire [15:0] bg_map, fg_map, bgc_dout, fgc_dout;
wire [31:0] bg_planes, fg_planes;
wire [10:0] bg_vram, fg_vram;
wire [ 8:0] bg_tpxl, fg_tpxl, xv;
wire [ 7:0] x;
wire [ 1:0] lane;
wire        unused_tiles, line_start, reload, fg0_we, fg1_we, bg0_we, bg1_we;

reg  [ 8:0] scry_f, scry_d, ycnt;
reg  [ 7:0] scrx_f, x_d;
reg         flip_f, flip_d, a0_l, cpage_l, bcpage_l;

assign unused_tiles = &{ 1'b0, vrender[8] };
assign line_start = pxl_cen && hdump==9'd438;
assign reload     = pxl_cen && hdump==9'd400;
assign x          = flip_f ? vrender[7:0] : 8'd239 - vrender[7:0];
assign xv         = { 1'b0, x };
assign bg_map     = bg_page ? bg1_dout : bg0_dout;
assign fg_map     = fg_page ? fg1_dout : fg0_dout;
assign bgmap_addr = { bg_vram[5:0], bg_vram[10:6] };
assign fgmap_addr = { fg_vram[5:0], fg_vram[10:6] };
assign lane       = { cpu_a0, ~cpu_a0 };
assign bg0_we     = bgram_we & ( bg_both | ~bg_cpage );
assign bg1_we     = bgram_we & ( bg_both |  bg_cpage );
assign fg0_we     = fgram_we & ( fg_both | ~fg_cpage );
assign fg1_we     = fgram_we & ( fg_both |  fg_cpage );
assign bg0c_we    = lane & { 2{ bg0_we } };
assign bg1c_we    = lane & { 2{ bg1_we } };
assign fg0c_we    = lane & { 2{ fg0_we } };
assign fg1c_we    = lane & { 2{ fg1_we } };
assign bgc_dout   = bcpage_l ? bg1c_dout : bg0c_dout;
assign fgc_dout   = cpage_l  ? fg1c_dout : fg0c_dout;
assign bgram_dout = a0_l ? bgc_dout[15:8] : bgc_dout[7:0];
assign fgram_dout = a0_l ? fgc_dout[15:8] : fgc_dout[7:0];

genvar p, k;
generate
    for( p=0; p<4; p=p+1 ) begin : gen_plane
        for( k=0; k<8; k=k+1 ) begin : gen_pixel
            assign bg_planes[8*p+7-k] = bgrom_data[4*k+p];
            assign fg_planes[8*p+7-k] = fgrom_data[4*k+p];
        end
    end
endgenerate

always @(posedge clk) begin
    if( rst ) begin
        a0_l     <= 0;
        cpage_l  <= 0;
        bcpage_l <= 0;
    end else begin
        a0_l     <= cpu_a0;
        cpage_l  <= fg_cpage;
        bcpage_l <= bg_cpage;
    end
end

always @(posedge clk) begin
    if( rst ) begin
        flip_f <= 0;
        flip_d <= 0;
        scry_f <= 0;
        scry_d <= 0;
        scrx_f <= 0;
        x_d    <= 0;
        ycnt   <= 0;
    end else begin
        if( line_start ) begin
            flip_f <= flip;
            scry_f <= scry;
            scrx_f <= scrx;
        end
        if( reload ) begin
            flip_d <= flip_f;
            scry_d <= scry_f;
            x_d    <= x;
        end
        if( pxl_cen ) begin
            if( reload )
                ycnt <= flip_f ? 9'd348 : 9'd483;
            else
                ycnt <= flip_d ? ycnt - 9'd1 : ycnt + 9'd1;
        end
    end
end

always @(posedge clk) begin
    if( rst ) begin
        bg_pxl <= 0;
        fg_pxl <= 0;
    end else if( pxl_cen ) begin
        bg_pxl <= bg_tpxl;
        fg_pxl <= fg_tpxl;
    end
end

jtframe_scroll #(
    .SIZE       ( 8             ),
    .VA         ( 11            ),
    .CW         ( 11            ),
    .PW         ( 9             ),
    .MAP_HW     ( 9             ),
    .MAP_VW     ( 8             )
) u_bg(
    .rst        ( rst           ),
    .clk        ( clk           ),
    .pxl_cen    ( pxl_cen       ),
    .hs         ( hs            ),
    .vdump      ( xv            ),
    .hdump      ( ycnt          ),
    .blankn     ( 1'b1          ),
    .flip       ( 1'b0          ),
    .scrx       ( scry_d        ),
    .scry       ( scrx_f        ),
    .vram_addr  ( bg_vram       ),
    .code       ( bg_map[10:0]  ),
    .pal        ( bg_map[15:11] ),
    .hflip      ( flip_d        ),
    .vflip      ( 1'b0          ),
    .rom_addr   ( bgrom_addr    ),
    .rom_data   ( bg_planes     ),
    .rom_cs     ( bgrom_cs      ),
    .rom_ok     ( bgrom_ok      ),
    .pxl        ( bg_tpxl       )
);

jtframe_tilemap #(
    .SIZE       ( 8             ),
    .VA         ( 11            ),
    .CW         ( 11            ),
    .PW         ( 9             ),
    .MAP_HW     ( 9             ),
    .MAP_VW     ( 8             ),
    .HDUMPW     ( 9             ),
    .VDUMPW     ( 8             ),
    .FLIP_HDUMP ( 0             ),
    .FLIP_VDUMP ( 0             )
) u_fg(
    .rst        ( rst           ),
    .clk        ( clk           ),
    .pxl_cen    ( pxl_cen       ),
    .vdump      ( x_d           ),
    .hdump      ( ycnt          ),
    .blankn     ( 1'b1          ),
    .flip       ( 1'b0          ),
    .vram_addr  ( fg_vram       ),
    .code       ( fg_map[10:0]  ),
    .pal        ( fg_map[15:11] ),
    .hflip      ( flip_d        ),
    .vflip      ( 1'b0          ),
    .rom_addr   ( fgrom_addr    ),
    .rom_data   ( fg_planes     ),
    .rom_cs     ( fgrom_cs      ),
    .rom_ok     ( fgrom_ok      ),
    .pxl        ( fg_tpxl       )
);

endmodule
