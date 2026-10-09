# v0.5.2 Stuffing Candidate

Prepared and tested October 8, 2026 using Unreal Engine 5.5.3. This source snapshot and candidate ZIP supersede the older main revision `2414cf4a6f6334c1e4bd3a0407d4d7c637956578` for current code review. Older releases remain available. It is not a promise of improved fun or completed polish.

## Exact playable artifact

Release tag: `v0.5.2`; in-game project version: `0.5.2-StuffingCandidate`.

File: `PillowWars-v0.5.2-StuffingCandidate-Windows.zip`.

Size: `451524234` bytes. SHA-256:

`8c255bbdd4e7c1a57be21074b9553f1bdff898240e32b63673b07c80a90631d6`

Extract everything and run `Windows/PillowWars.exe`. Do not redistribute just the EXE. Windows 64-bit, unsigned Development build; Unreal Editor is not required. If the runtime is missing, the complete package includes `Windows/Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe`. Source-only GitHub ZIPs lack binary assets and are not playable. The candidate archive's documentation was captured before publication; use the release page for current upload status.

## Changes and reasons

- Integer thousandths are server authority; floating-point values are display projections. Guard/refill integrate elapsed server time with fractional carry instead of the previous 0.05-second cap.
- Tap F retains the 8-stuffing cost and no new spill. Accepted charged attacks cost `8 + 24 * power`, up to 32, and stage up to `8 * power` of that payment as shared loose stuffing. A full charge loses at least 24 permanently. Rejected attacks do not debit or spill. Failed placement or full pools leaves the spill as loss.
- E cover commits 25. Normal break, natural expiry, or nearby owner V reclaim stages up to 15 as shared salvage; at least 10 is lost. Owner departure/elimination or round cleanup gives no salvage. Generation fencing and first-claim resolution prevent duplicate credit.
- Ambient/funded piles have separate four-pile caps (eight total). Paid merges choose an eligible old serial, capped at 96. Ambient renewal is explicit supply; salvage never creates unpaid resources.
- Grounded, still, visible-pile rest transfers finite material at 24/second after the one-second action delay; guarding drains 8/second. Throwing resets the action delay.
- Private denial and newly affordable basic-attack messages explain failed input. Optional T solo exercise demonstrates dummy hit, labeled resource depletion, denied attack, paid-pile recovery and a second hit; T can skip/restart it. Multiplayer starting resources are unchanged.
- This publication also brings the public source up to the already-developed health/critical/bot/head-reaction preview. It does not import Pranav's portable arithmetic packet or a fruit-fly-brain model.

Accounting invariant: held + committed cover + loose + terminal loss = starting balances + explicit supply. Attack payment precedes spill staging. Existing weapon/character animation files are unchanged relative to the preserved v0.5.1 local preview; movement, cooldown, server hit checks, scenery blocking, one hit per opponent, health/critical curves, elimination and winner/rematch rules are retained. No maps or character content were edited in this candidate pass.

## Final automated results

| Scope | Passed | Configuration |
| --- | ---: | --- |
| Resource transactions, feedback, cleanup | 21/21 | Listen-server/client PIE; normal F/G/E/V/R input |
| Optional solo practice exercise | 9/9 | Standalone PIE; normal T/F input |
| Health, costs, criticals, movement, elimination | 36/36 | Listen-server/client and one server-controlled bot; includes native formula assertions |
| Existing functional regression | 57/57 | Listen-server/client; movement, swing variants, charge, guard, cover, finite refill, gravity throw, elimination, winner and reset |
| Head-motion regression | 23/23 | Listen-server/client; gain curves, attack input and replicated state samples |

Total **146/146**. Final scripts completed without exception; final editor processes exited 0. Final result JSON and exact editor Python scripts are in `tests/stuffing_candidate/`.

All network worlds ran in one editor process on one Windows laptop. Input was queued through production player InputKey handlers on the relevant local controller, including client RPCs. Positions and health/resource/gauge fixtures were scripted. Duplicate cover callbacks were explicitly injected; natural V reclaim and expiry were separate checks. This is not a human keyboard/focus test, packaged match or independent-machine validation.

