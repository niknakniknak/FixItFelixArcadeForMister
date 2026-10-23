/* SPDX-FileCopyrightText: 2026 Chris Watson
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Date: 21-10-2026 */

module jtfelix_obj_draw(
    input             rst,
    input             clk,
    input             pxl_cen,
    input             hs,
    input             start,
    input      [ 8:0] rd_addr,

    input             obj_ok,
    output            obj_take,
    input      [10:0] obj_code,
    input      [ 5:0] obj_col,
    input      [ 8:0] obj_y,
    input      [ 4:0] obj_pal,
    input      [ 1:0] obj_hsz,
    input             obj_prio,

    output     [15:0] rom_addr,
    output            rom_cs,
    input      [31:0] rom_data,
    input             rom_ok,

    output     [ 9:0] pxl
);

reg  [10:0] code;
reg  [ 8:0] ypos;
reg  [ 3:0] ysub;
reg  [ 5:0] pal;
reg  [ 1:0] left;
reg         act, draw;
wire [17:2] dr_addr;
wire [10:0] base;
wire        dr_busy;

assign obj_take = !act && obj_ok && !start;
assign base     = obj_code + ( { 9'd0, obj_col[5:4] } << obj_hsz );
assign rom_addr = { dr_addr[17:7], dr_addr[5:2], dr_addr[6] };

always @(posedge clk) begin
    if( rst ) begin
        act  <= 0;
        draw <= 0;
        code <= 0;
        ypos <= 0;
        ysub <= 0;
        pal  <= 0;
        left <= 0;
    end else if( start ) begin
        act  <= 0;
        draw <= 0;
    end else begin
        draw <= 0;
        if( !act ) begin
            if( obj_ok ) begin
                act  <= 1;
                draw <= 1;
                code <= base;
                ypos <= obj_y;
                ysub <= obj_col[3:0];
                pal  <= { obj_prio, obj_pal };
                left <= obj_hsz==2'd0 ? 2'd0 : obj_hsz==2'd1 ? 2'd1 : 2'd3;
            end
        end else if( !draw && !dr_busy ) begin
            if( left!=2'd0 ) begin
                left <= left - 2'd1;
                code <= code + 11'd1;
                ypos <= ypos + 9'd16;
                draw <= 1;
            end else begin
                act  <= 0;
            end
        end
    end
end

jtframe_objdraw #(
    .AW         ( 9         ),
    .CW         ( 11        ),
    .PW         ( 10        ),
    .HJUMP      ( 0         ),
    .HFIX       ( 0         ),
    .ALPHA      ( 0         ),
    .PACKED     ( 1         )
) u_draw(
    .rst        ( rst       ),
    .clk        ( clk       ),
    .pxl_cen    ( pxl_cen   ),
    .hs         ( hs        ),
    .flip       ( 1'b0      ),
    .hdump      ( rd_addr   ),
    .draw       ( draw      ),
    .busy       ( dr_busy   ),
    .code       ( code      ),
    .xpos       ( ypos      ),
    .ysub       ( ysub      ),
    .hzoom      ( 6'd0      ),
    .hz_keep    ( 1'b0      ),
    .hflip      ( 1'b0      ),
    .vflip      ( 1'b0      ),
    .pal        ( pal       ),
    .rom_addr   ( dr_addr   ),
    .rom_cs     ( rom_cs    ),
    .rom_ok     ( rom_ok    ),
    .rom_data   ( rom_data  ),
    .pxl        ( pxl       )
);

endmodule
