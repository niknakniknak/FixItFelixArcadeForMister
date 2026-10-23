/* SPDX-FileCopyrightText: 2026 Chris Watson
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Date: 21-10-2026 */

module jtfelix_colmix(
    input             rst,
    input             clk,
    input             pxl_cen,
    input             preLHBL,
    input             preLVBL,
    input             preHS,
    input             preVS,
    input      [ 8:0] bg_pxl,
    input      [ 8:0] fg_pxl,
    input      [ 9:0] obj_pxl,
    input             bg_en,
    input             fg_en,
    input             obj_en,
    input      [ 3:0] gfx_en,
    output reg [ 9:0] pal_addr,
    input      [15:0] pal_data,
    output            LHBL,
    output            LVBL,
    output            HS,
    output            VS,
    output     [ 4:0] red,
    output     [ 4:0] green,
    output     [ 4:0] blue
);

wire [14:0] rgb;
wire [ 1:0] presync, sync;
wire        bg_on, fg_on, obj_on, fg_opaque, obj_opaque, unused_colmix, unused_prelbl;

assign red           = rgb[14:10];
assign green         = rgb[ 9: 5];
assign blue          = rgb[ 4: 0];
assign presync       = { preHS, preVS };
assign HS            = sync[1];
assign VS            = sync[0];
assign bg_on         = bg_en  & gfx_en[0];
assign fg_on         = fg_en  & gfx_en[1];
assign obj_on        = obj_en & gfx_en[3];
assign fg_opaque     = fg_on  && fg_pxl[3:0]!=4'd0;
assign obj_opaque    = obj_on && obj_pxl[3:0]!=4'd0 && !( obj_pxl[9] && bg_on && bg_pxl[3:0]!=4'd0 );
assign unused_colmix = &{ 1'b0, pal_data[15], gfx_en[2] };

always @(posedge clk) begin
    if( rst ) begin
        pal_addr <= 0;
    end else if( pxl_cen ) begin
        if( fg_opaque )
            pal_addr <= { 1'b0, fg_pxl };
        else if( obj_opaque )
            pal_addr <= { 1'b1, obj_pxl[8:0] };
        else
            pal_addr <= bg_on ? { 1'b0, bg_pxl } : 10'd0;
    end
end

jtframe_blank #(.DLY(3),.DW(15)) u_blank(
    .clk        ( clk               ),
    .pxl_cen    ( pxl_cen           ),
    .preLHBL    ( preLHBL           ),
    .preLVBL    ( preLVBL           ),
    .LHBL       ( LHBL              ),
    .LVBL       ( LVBL              ),
    .preLBL     ( unused_prelbl     ),
    .rgb_in     ( pal_data[14:0]    ),
    .rgb_out    ( rgb               )
);

jtframe_sh #(.W(2),.L(3)) u_sync(
    .clk        ( clk               ),
    .clk_en     ( pxl_cen           ),
    .din        ( presync           ),
    .drop       ( sync              )
);

endmodule
