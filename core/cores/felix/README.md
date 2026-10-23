# FIFELIX

FPGA core for Fix-It Felix Jr. on the Tobikomi board, by Chris Watson.

The Tobikomi board is an invented 1982-style arcade board made for this game: Z80 at 8 MHz, two 8x8 tile layers with page flip, a line-buffer sprite engine for up to 48 16-pixel cells per line, a Seta X1-010 for sampled sound, 1 KB of NVRAM and a vertical 320x240 monitor.

## ROM

No ROM is distributed. Build felixjr.zip with the Fix-It Felix Jr. ROM maker from your own copy of the game, then use it with the MRA as any other arcade set.

## Controls

4-way joystick, Jump and Fix for each player. Player 2 plays on the second joystick. In cocktail mode the screen flips on player 2's turn. Coin and start use the usual MiSTer buttons; service coin is 9.

## Service mode

The TEST switch is F2 on the keyboard or "Service mode" in the OSD. Hold it at power-up, or switch it on during attract mode, to enter the service menu (RAM/ROM, inputs, sound, screen tests, DIP switches, extra settings, bookkeeping, high scores).

## NVRAM

Extra settings, bookkeeping and high scores are kept in 1 KB of NVRAM. They are written to the SD card when you choose "Save settings" in the OSD. A new card starts with the default settings.

## DIP switches

Lives (3, 2, 4, 5), Bonus life (every 5000, 10000, 20000, none), Difficulty (normal, easy, hard, hardest), Demo sounds, Cabinet (upright, cocktail), Coin A and Coin B, and Invulnerability for testing. The defaults play like the original game.
