/* SPDX-FileCopyrightText: 2026 Chris Watson
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Date: 21-10-2026 */

module jtfelix_cab(
    input            rst,
    input            clk,
    input      [2:0] addr,

    input            LVBL,
    input            obj_pending,

    input      [1:0] cab_1p,
    input      [1:0] coin,
    input            service,
    input            dip_test,
    input            dip_pause,
    input      [5:0] joystick1,
    input      [5:0] joystick2,
    input     [23:0] dipsw,

    output reg [7:0] cab
);

function [7:0] arrange(input [5:0] joy); begin
    arrange = { 2'b00, ~joy };
end endfunction

always @(posedge clk) begin
    if( rst ) begin
        cab <= 8'hff;
    end else begin
        case( addr )
            3'd0: cab <= arrange(joystick1);
            3'd1: cab <= arrange(joystick2);
            3'd2: cab <= { ~LVBL, obj_pending, ~dip_test, ~service, ~cab_1p, ~coin };
            3'd3: cab <= dipsw[ 7: 0];
            3'd4: cab <= dipsw[15: 8];
            3'd5: cab <= 8'h00;
            3'd6: cab <= dipsw[23:16];
            3'd7: cab <= { 7'd0, ~dip_pause };
        endcase
    end
end

endmodule
