Fix-It Felix Jr. ROM maker (felixrommaker.exe)

This tool builds felixjr.zip, the ROM set for the Fix-It Felix Jr. MiSTer core, from your own copy of the game. It contains no graphics, sound or data from the game: only our program, the positions where it finds things in your files, and checksums.

You need the Windows "FIFJ_leaked" build of Fix-It Felix Jr.: FixitFelixJr.exe with its Data folder next to it. Optional: the Airwalk Studio's Mega Drive ROM "Fix-It Felix Jr. (World) (Aftermarket).md" or "felix.bin" for the two voice clips, and the AtGames build's "res_fif" folder (it sits next to "fif_libretro.so") for the high-score entry and player-change sounds. Without them those sounds are silent; everything else is the same.

Easiest: put felixrommaker.exe in the folder with FixitFelixJr.exe (and the .md file or res_fif folder if you have them) and double-click it. Or drag FixitFelixJr.exe, its folder, the .md file and the res_fif folder onto felixrommaker.exe at the same time. It writes felixjr.zip next to felixrommaker.exe.

Command line: felixrommaker.exe --exe <path to FixitFelixJr.exe> [--megadrive <path to .md>] [--atgames <res_fif folder>] [-o felixjr.zip]

Copy felixjr.zip to games/mame (or wherever your MiSTer keeps arcade ROM zips) next to the core and the MRA.

The tool checks every input file against known checksums and checks its own output against the checksum of the release. If a file is a different version, or the output would not match, it stops and writes nothing.

felix_prog.bin is our game program with the parts that come from the original left blank; it is also built into felixrommaker.exe, so you do not need it to run the tool. expected_md5.json lists the checksum of the finished ROM set for each combination of inputs.
