# as-scrobbler
Allows you to scrobble music listened to in Audiosurf 1 to a [maloja](https://github.com/krateng/maloja) server with the power of [SafetyHook](https://github.com/cursey/safetyhook), C++ and Rust. as-scrobbler rewrites parts of the Wininet library to emulate the last.fm server Audiosurf 1 uses for its built-in scrobbling capabilities, and allows you to send these scrobbles elsewhere.

# How do I get it to work?
You will need to copy the DLL to `...\SteamLibrary\steamapps\common\Audiosurf\engine\channels`, and also make sure to place my [audiosurfscrobblerlib](https://github.com/duckfromdiscord/audiosurfscrobblerlib) alongside it. Note that as-scrobbler **only** emulates a server, and the actual scrobbling logic comes from audiosurfscrobblerlib. Your game will not work if you only have as-scrobbler. You also need to place `preload_enabler.toml` one folder above, in `...\SteamLibrary\steamapps\common\Audiosurf\engine`. This file is where you specify your maloja URL, port, and API key. **Also, make sure to enable last.fm scrobbling in-game so that the requests will trigger and thereby be intercepted.**

# Why is it called preload_enabler?
The original goal of this project was to force Audiosurf to fully load your song into memory before playing it, but this functionality is not yet implemented and enabling it will crash your game.

# Can you allow scrobbling to something else?
All of the scrobbling logic is in [audiosurfscrobblerlib](https://github.com/duckfromdiscord/audiosurfscrobblerlib). I did this on purpose because it's much easier to add functionality to that than rebuilding as-scrobbler all the time. If you open a PR, it will make it easier for me to add a scrobbling server. I may also add some on my own later on.