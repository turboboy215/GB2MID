# GB2MID
Game Boy/Game Boy Color to MIDI/tracker/sample converter

GB2MID is an all-in-one command line tool that automatically converts music, and sometimes sound effects or samples from Game Boy and Game Boy Color games. For the majority of games, the output format is MIDI, but for games using a tracker-based sound engine, XM or MOD is instead used.
The ultimate goal of GB2MID is to support almost every offical GB and GBC game released including all their variants, exceptions being games with no music or only PCM audio. Currently, GB2MID supports over 30 sound engines which covers over 1,500 unique ROMs for both GB and GBC. Support for new drivers will frequently be added.

NOTE: If you experience startup errors, you will most likely need the Visual Studio distributables. These can be found on Microsoft's website.

Using the tool is very simple - just call the program with the name of your ROM, and files will immediately be converted, although sometimes you will be prompted to select an individual "part" of the game first. For many games using multiple banks of audio data, the music files are split into multiple folders named "Bank 1", "Bank 2", etc. ROMs are recognized via an XML database file that contains basic information about the ROM files themselves, including the hash, checksum, and game code, and the necessary information to convert audio.
In most cases, the ROM in the database is a "clean" dump. However, for many unlicensed games, a "fixed" version is used instead.
This tool integrates the functionality from all my previous standalone converters, and in some cases are slightly improved or more stable. It is recommended, at least for casual use, that this should be used instead of the other converters, and almost all future work will only be done on this tool.
While GB2MID works well for most games, some might not be converted 100% accurately - there might be desync issues. These are typically due to differences in the way audio is handled in some games using more complex drivers compared to others that may use the same general engine but slightly different. As such, if any issues are encountered, please contact me.
For a complete list of currently supported ROMs, check the document included.
