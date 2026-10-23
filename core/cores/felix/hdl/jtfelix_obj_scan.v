/* SPDX-FileCopyrightText: 2026 Chris Watson
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Date: 21-10-2026 */

module jtfelix_obj_scan #(parameter [6:0] BUDGET=7'd48)(
    input             rst,
    input             clk,
    input             start,
    input             active,
    input      [ 7:0] gx,

    output reg [ 6:0] ram_addr,
    output            ram_cen,
    input      [39:0] ram_dout,

    output reg        obj_ok,
    input             obj_take,
    output reg [10:0] obj_code,
    output reg [ 5:0] obj_col,
    output reg [ 8:0] obj_y,
    output reg [ 4:0] obj_pal,
    output reg [ 1:0] obj_hsz,
    output reg        obj_prio
);

localparam [1:0] IDLE = 2'd0,
                 PASS1= 2'd1,
                 PASS2= 2'd2;

reg  [ 1:0] st;
reg  [ 7:0] gxl;
reg  [ 6:0] idx, idxa, idxb, last;
reg  [ 6:0] cnt;
reg         iss, va, vb;
wire [ 8:0] diff, ypos, ytop;
wire [ 6:0] ncnt;
wire [ 2:0] cost;
wire [ 1:0] wsz, hsz;
wire        hit, xin, yin, adv, fits, take, full, any, unused_scan;
wire [ 6:0] nlast;

assign unused_scan = &{ 1'b0, ram_dout[39] };
assign wsz     = ram_dout[33] ? 2'd2 : ram_dout[33:32];
assign hsz     = ram_dout[35] ? 2'd2 : ram_dout[35:34];
assign diff    = { 1'b0, gxl } - { ram_dout[36], ram_dout[7:0] };
assign ypos    = { ram_dout[26], ram_dout[15:8] };
assign ytop    = hsz==2'd0 ? 9'd496 : hsz==2'd1 ? 9'd480 : 9'd448;
assign cost    = hsz==2'd0 ? 3'd1 : hsz==2'd1 ? 3'd2 : 3'd4;
assign xin     = diff[8:6]==3'd0 && ( wsz==2'd2 || diff[5]==1'b0 && ( wsz==2'd1 || diff[4]==1'b0 ) );
assign yin     = ypos < 9'd320 || ypos > ytop;
assign hit     = xin && yin;
assign ncnt    = cnt + { 4'd0, cost };
assign fits    = ncnt <= BUDGET;
assign take    = hit && fits;
assign full    = ( hit && !fits ) || ( take && ncnt==BUDGET );
assign any     = take || cnt!=7'd0;
assign adv     = ~( st==PASS2 && vb && hit && obj_ok && !obj_take );
assign ram_cen = adv;
assign nlast   = take ? idxb : last;

always @(posedge clk) begin
    if( rst ) begin
        st       <= IDLE;
        gxl      <= 0;
        idx      <= 0;
        idxa     <= 0;
        idxb     <= 0;
        iss      <= 0;
        va       <= 0;
        vb       <= 0;
        cnt      <= 0;
        last     <= 0;
        ram_addr <= 0;
        obj_ok   <= 0;
        obj_code <= 0;
        obj_col  <= 0;
        obj_y    <= 0;
        obj_pal  <= 0;
        obj_hsz  <= 0;
        obj_prio <= 0;
    end else if( start ) begin
        st     <= active ? PASS1 : IDLE;
        gxl    <= gx;
        idx    <= 0;
        iss    <= active;
        va     <= 0;
        vb     <= 0;
        cnt    <= 0;
        obj_ok <= 0;
    end else begin
        if( obj_take ) obj_ok <= 0;
        if( adv ) begin
            ram_addr <= idx;
            va       <= iss;
            idxa     <= idx;
            vb       <= va;
            idxb     <= idxa;
            if( iss ) begin
                if( st==PASS1 ) begin
                    idx <= idx + 7'd1;
                    if( &idx ) iss <= 0;
                end else begin
                    idx <= idx - 7'd1;
                    if( idx==7'd0 ) iss <= 0;
                end
            end
            if( vb ) begin
                if( st==PASS1 ) begin
                    if( take ) begin
                        cnt  <= ncnt;
                        last <= idxb;
                    end
                    if( full || &idxb ) begin
                        va  <= 0;
                        vb  <= 0;
                        idx <= nlast;
                        iss <= any;
                        st  <= any ? PASS2 : IDLE;
                    end
                end else if( st==PASS2 ) begin
                    if( hit ) begin
                        obj_ok   <= 1;
                        obj_code <= { ram_dout[37], ram_dout[25:24], ram_dout[23:16] };
                        obj_col  <= diff[5:0];
                        obj_y    <= ypos;
                        obj_pal  <= ram_dout[31:27];
                        obj_hsz  <= hsz;
                        obj_prio <= ram_dout[38];
                    end
                    if( idxb==7'd0 ) st <= IDLE;
                end
            end
        end
    end
end

endmodule
