# v0.5.3 Input Fix Candidate

Published October 11, 2026. Fixes were implemented and tested overnight October 10–11 in the existing Unreal Engine 5.5.3 project. Prior packages/releases are preserved. No map or character asset was replaced in this update.

## Six confirmed fixes

1. V reclaims cover without also cycling appearance. B explicitly cycles gameplay appearance; gamepad Y and lobby V selection remain available.
2. F/gamepad X are blocked in Pause/Settings. Opening a menu cancels an already-held charge through the server without an attack, payment, spill, or refund. Releasing that old press after resuming does not attack; a fresh press works. Resource settlement and recovery-delay rules are retained.
3. Join Party can enter optional ports. Tab inserts a colon once; numeric editing and validation accept ports 1–65535 and reject empty/invalid ports. Validated entry does not prove a custom-port connection works.
4. Optional Practice advances after a real positive recovery reaching eight stuffing, whether or not the player first attempted an unaffordable attack. Partial recoveries below eight, inactive/skipped exercises, restart, and the denial-first route retain their behavior.
5. Bot target selection uses the same existing 160 cm vertical recovery limit as the server recovery rule. This filter is not a new navigation system or proof of complete bot routing.
6. Projectile guarding and horizontal knockback use incoming flight direction, not the thrower's location at impact. The actual collision sweep direction is preferred with velocity/last-flight fallbacks; zero/vertical contact does not invent a horizontal direction. Melee direction and combat tuning remain unchanged.

The optional feather-pile content label was deferred. Lobby visual redesign is not included in this candidate.

## Current evidence

| Suite | Passed | Scope |
| --- | --- | --- |
| Focused fixes | 47/47 | Actual listen host/client and server bot; normal queued keys for input cases; explicit direction receiver/bot decision fixtures; one actual projectile collision |
| Practice | 14/14 | Actual single-player PIE, optional T/F exercise, early/denial-first recovery, partial recovery, skip and restart |
| Resources | 21/21 | Native payment/denial, finite piles, cover reclaim/expiry, cleanup, elimination, reset and conservation; duplicate callbacks explicitly injected |
| Two-player functional | 57/57 | Readiness/loading, movement/jump, both input paths, tap/charged attacks, replication, guard, refill, cover, scenery blocking, gravity/recharge, elimination/results and reset |
| Health/winner/rematch | 36/36 | Cost and critical formulas, host/client attacks, health replication/elimination, no mid-round respawn, winner agreement and explicit rematch |

Total: **175/175 assertions across five suites**, not 175 separate matches. Real network PIE worlds ran within one editor process on one laptop. Positions, resource values, and deliberate fall placement are fixtures. This is not a claim of unscripted competitive play or networking across independent computers.

Editor C++ build and Windows Development build/cook/stage/archive passed. A graphics-enabled, offscreen D3D11 packaged startup/map-load/quit passed with exit 0. That is not an interactive packaged gameplay session. The prior candidate's unresolved packaged NullRHI exit failure was not retested or fixed here.

Initial focused failures were test setup/measurement issues: same-tick queued digits arrived out of order, late velocity samples missed already-ended guarded knockback, and a disabled bot's status was sampled after recovery ended. The corrected checks serialize digits, examine directional displacement as well as velocity, and capture the selected bot decision before waiting. Initial health-cost checks sampled after paid-spill recovery was eligible; final checks sample after contact but before the existing one-second recovery delay. No gameplay tuning was changed to satisfy these assertions. Private first-run logs/reports are preserved locally, not uploaded.

Final scripts and reports: [`tests/input_fixes`](../tests/input_fixes/). They require the complete local asset-bearing project; this public source snapshot cannot run them by itself.

## Complete Windows download

[v0.5.3 release](https://github.com/Convexitys/PillowWars/releases/tag/v0.5.3) — extract the complete ZIP, then run `Windows/PillowWars.exe`. The game does not require Unreal Editor.

Artifact: `PillowWars-v0.5.3-InputFixCandidate-Windows.zip`.
Size: **451,519,883 bytes**.
SHA-256: `705e2017d731d1d71a9c19c8e22b5096af9d33950352563e36d65ddbbf1603a5`.

The local ZIP contains 48 entries, including bootstrap, game executable, five packaged data entries, README, and prerequisite installer. No Saved, Intermediate, or Git metadata entries were included. Public asset delivery must be verified after upload; artifact integrity is separate from gameplay QA.

## Still needs human testing

- Physical keyboard/window focus, all movement keys, controller input, menu/HUD readability, and animation/clipping at normal speed.
- Two separate Windows computers running the same candidate: connect, ready/start, movement, both attacks, guarding, knockback, elimination, winner and rematch.
- Real custom-port connectivity, separate-network internet connectivity, 8–10 players, low-end performance and long-match balance.
- Optional improvements and a planned lobby redesign are not implemented by these fixes.
