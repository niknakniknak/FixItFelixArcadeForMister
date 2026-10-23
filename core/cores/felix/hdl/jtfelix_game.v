/* SPDX-FileCopyrightText: 2026 Chris Watson
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Date: 21-10-2026 */

module jtfelix_game(
    `include "jtframe_game_ports.inc" // see $JTFRAME/hdl/inc/jtframe_game_ports.inc
);

wire [15:0] pal_data;
wire [ 7:0] bgram_dout, fgram_dout;
wire [ 8:0] scry;
wire [ 7:0] scrx;
wire [ 7:0] snd_addr, snd_dout;
wire        bgram_we, fgram_we, cpu_a0, flip, bg_en, fg_en, obj_en,
            vint, snd_we, snd_cs, snd_bank, obj_cmode, obj_commit, obj_pending, fg_page, fg_cpage, fg_both,
            bg_page, bg_cpage, bg_both, scr_flip;

assign debug_view = 0;
assign ioctl_din  = 0;
assign pxl_cen    = cen_pxl;
assign pxl2_cen   = cen_pxl2;
assign scr_flip   = flip ^ dip_flip;
assign pal_data   = { palhi_data, pallo_data };
assign palhi_addr = pallo_addr;
assign cpu_a0     = cpu_addr[0];

jtfelix_main u_main(
    .rst         ( rst            ),
    .clk         ( clk            ),
    .cpu_cen     ( cpu_cen        ),
    .vint        ( vint           ),
    .LVBL        ( LVBL           ),
    .cab_1p      ( cab_1p[1:0]    ),
    .coin        ( coin[1:0]      ),
    .service     ( service        ),
    .dip_test    ( dip_test       ),
    .dip_pause   ( dip_pause      ),
    .joystick1   ( joystick1[5:0] ),
    .joystick2   ( joystick2[5:0] ),
    .dipsw       ( dipsw[23:0]    ),
    .cpu_addr    ( cpu_addr       ),
    .cpu_dout    ( cpu_dout       ),
    .bgram_we    ( bgram_we       ),
    .fgram_we    ( fgram_we       ),
    .objram_we   ( objram_we      ),
    .ram_we      ( ram_we         ),
    .ram_dout    ( ram_dout       ),
    .nvram_we    ( nvram_we       ),
    .nvram_din   ( nvram_din      ),
    .nvram_dout  ( nvram_dout     ),
    .bgram_dout  ( bgram_dout     ),
    .fgram_dout  ( fgram_dout     ),
    .objram_dout ( objram_dout    ),
    .scry        ( scry           ),
    .scrx        ( scrx           ),
    .flip        ( flip           ),
    .bg_en       ( bg_en          ),
    .fg_en       ( fg_en          ),
    .obj_en      ( obj_en         ),
    .obj_cmode   ( obj_cmode      ),
    .obj_commit  ( obj_commit     ),
    .obj_pending ( obj_pending    ),
    .fg_page     ( fg_page        ),
    .fg_cpage    ( fg_cpage       ),
    .fg_both     ( fg_both        ),
    .bg_page     ( bg_page        ),
    .bg_cpage    ( bg_cpage       ),
    .bg_both     ( bg_both        ),
    .snd_addr    ( snd_addr       ),
    .snd_we      ( snd_we         ),
    .snd_cs      ( snd_cs         ),
    .snd_dout    ( snd_dout       ),
    .snd_bank    ( snd_bank       ),
    .main_cs     ( main_cs        ),
    .main_addr   ( main_addr      ),
    .main_data   ( main_data      ),
    .main_ok     ( main_ok        )
);

jtfelix_video u_video(
    .rst         ( rst            ),
    .clk         ( clk            ),
    .pxl_cen     ( pxl_cen        ),
    .cpu_a0      ( cpu_a0         ),
    .bgram_we    ( bgram_we       ),
    .fgram_we    ( fgram_we       ),
    .fg_page     ( fg_page        ),
    .fg_cpage    ( fg_cpage       ),
    .fg_both     ( fg_both        ),
    .bg_page     ( bg_page        ),
    .bg_cpage    ( bg_cpage       ),
    .bg_both     ( bg_both        ),
    .obj_cmode   ( obj_cmode      ),
    .obj_commit  ( obj_commit     ),
    .obj_pending ( obj_pending    ),
    .bgram_dout  ( bgram_dout     ),
    .fgram_dout  ( fgram_dout     ),
    .bg0c_we     ( bg0c_we        ),
    .bg1c_we     ( bg1c_we        ),
    .fg0c_we     ( fg0c_we        ),
    .fg1c_we     ( fg1c_we        ),
    .bg0c_dout   ( bg0c_dout      ),
    .bg1c_dout   ( bg1c_dout      ),
    .fg0c_dout   ( fg0c_dout      ),
    .fg1c_dout   ( fg1c_dout      ),
    .bgmap_addr  ( bgmap_addr     ),
    .fgmap_addr  ( fgmap_addr     ),
    .bg0_dout    ( bg0_dout       ),
    .bg1_dout    ( bg1_dout       ),
    .fg0_dout    ( fg0_dout       ),
    .fg1_dout    ( fg1_dout       ),
    .oram_addr   ( oram_addr      ),
    .oram_dout   ( oram_dout      ),
    .scry        ( scry           ),
    .scrx        ( scrx           ),
    .flip        ( scr_flip       ),
    .bg_en       ( bg_en          ),
    .fg_en       ( fg_en          ),
    .obj_en      ( obj_en         ),
    .LHBL        ( LHBL           ),
    .LVBL        ( LVBL           ),
    .HS          ( HS             ),
    .VS          ( VS             ),
    .vint        ( vint           ),
    .bgrom_addr  ( bgrom_addr     ),
    .bgrom_cs    ( bgrom_cs       ),
    .bgrom_data  ( bgrom_data     ),
    .bgrom_ok    ( bgrom_ok       ),
    .fgrom_addr  ( fgrom_addr     ),
    .fgrom_cs    ( fgrom_cs       ),
    .fgrom_data  ( fgrom_data     ),
    .fgrom_ok    ( fgrom_ok       ),
    .obj_addr    ( obj_addr       ),
    .obj_cs      ( obj_cs         ),
    .obj_data    ( obj_data       ),
    .obj_ok      ( obj_ok         ),
    .pal_addr    ( pallo_addr     ),
    .pal_data    ( pal_data       ),
    .red         ( red            ),
    .green       ( green          ),
    .blue        ( blue           ),
    .gfx_en      ( gfx_en         )
);

jtfelix_sound u_sound(
    .rst         ( rst            ),
    .clk         ( clk            ),
    .cen         ( pcm_cen        ),
    .cpu_addr    ( snd_addr       ),
    .cpu_dout    ( cpu_dout       ),
    .cpu_we      ( snd_we         ),
    .cpu_cs      ( snd_cs         ),
    .cpu_din     ( snd_dout       ),
    .bank        ( snd_bank       ),
    .pcm_addr    ( smp_addr       ),
    .pcm_cs      ( smp_cs         ),
    .pcm_data    ( smp_data       ),
    .pcm_ok      ( smp_ok         ),
    .snd         ( pcm            )
);

endmodule
