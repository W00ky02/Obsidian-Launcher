# Launch behavior

Obsidian starts an installed Minecraft Java profile as a child process and shows its output in the live console. Token sign-in retrieves the profile name and UUID from Minecraft Services. Microsoft sign-in needs a registered and approved client ID and is not configured in this release. Offline singleplayer creates a local identity without contacting the profile service.

The separate option to open the official launcher is a fallback. That launcher handles its own sign-in, game processes, and logs; Obsidian does not track those games.

## Local profiles and Java

- All game sessions use `%APPDATA%\.minecraft`. Profiles share mods, worlds, and settings.
- Profile inheritance is resolved recursively, with cycle detection. Child libraries replace matching parent libraries. Vanilla, Fabric, Forge, NeoForge, and Quilt are detected from profile IDs, main classes, and libraries.
- The launcher reads modern `arguments.game/jvm`, legacy `minecraftArguments`, platform and feature rules, Maven coordinates, native classifiers, and installed logging configurations.
- Only Windows x64 runtimes are supported. ARM64 and 32-bit runtimes are not supported.
- Java is searched for in Minecraft runtime folders, known official launcher locations, `JAVA_HOME`, common installation folders, and `PATH`. Its major version and architecture are checked with `java -version`. You can also select Java manually for the current session.
- Main class, client JAR, libraries, native JARs, asset index, and logging configuration are checked. Asset existence and size are checked before launch. This is not a full integrity check or a guarantee that mods will work.
- Missing game files, Java, loaders, and Forge or NeoForge installer outputs are not downloaded or generated. The profile must already be fully installed.
- Unknown argument placeholders cause a launch error. Demo and Quick Play are not supported. Optional account `clientId` and `xuid` fields remain empty.

## Accounts and privacy

Account sessions stay in memory. Obsidian does not read existing official account files or save tokens, profile details, or offline names. The token input is cleared when a sign-in attempt begins. Settings contain interface and launch preferences, favorites, public application IDs, and launcher paths.

Minecraft receives its token as a process argument. Operating system tools, Minecraft, and mods may be able to see it, and the game can write logs or crash reports under `.minecraft`. Obsidian cannot control those files. Qt strings do not guarantee that every memory copy can be erased. Java heap dumps on out-of-memory errors are disabled.

Offline names use 1–16 ASCII letters, numbers, or underscores. The offline UUID is derived from `OfflinePlayer:<Name>` using Java-compatible UUID rules. The token placeholder is `0`, with the legacy user type. Changing a name or its capitalization changes the offline identity and may affect existing worlds. Offline mode does not grant access to servers that verify accounts.

## Logs and process handling

The game is started directly, without a shell command line. Standard output and error are redacted before display, including credentials split across received chunks. The console keeps 1,000–50,000 lines, with a maximum of 16 KiB per line. Timestamps record when output arrives; there is no guaranteed total ordering between output and error streams. Logs are exported only when requested.

Native JARs are extracted to a temporary session folder, with CRC checks, size limits, and path traversal protection. That folder is removed after the game ends. ZIP64 and encrypted native archives are rejected.

Launch failures and game exits include an exit code. Closing the launcher while Minecraft is running asks for confirmation, because forcing the game to stop can lose unsaved progress.

## Discord

Rich Presence uses local Discord IPC. The built-in public application ID is `1556990126570078248`; no Discord token or login is required. Discord must be running and allow activity sharing.

The activity shows launcher or game state, profile and loader, and elapsed play time for games started directly by Obsidian. Turning the feature off or closing the launcher clears its activity. Credentials, player names, UUIDs, and local paths are not sent to Discord.

## Verification

The existing 2.3 tests cover synthetic profiles, mock Java child processes, argument construction, log redaction, interface settings and layout, authentication with a local test server, and Discord IPC with a local test server.

Real Minecraft launches, real token or Microsoft sign-in, real Discord activity, and a full installer or upgrade run have not been verified in the development environment.
