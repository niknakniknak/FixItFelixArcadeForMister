/* SPDX-FileCopyrightText: 2026 Chris Watson
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Date: 21-10-2026 */

module jtfelix_main(
    input             rst,
    input             clk,
    input             cpu_cen,
    input             vint,
    input             LVBL,
    input      [ 1:0] cab_1p,
    input      [ 1:0] coin,
    input             service,
    input             dip_test,
    input             dip_pause,
    input      [ 5:0] joystick1,
    input      [ 5:0] joystick2,
    input      [23:0] dipsw,
    output     [11:0] cpu_addr,
    output     [ 7:0] cpu_dout,
    output            bgram_we,
    output            fgram_we,
    output            objram_we,
    output            ram_we,
    input      [ 7:0] ram_dout,
    output            nvram_we,
    output     [ 7:0] nvram_din,
    input      [ 7:0] nvram_dout,
    input      [ 7:0] bgram_dout,
    input      [ 7:0] fgram_dout,
    input      [ 7:0] objram_dout,
    output reg [ 8:0] scry,
    output reg [ 7:0] scrx,
    output reg        flip,
    output reg        bg_en,
    output reg        fg_en,
    output reg        obj_en,
    output reg        obj_cmode,
    output            obj_commit,
    input             obj_pending,
    output reg        fg_page,
    output reg        fg_cpage,
    output reg        fg_both,
    output reg        bg_page,
    output reg        bg_cpage,
    output reg        bg_both,
    output     [ 7:0] snd_addr,
    output            snd_we,
    output            snd_cs,
    input      [ 7:0] snd_dout,
    output reg        snd_bank,
    output            main_cs,
    output     [16:0] main_addr,
    input      [ 7:0] main_data,
    input             main_ok
);

wire [15:0] A;
wire [ 7:0] cab;
wire        mreq_n, rd_n, wr_n, rfsh_n,
            memrd, memwr, wr_pulse, rom_sel, ram_sel, bg_sel, fg_sel, obj_sel, io_sel,
            reg_sel, snd_sel, cab_sel, reg_we, rst_n, nv_sel;
wire [ 7:0] nv_q;
reg  [ 7:0] din;
reg  [ 2:0] bank;
reg         int_n, int_en, wr_l, fg_preq, bg_preq, flip_req;

assign rst_n     = ~rst;
assign memrd     = ~mreq_n & ~rd_n;
assign memwr     = ~mreq_n & ~wr_n;
assign rom_sel   = A[15:14] != 2'b11;
assign ram_sel   = A[15:12] == 4'hc;
assign bg_sel    = A[15:12] == 4'hd;
assign fg_sel    = A[15:12] == 4'he;
assign obj_sel   = A[15:10] == 6'b1111_00;
assign nv_sel    = A[15:10] == 6'b1111_01;
assign io_sel    = A[15: 8] == 8'hf8;
assign reg_sel   = io_sel && A[7:4] == 4'h1;
assign snd_sel   = A[15: 8] == 8'hf9;
assign cab_sel   = io_sel && A[7:3] == 5'h0;
assign wr_pulse  = memwr & ~wr_l;
assign ram_we    = memwr & ram_sel;
assign nvram_we  = memwr & nv_sel;
assign nvram_din = ~cpu_dout;
assign nv_q      = ~nvram_dout;
assign reg_we    = wr_pulse & reg_sel;
assign bgram_we  = wr_pulse & bg_sel;
assign fgram_we  = wr_pulse & fg_sel;
assign objram_we = wr_pulse & obj_sel;
assign snd_we    = memwr;
assign snd_cs    = snd_sel;
assign snd_addr  = A[7:0];
assign cpu_addr  = A[11:0];
assign main_cs   = ~mreq_n & rfsh_n & rom_sel;
assign main_addr = { A[15] ? bank : { 2'b00, A[14] }, A[13:0] };
assign obj_commit= reg_we && A[3:0]==4'h6;

always @(posedge clk) begin
    if( rst ) begin
        scry   <= 0;
        scrx   <= 0;
        flip   <= 0;
        flip_req <= 0;
        bg_en  <= 1;
        fg_en  <= 1;
        obj_en <= 1;
        obj_cmode <= 0;
        fg_page  <= 0;
        fg_preq  <= 0;
        fg_cpage <= 0;
        fg_both  <= 0;
        bg_page  <= 0;
        bg_preq  <= 0;
        bg_cpage <= 0;
        bg_both  <= 0;
        bank   <= 3'd2;
        snd_bank <= 0;
        int_en <= 0;
        int_n  <= 1;
        wr_l   <= 0;
    end else begin
        wr_l <= memwr;
        if( vint && int_en ) int_n <= 0;
        if( vint ) begin
            fg_page <= fg_preq;
            flip    <= flip_req;
            bg_page <= bg_preq;
        end
        if( reg_we ) case( A[3:0] )
            4'h0: scry[7:0] <= cpu_dout;
            4'h1: scry[8]   <= cpu_dout[0];
            4'h2: scrx      <= cpu_dout;
            4'h3: { obj_cmode, obj_en, fg_en, bg_en, flip_req } <= cpu_dout[4:0];
            4'h4: begin
                int_en <= cpu_dout[0];
                if( !cpu_dout[0] ) int_n <= 1;
            end
            4'h7: { bg_both, bg_cpage, bg_preq, fg_both, fg_cpage, fg_preq } <= cpu_dout[5:0];
            4'h8: bank <= cpu_dout[2:0];
            4'h9: snd_bank <= cpu_dout[0];
            default:;
        endcase
    end
end

always @* begin
    din = 8'hff;
    if( memrd ) begin
        if( rom_sel )      din = main_data;
        else if( ram_sel ) din = ram_dout;
        else if( bg_sel  ) din = bgram_dout;
        else if( fg_sel  ) din = fgram_dout;
        else if( obj_sel ) din = objram_dout;
        else if( nv_sel  ) din = nv_q;
        else if( snd_sel ) din = snd_dout;
        else if( cab_sel ) din = cab;
    end
end

jtfelix_cab u_cab(
    .rst         ( rst         ),
    .clk         ( clk         ),
    .addr        ( A[2:0]      ),
    .LVBL        ( LVBL        ),
    .obj_pending ( obj_pending ),
    .cab_1p      ( cab_1p      ),
    .coin        ( coin        ),
    .service     ( service     ),
    .dip_test    ( dip_test    ),
    .dip_pause   ( dip_pause   ),
    .joystick1   ( joystick1   ),
    .joystick2   ( joystick2   ),
    .dipsw       ( dipsw       ),
    .cab         ( cab         )
);

jtframe_z80_romwait #(.RECOVERY(0)) u_cpu(
    .rst_n      ( rst_n     ),
    .clk        ( clk       ),
    .cen        ( cpu_cen   ),
    .cpu_cen    (           ),
    .int_n      ( int_n     ),
    .nmi_n      ( 1'b1      ),
    .busrq_n    ( 1'b1      ),
    .m1_n       (           ),
    .mreq_n     ( mreq_n    ),
    .iorq_n     (           ),
    .rd_n       ( rd_n      ),
    .wr_n       ( wr_n      ),
    .rfsh_n     ( rfsh_n    ),
    .halt_n     (           ),
    .busak_n    (           ),
    .A          ( A         ),
    .din        ( din       ),
    .dout       ( cpu_dout  ),
    .rom_cs     ( main_cs   ),
    .rom_ok     ( main_ok   )
);

endmodule
