# v0.5.3 test evidence

Current final reports: focused input 47, Practice 14, resources 21, functional 57, health/winner/rematch 36 — **175/175 automatic assertions passed**. Read [candidate evidence and limitations](../../docs/CANDIDATE-0.5.3.md).

Run each script separately only in a complete UE 5.5.3 asset-bearing project with this native module/editor helpers built, work saved, and no PIE active. The public repository does not include the necessary Content assets. Scripts load the polished map, start/stop real PIE worlds, write their report beside the script, restore PIE settings, and request editor exit. Back up reports before rerunning.

`verify_regression.py` is a wrapper; keep `tests/health_20261004/verify_regression.py`, `work/polish_release/verify.py`, and `work/windup_review/verify.py` at their included relative locations. The other four scripts are self-contained editor test drivers, not standalone Python simulations.

Production keys are queued on each local controller's game-world tick so client RPCs execute through the real input path. Position/resource fixtures, injected projectile receiver cases, injected bot decisions, and duplicate callback tests are explicitly labelled. One focused case observes an actual launched projectile collision. There is no claim of physical keyboard/controller QA, packaged multiplayer, or independent-PC connectivity.

The prior candidate's 14 arithmetic assertions and 23 head-motion checks were not rerun for this update; those historical results are not part of the current 175 total.