The actual standalone production arithmetic header passed **14/14** C++ assertions covering 10/20/30/60/144 Hz time partitions, small carry and a 1.5-second interval. This is not a native Unreal timing/hitch matrix. Five final editor logs contained 351 resource-audit snapshots, with zero observed nonzero residuals, negative aggregate balances or pool-cap violations; not a proof of every lifecycle ordering or load case. Logs/private backups are retained locally, not published.

## Build and runtime evidence

Editor compile/link/metadata passed with MSVC 14.38 and Windows SDK 10.0.26100. Windows Development BuildCookRun completed build/cook/pak/IoStore/stage/archive with exit 0. The complete ZIP includes launch/game executables, cooked data, prerequisites and reports; no Saved/Logs/checkpoint content was found during archive inspection. Prior ZIP and source/config/binary checkpoint are preserved locally.

Graphics-enabled unattended packaged startup/quit loaded `PillowWarsBedroomPolished` and exited 0. That launch selected D3D11/SM5 fallback after D3D12 was unavailable, with offscreen mode and requested 640x360. It did not assess normal gameplay or visual quality.

**Unresolved:** NullRHI startup/quit loaded the map and logged shutdown but exited `-1073741819` (`0xC0000005`). The preserved v0.5.1 executable reproduced the failure with the same options and separate settings. A temporary NNE-plugin-disable comparison still failed. No new crash report/call stack or Windows application error entry was found, and no native debugger was available. Cause unknown; no unproven gameplay/plugin fix applied. This is not proof that ordinary rendered play crashes or that this candidate caused it.

## Pranav's latest review items: still open

His elapsed-time carry/retry example is a **hypothetical adapter-boundary risk**, not an observed game failure. The current native `SettleResources` validates its book/state before integration and normally consumes time through `Clock.LastTime`; it is not his portable Accrue/Transfer adapter. However, effects/callbacks occur before the final time marker, so failure/reentrancy ordering warrants review. Internal-failure injection after budget calculation followed by retry, and zero/partial receipt replay acceptance tests have **not been executed**. Ledger conservation alone would not detect double-counted elapsed time. Ordinary gameplay denial must not rewind legitimate settled guard/refill time.

His emailed portable listings were not imported or verified against their hashes: email wrapping breaks a C++ character literal and Python assertions. A raw UTF-8 attachment or source commit is needed for faithful packet reproduction. His previously reported packet results are not our reproduced results.

Build-matched uncut, normal-speed isolated/repeated head-hit settling and carry/charge/throw clips remain requested. No new candidate clip or before/after visual verdict is included here. When recording, identify ZIP/hash/source revision, map/mode/player roles, settings/resolution, game FPS versus capture FPS, and exact inputs. Mark critical/charge context unknown if not visible or evidenced.

## Short manual pass and remaining limits

1. Practice: WASD/mouse/Space; check physical input and camera. Tap F; valid dummy contact should add one hit, miss none. Review charge wind-up, release and settling at normal speed.
2. Full charged release: spend 32, stage up to 8 paid material. Leave, return and recover only its finite contents while grounded/still with line of sight. No remote refill.
3. T optional exercise: judge private hints, depletion/recovery and skipping. E then nearby owner V: no through-wall or other-owner reclaim.
4. Two computers, same candidate: test ready gating, host/client hits, health, Daze, charge/cooldown spam, guard/refill, Q arc and 15-second recharge, elimination, matching winner and both R votes.
5. Measure exhausted downtime, pile camping versus contested fights, match length and novice clarity. More features do not by themselves prove better fun.

Separate-PC/LAN/Internet, packaged two-player, fresh 8/10-player networking, native timing/lifecycle stress, ten full matches, physical gamepads, low-end performance and normal-speed animation/HUD quality remain unverified. Historical September 8/10-player smoke results are not current-candidate verification.
