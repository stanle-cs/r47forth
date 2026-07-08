# C47 firmware — Forth package project
- Upstream: gitlab.com/rpncalculators/c43. Do not modify upstream files;
  all work goes through the external package system (see packages/README).
- Forth work lives in packages/forth-core/. DESIGN.md there is authoritative.
- Build: BUILD.md details all the builds. compile_commands.json exists; respect it.
- Target: DM42-class hardware. Flash/RAM budget is the binding constraint —
  report arena high-water marks with any dictionary change.
- Conventions: single clean commit series per stage, branch per stage.
- Division of labor: you (Claude) do design, review, and hard debugging.
  A local model implements from your specs — write specs with zero unstated
  decisions, exact struct layouts, and pseudocode for stateful logic.