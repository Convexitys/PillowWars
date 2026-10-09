# Pillow Wars: Dream Resonance

A student-built Unreal Engine 5.5.3 party-combat prototype: small pajama ninjas, oversized pillows, spring-neck bobbleheads, and a bedroom turned into a battleground.

![Historical v0.3.0 Practice gameplay; not a v0.5.2 visual review](docs/images/gameplay.png)

Current source: **v0.5.2-StuffingCandidate**, October 8, 2026. Use the matching candidate ZIP below for reviewing the new resource rules; the older v0.3.0 release remains available for comparison. This is a test candidate, not a claim of finished balance or visual polish.

## Play without Unreal

**[Download Pillow Wars for Windows - v0.5.2 Stuffing Candidate (451.5 MB)](https://github.com/Convexitys/PillowWars/releases/download/v0.5.2/PillowWars-v0.5.2-StuffingCandidate-Windows.zip)**

1. Download the complete game ZIP above. GitHub's green **Code > Download ZIP** is source code, not the playable game.
2. Extract the entire ZIP, then open the `Windows` folder and run `PillowWars.exe`.
3. Keep its `Engine` and `PillowWars` folders beside it. The EXE alone is not the game.
4. Select **Practice** for solo play. Read `READ_ME.md` and `Documentation` for controls, rules, and testing limits.

[Candidate release notes and checksum](https://github.com/Convexitys/PillowWars/releases/tag/v0.5.2). Exact ZIP: **451,524,234 bytes**, SHA-256 `8c255bbdd4e7c1a57be21074b9553f1bdff898240e32b63673b07c80a90631d6`. Artifact integrity is separate from gameplay testing. Documentation inside the ZIP was captured before publication; its not-yet-published status line is historical. This release page is the publication status.

Windows 64-bit only. A compatible graphics card and Microsoft Visual C++ runtime are required. The build includes the Unreal prerequisite installer. No Unreal Editor installation is needed to play. macOS, Linux, mobile, and browser play are not supported by this Windows build. The executable is unsigned; no code-signing certificate is included.

This repository contains the custom C++ gameplay source, sanitized default configuration, screenshots, and test documentation. It is **not a complete clone-and-run Unreal project**: the editor's binary content, Epic template/Starter Content dependencies, and development-only MCP plugin are not in this public source snapshot. See [asset/build scope](docs/ASSETS-AND-BUILD.md). The packaged game is the player-facing distribution.

## The game loop

- Tap **F** to cycle left/right/overhead pillow swings. Hold **F** for up to two seconds, then release for a charged circular uppercut.
- Hits reduce Health and build Daze, increasing later knockback. Zero health or falling out eliminates you without competitive in-round respawning. Win by being the last dreamer standing; the match tracks first-to-three round wins.
- Attacks and blocking spend pillow stuffing. Stop beside a feather pile to crouch and restuff; moving or attacking interrupts it.
- Throw a gravity-affected back pillow with **Q**, then wait 15 seconds for it to recharge.
- Guard, create temporary pillow cover, and use the Resonance timing window to turn defense into an opportunity.
- Critical probability rises from 1.5% at zero Resonance to 55% at full Resonance; critical damage is 1.75x. Server-controlled bots are available for practice, not fruit-fly-brain integration.
- Charged attacks cost `8 + 24 * power` and spill up to `8 * power` of that already-paid stuffing. Cover costs 25; normal break/expiry or owner **V** reclaim salvages up to 15 into a shared finite pile. Tap attacks still cost 8 with no new spill. See [candidate rules and evidence](docs/CANDIDATE-0.5.2.md).

## Controls

| Action | Keyboard |
| --- | --- |
| Menu navigation / select | Arrow keys / Enter |
| Movement / camera / jump | WASD / mouse / Space |
| Three tap swings / charged uppercut | Tap F / hold F and release |
| Guard / temporary pillow cover / throw | G / E / Q |
| Reclaim nearby owned cover / optional solo exercise | V / T in Practice |
| Pause / back | Esc |
| Lobby character selection / ready | C / Enter |
| Open LAN host / start match | H to open host; H again after both/all players are ready |
| Join by IPv4 address | Join Party menu, or J in lobby |
| Round reset / rematch vote | R |

For solo testing, select **Practice**. A labeled character dummy reports hits and Daze; it does not count as a player. Gamepad bindings are shown in-game but have not received the same fresh physical-controller QA as keyboard input.

## Multiplayer scope

Designed around a listen host and direct IPv4 joining. Use the same game version on both Windows computers. This is not Steam/Epic matchmaking and does not provide party invites, NAT traversal, or a relay service. Separate-network Internet connectivity is not yet verified; do not assume a friend on another network can connect automatically. Do not disable your firewall to troubleshoot.

Host: open Play / Party Lobby and press **H** to open port 7777. Share your LAN IPv4 address privately with the other tester. The other tester uses **Join Party** and that address. Each player presses **Enter** to ready up; the host presses **H** again to start. Hosting alone does not start a match; use Practice for solo play. Packaged two-player matches and independent-PC networking remain unverified.

Current v0.5.2 verification: **146/146 automated Unreal checks** across resources (21), optional solo exercise (9), health/costs/criticals (36), existing two-player functional regression (57), and head-motion regression (23). Network tests ran as listen host/client worlds in one editor process on one laptop; queued production input events and scripted fixtures were used. A separate C++ test passed 14 arithmetic assertions. Editor build and Windows packaging passed; a graphics-enabled unattended packaged startup/quit passed with D3D11/SM5 fallback. Headless NullRHI exit still fails with `0xC0000005`, also reproduced with the older v0.5.1 executable; cause unknown. Fresh 8/10-player, separate-PC, normal-speed visual, novice and long-match tests are **not** claimed. Historical September results are retained separately in [test evidence and limits](docs/TESTING.md).

## Implementation

Unreal C++, replicated player/match state, server-authoritative contact sweeps and damage, procedural poseable-mesh animation, generated character geometry, and editor-Python test/capture scripts. Original character and furniture asset preparation used Python and Blender.

Start with `PillowWarsGameMode.cpp` (rules/combat), `PillowWarsResources.cpp` and `PillowWarsResourceMath.h` (server resource ledger/time carry), `PillowWarsWeapon.cpp` (pose/contact/replication), `PillowWarsPlayerController.cpp` (input/frontend), and `PillowWarsMatchState.cpp` (state/HUD) under `Source/PillowWars/`. Tests and final result JSON are in `tests/stuffing_candidate/`.

## Submission

Prepared for Next Byte Hacks: V4. [Official rules](https://next-byte-hacks-v4.devpost.com/rules) require a project description, tech stack, and a public code repository. See [submission checklist](docs/SUBMISSION.md). Eligibility, team details, and the work's event-date compliance must be confirmed by the team. No placement or prize is guaranteed.
