/* SPDX-FileCopyrightText: 2026 Chris Watson
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Date: 21-10-2026 */

module jtfelix_sound(
    input                rst,
    input                clk,
    input                cen,
    input      [ 7:0]    cpu_addr,
    input      [ 7:0]    cpu_dout,
    input                cpu_we,
    input                cpu_cs,
    output     [ 7:0]    cpu_din,
    input                bank,
    output     [20:0]    pcm_addr,
    output               pcm_cs,
    input      [ 7:0]    pcm_data,
    input                pcm_ok,
    output reg signed [14:0] snd
);

wire [19:0] x1_addr;
wire [12:0] x1_reg;
wire signed [15:0] left, right;
wire        unused_snd, sample;

assign x1_reg     = { 5'd0, cpu_addr };
assign pcm_addr   = { bank, x1_addr };
assign unused_snd = &{ 1'b0, right, sample, pcm_ok };

always @(posedge clk) begin
    if( rst )
        snd <= 0;
    else
        snd <= left[15]==left[14] ? left[14:0] : { left[15], {14{~left[15]}} };
end

jtx1010 u_x1010(
    .rst        ( rst       ),
    .clk        ( clk       ),
    .cen        ( cen       ),
    .addr       ( x1_reg    ),
    .din        ( cpu_dout  ),
    .dout       ( cpu_din   ),
    .we         ( cpu_we    ),
    .cs         ( cpu_cs    ),
    .rom_addr   ( x1_addr   ),
    .rom_data   ( pcm_data  ),
    .rom_cs     ( pcm_cs    ),
    .left       ( left      ),
    .right      ( right     ),
    .sample     ( sample    )
);

endmodule
