# C47 firmware — Forth package project
- Upstream: gitlab.com/rpncalculators/c43. Do not modify upstream files;
  all work goes through the external package system (see design-docs/package-manager/README.md).
- Packages are a flat working area mirroring upstream paths; no meson.build,
  nothing to declare. `tools/pkg_patch_refresh.py` regenerates patches/+files/,
  which are generated output — the build reads ONLY those, never your edits
  directly. Gate: ./packages/forth-core/build-test.sh (it refreshes first).
- Forth work lives in packages/forth-core/. DESIGN.md there is authoritative;
  DESIGN-HISTORY.md is its non-normative amendment trail.
- Build: BUILD.md details all the builds. compile_commands.json exists; respect it.
- Target: R47 on DM42n (DMCP5). DM42 compatibility is best-effort, never
  design-binding (ruled 2026-07-15). RAM/arena discipline is binding — report
  arena high-water marks with any dictionary change. Flash increases are fine
  when justified: record the measured `make dmcp5r47` delta in the stage
  commit instead of designing around bytes.
- Conventions: single clean commit series per stage, branch per stage.
- Division of labor: you (Claude) do design, review, and hard debugging.
  A local model implements from your specs — write specs with zero unstated
  decisions, exact struct layouts, and pseudocode for stateful logic.