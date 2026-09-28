# Verification - polished prototype

Checked September 27-28, 2026 using Unreal Engine 5.5.3 on the development Windows laptop.

| Scope | Result | What was actually exercised |
| --- | --- | --- |
| Editor C++ build | Passed | Compilation and linking with MSVC 14.38; one manifest-option linker warning |
| Single-player | 16/16 | Ten supported clear spawn probes and safe spacing, saved preferences/reload/reset, Practice loading, new mesh, W/A/S/D, jump, each tap swing hits dummy once, miss adds no hit |
| Two-player | 57/57 | One listen host + one client, real PIE replication, player input events, readiness, loading, movement/jump, tap variants, short/full uppercuts, no charge damage, hand-grip telemetry, resource/refill, guard, cover, scenery blocking, projectile gravity/recharge, elimination/winner/rematch |
| 8 and 10 players | 12/12 | Separate networked PIE worlds, same process/laptop, low runtime scalability and 640x360 windows; possessed pawns, ready start, minimum initial spacing about 877 cm, client hit/Daze across all worlds, client movement, winner agreement |
| Arena traversal | 12/12 | Actual CharacterMovement and normal jump input across bed steps, table, chest, shelf, fort, recovery ledge, training bridge, and both ramps; scripted movement directions and reset positions |

The first two-player throw sample used a fixed time delay and missed the projectile. The corrected test polls for the actual server projectile before measuring velocity. Its vertical velocity decreased from approximately +283 to +143 cm/s over the sample, consistent with gravity. No projectile gameplay code was changed to make the test pass.

## Limits

- Controlled positions, resource values, and deliberate elimination-boundary placement are test fixtures. Winner agreement is not a claim that every ring-out arose from unscripted combat.
- 8/10-player checks are local loopback smoke tests, not Internet, separate-computer, long-session, or shipping-performance certification.
- Separate-computer LAN/Internet joining, low-end hardware, physical gamepads, and long-duration balance remain manual QA items.
- The pillow uses bounded bone deformation and authored lag, not a full cloth simulation. Animation feel, brief clipping, camera obstruction during chaotic fights, and audio mix need human judgment.
- Build success, automated gameplay checks, actual screenshots, and any packaged-build smoke tests are separate evidence categories.

## Five-minute tester pass

1. Extract the complete release; start Practice. Confirm all four movement keys, mouse look, and jumping.
2. Approach the dummy. Tap F three times with pauses; each valid swing should add one hit. Swing out of range: no hit.
3. Hold F: watch the circular wind-up; no hit until release. Compare a short hold with two seconds. Review normal-speed hand/pillow contact from both sides.
4. Spend stuffing, stop beside feathers, and watch the crouch/scoop/refill. Walk away: refill stops. Idle away from feathers: no refill.
5. Throw with Q: confirm an arc and 15-second recharge. With a second computer, check readiness gating, both players' movement/attacks, matching Daze/results, a fall elimination, and R rematch.

Report game version, map, host/client role, exact inputs, expected vs actual result, and a short recording. Do not include passwords or access tokens in bug reports.
