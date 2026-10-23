/* SPDX-FileCopyrightText: 2026 Chris Watson
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Date: 21-10-2026 */

module jtfelix_obj_dma(
    input             rst,
    input             clk,
    input             start,

    output reg [ 9:0] ram_addr,
    input      [ 7:0] ram_dout,

    input      [ 6:0] scan_addr,
    input             scan_cen,
    output     [39:0] scan_dout
);

reg  [ 9:0] ka, kb;
reg  [10:0] cnt;
reg  [23:0] word;
reg  [31:0] wr_data;
reg  [ 7:0] sz_data;
reg  [ 6:0] wr_addr;
reg         va, vb, wr_we, sz_we;
wire [ 7:0] unused_szq0;
wire [31:0] unused_q0, scan_word;
wire [ 7:0] scan_size;
wire        done;

assign done      = cnt==11'd640;
assign scan_dout = { scan_size, scan_word };

always @(posedge clk) begin
    if( rst ) begin
        cnt     <= 11'd640;
        ram_addr <= 0;
        ka      <= 0;
        kb      <= 0;
        va      <= 0;
        vb      <= 0;
        word    <= 0;
        wr_data <= 0;
        wr_addr <= 0;
        wr_we   <= 0;
        sz_we   <= 0;
        sz_data <= 0;
    end else begin
        wr_we <= 0;
        sz_we <= 0;
        if( start ) begin
            cnt <= 0;
        end else if( !done ) begin
            cnt <= cnt + 11'd1;
        end
        ram_addr <= cnt[9:0];
        va      <= ~done & ~start;
        ka      <= cnt[9:0];
        vb      <= va;
        kb      <= ka;
        if( vb ) begin
            if( kb[9] ) begin
                sz_we   <= 1;
                wr_addr <= kb[6:0];
                sz_data <= ram_dout;
            end else if( &kb[1:0] ) begin
                wr_we   <= 1;
                wr_addr <= kb[8:2];
                wr_data <= { ram_dout, word };
            end else begin
                word <= { ram_dout, word[23:8] };
            end
        end
    end
end

jtframe_dual_ram_cen #(.DW(32),.AW(7)) u_copy(
    .clk0  ( clk       ),
    .cen0  ( 1'b1      ),
    .data0 ( wr_data   ),
    .addr0 ( wr_addr   ),
    .we0   ( wr_we     ),
    .q0    ( unused_q0 ),
    .clk1  ( clk       ),
    .cen1  ( scan_cen  ),
    .data1 ( 32'd0     ),
    .addr1 ( scan_addr ),
    .we1   ( 1'b0      ),
    .q1    ( scan_word )
);

jtframe_dual_ram_cen #(.DW(8),.AW(7)) u_size(
    .clk0  ( clk         ),
    .cen0  ( 1'b1        ),
    .data0 ( sz_data     ),
    .addr0 ( wr_addr     ),
    .we0   ( sz_we       ),
    .q0    ( unused_szq0 ),
    .clk1  ( clk         ),
    .cen1  ( scan_cen    ),
    .data1 ( 8'd0        ),
    .addr1 ( scan_addr   ),
    .we1   ( 1'b0        ),
    .q1    ( scan_size   )
);

endmodule
