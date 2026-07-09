Audit complete. I traced the full data flow and verified findings empirically against the live build.sim (compile_commands.json, build.ninja, the shadow tree itself). You were right to assume a family: the known generator bug is actually two stacked bugs — fixing the one you found would visibly do nothing, because the freshly generated catalog header is then masked by the checked-in src/generated/ copy at higher include priority. Plus a frozen-tree failure mode on Windows, a Makefile target that silently drops the package, and a set of resolver defects. 16 findings, ranked.

---
Findings, ranked

F1 — CRITICAL (the known bug, mechanism pinned): generator source lists are never remapped

Mechanism. Phase 2 of the top-level meson.build (lines 130–167) rewrites only c47_src and c47_inc. But generateCatalogs_src and generateTestPgms_src are materialized as files() objects during subdir('src/c47') (src/c47/meson.build:232–242) — they point at src/c47/items.c etc. forever. src/generateCatalogs/meson.build and src/generateTestPgms/meson.build consume those variables directly.
Evidence. compile_commands.json: both generators compile -c ../src/c47/items.c while the runtime compiles custom_pkg_shadow/items.c.
Failure scenario. Package adds a menu item via items.c override → softmenuCatalogs.h and testPgms.bin are generated from upstream items.c → menus empty, test corpus lacks the item. Ripple: testSuite links shadow c47_src plus softmenuCatalogs_h generated from upstream — a franken-binary that tests overridden code against un-overridden catalogs.
Fix. Remap both variables in Phase 2 (top-level meson.build — see fix plan; unavoidable and justified there). Consumers src/generateCatalogs/, src/generateTestPgms/, src/testSuite/ then inherit correctness with zero edits.

F2 — CRITICAL (new): the checked-in src/generated/ headers mask the freshly generated ones — F1's fix alone is a silent no-op

Mechanism. c47_inc = ['.', '../generated'] puts -I../src/generated (committed, pregenerated softmenuCatalogs.h, constantPointers.h) at include index 8; meson auto-appends the custom_target output dirs (-Isrc/generateCatalogs) at index 10. softmenus.c does #include "softmenuCatalogs.h"; the shadow dir doesn't contain it (shadow mirrors only src/c47/), so the committed stale copy wins over the freshly built one.
Evidence. Verified -I order on the shadow softmenus.c TU: ../src/generated (8) precedes src/generateCatalogs (10). Today both copies lack FORTH entries so it's latent; the moment F1 is fixed, the fresh header gains the package items and still doesn't get included.
Failure scenario. You implement F1's fix, rebuild, menu still empty, no error anywhere. Days lost concluding the fix "didn't work."
Fix. When CUSTOM_PKG is active, build c47_inc as ('custom_pkg_shadow', 'src/generateCatalogs', 'src/generateConstants', 'src/c47', 'src/generated') so fresh-generated precedes checked-in. Conditional → vanilla builds keep upstream's committed-copy semantics. Both dirs exist source-side, so include_directories() validation passes; duplicate -Is later in the line are harmless.

F3 — CRITICAL on Windows / copy-mode, silent: copy-fallback shadow is a frozen tree

Mechanism. link_or_copy (resolver:74–83) falls back to shutil.copy2 on the first OSError — permanently, for all subsequent files, with no warning printed. Windows without symlink privilege hits this immediately; CUSTOM_PKG_SHADOW_COPY=1 opts in. copy2 preserves mtimes, ninja depfiles reference shadow paths → an edit to any upstream or package file changes nothing ninja can see. Bare ninja silently builds the old code. Worse, a mid-walk fallback yields a hybrid tree: some files live-update through symlinks, others are frozen copies.
Failure scenario. Windows dev edits keyboard.c, rebuilds, runs — old behavior. No error, ever, until a reconfigure happens for an unrelated reason. (build-test.sh's comment shows you've been bitten by this class; on Linux/symlinks the staleness folklore is actually false for content edits — stat follows symlinks — it's copy mode where it's real.)
Fix. (a) Print a loud stderr warning the moment fallback engages ("copy mode: bare ninja will NOT see source edits; reconfigure required") — meson surfaces it via warning(). (b) Optionally add a resolver --sync mode run as a build_always_stale step; but that requires injecting a dependency into upstream consumer targets, so for now: loud warning + docs + build-test.sh's reconfigure-by-default convention. Revisit if Windows becomes primary.

F4 — HIGH, silent: a declared-but-missing override file degrades to a warning and builds upstream

Mechanism. Resolver:109–117: not isfile(pkg_file) → stderr warning → continue. Exit code stays 0; check: true is satisfied; the upstream symlink stays in the shadow. Configure-time warnings are also never repeated on subsequent bare-ninja runs, so the signal exists exactly once, buried in setup noise.
Failure scenario. pkg_override_headers = ['mathematics/factorial.h'] but the file is at factorial.h (path typo) → package builds green with the override absent. For forth-core, a typo'd programming/lblGtoXeq.c would mean H2 never lands — Forth-from-program silently broken while all PC self-tests (which don't exercise that path) stay green.
Fix. Make both "not found in package" and "does not exist in src/c47" fatal (sys.exit(1)). A declaration that can't be honored is a broken package manifest, not a preference. Keep duplicate-override cases as warnings (they're legitimate layering).

F5 — HIGH: make test_asan silently drops the package (and is likely broken outright)

Mechanism. Makefile:147–148: test_asan: clean testPgms then a bare meson setup $(BUILD_PC) — no $(CUSTOM_PKG_FLAG), no buildtype — followed by a second flagged meson setup (151/153) on a now-already-configured dir, which modern meson refuses without --reconfigure. Either make aborts, or (older meson) the unflagged configure wins and the ASan test run is package-less.
Failure scenario. make test_asan CUSTOM_PKG=packages/forth-core → tests run without Forth compiled in; ASan coverage of the package is zero while appearing to pass. custom_package/README.md's target table (line 97) documents test_asan as propagating — false.
Fix. Delete line 148 (or fold flags + --reconfigure into one invocation matching siblings). Makefile is package-manager surface (README lists it as such), not upstream-source.

F6 — HIGH, systemic: the test corpus is generated-from-upstream and copied into the shared source tree

Mechanism. testPgms Makefile target: ninja testPgms then cp $(BUILD_PC)/src/generateTestPgms/testPgms.bin res/testPgms/ — a build artifact written into the checked-in source tree, shared by all configurations.
Failure scenario. Today: package builds regenerate a corpus that cannot contain package items (F1) and overwrite res/testPgms/. After F1 is fixed: a CUSTOM_PKG build writes a package-specific corpus into the shared tree; the next vanilla build/test consumes contaminated test programs — or commits them.
Fix. When CUSTOM_PKG is set, skip the res/ copy (or copy to a per-package path) with an explicit message. Keep vanilla behavior byte-identical.

F7 — MEDIUM-HIGH: mixed-header generator TUs (the include-path asymmetry you asked about)

Mechanism. In today's generator builds, generateCatalogs.c resolves "c47.h" via -I (shadow first — index 4) while the items.c it links resolves its quote-chain from its own directory src/c47/ (GCC searches dir-of-includer first). One binary, two header universes.
Failure scenario. Package overrides items.h changing a constant/struct consumed by both TUs (e.g. a catalog count) → generator's own TU uses the shadow value, items.c's TU uses upstream → generated tables internally inconsistent, silently.
Fix. F1's fix resolves this as a side effect only if implemented by remapping sources into the shadow (dir-of-file becomes the shadow, whole quote-chain coheres). Stating it explicitly so nobody "fixes" F1 by other means (e.g. stubs/extra -I) and leaves this live.

F8 — MEDIUM: the stub problem is ad hoc, and forth-core hits it the moment F1 lands

Mechanism. Generator builds of items.c stub every referenced handler inside #if GENERATE_CATALOGS || GENERATE_TESTPGMS (upstream items.c:775–1665, void fnXxx(uint16_t){} each). The forth override adds extern fnForthOuter/fnForthCall (lines 7–8) + table rows (4701–4702) but no stubs in that section (verified). After F1's remap, generateCatalogs fails to link: undefined fnForthOuter/fnForthCall. Loud, at least — but it must land in the same series, and the rule is nowhere documented.
Fix. Add the two stubs to the forth override's generator section; document in custom_package/README.md: "an items.c override that adds rows must add matching stubs inside the GENERATE_* block." (A pkg_generator_stubs mechanism is unnecessary — the whole-file override already owns that section. runFunction/forthResolveXEQ at items.c:683 is inside the !GENERATE_* region, so only the two pointers need stubs.)

F9 — MEDIUM: rmtree(ignore_errors=True) + os.path.exists → stale shadow entries survive removal, silently

Mechanism. Resolver:68 swallows all rmtree failures (locked file on Windows/NFS → partial wipe, no error). Overlay then uses os.path.exists (line 121), which returns False for dangling symlinks, so a stale entry isn't removed and copy2 writes through it. A removed package's override can keep shadowing upstream indefinitely.
Fix. After rmtree, verify the dir is gone else exit(1) with a clear message; use os.path.lexists/os.remove unconditionally in a try.

F10 — MEDIUM: unvalidated override paths can write (and delete) outside the shadow tree

Mechanism. rel is joined unchecked: rel='../generated/softmenuCatalogs.h' passes the isfile(src_c47_dir/rel) check (file exists!) and link_or_copy writes to shadow_dir/../generated/... — into the build root outside the shadow. An absolute rel makes os.path.join discard shadow_dir entirely, and line 122's os.remove(dst_path) deletes an arbitrary existing path.
Failure scenario. Not a hostile-package threat model — a typo'd spec silently mutating paths outside the shadow, or deleting a file, during meson setup.
Fix. Normalize and enforce containment (os.path.commonpath([shadow_dir, dst]) == shadow_dir, same for the upstream check) → exit(1) on escape.

F11 — MEDIUM: the resolver's meson parser fails silently on upstream format drift

Mechanism. re.search(r'c47_src\s*=\s*files\((.*?)\)') stops at the first ). If upstream ever reformats to files(...) + files(...) or nests a call, the tail is silently dropped → missing sources → link errors far from the cause. strip_comments also breaks on a # inside a quoted filename.
Fix. Post-parse sanity: every parsed entry must exist under src/c47/ (exit(1) naming the first miss), and reject a captured group that still contains (. Three lines, converts silent truncation into a named configure error.

F12 — MEDIUM: the shadow looks like a scratch tree but writes through to upstream — via your own tooling

Mechanism. compile_commands.json points every c47 TU at build.sim/custom_pkg_shadow/... (verified). clangd/IDE "go to definition → edit" edits the symlink target: src/c47/ upstream (violating the system's core promise) or the package master — without the developer knowing which. CLAUDE.md says to respect compile_commands.json, which makes this the default editing path.
Fix. Resolver drops a DO_NOT_EDIT_shadow_tree.txt sentinel at the shadow root; document prominently. (Symlink permissions can't portably prevent it; copies would trade this for F3. Documentation + sentinel is the honest mitigation.)

F13 — LOW-MEDIUM: package meson.build executes in top-level scope; typos are silently ignored

Mechanism. subdir(pkg) runs in the parent scope. pkg_override_source = [...] (missing s) is just an unused variable — no error, no override, green build. Packages can also clobber any top-level variable and inject project-wide args (forth-core already adds -DFORTH_DEBUG_SELFTEST to all native targets, generators included — visible in the generator compile lines).
Fix. After each subdir(pkg), warn when all three pkg_* variables are empty ("package %s declared nothing"); document the scope rules and the injection footgun.

F14 — LOW: docs document upstream only

Doxyfile:792 INPUT = ../../src/c47; docs/code is also subdir'd before Phase 2. Overrides and package sources never appear in generated docs. Silent, cosmetic. Fix: document as a limitation (or conditionally append package dirs to INPUT — low value now).

F15 — LOW: dead (byte-identical) overrides are undetected → invisible whole-file pinning

A byte-identical override (exactly what forth-core's placeholders were until H1) silently pins the file: subsequent upstream changes to it are masked in package builds, and divergence grows with every upstream merge. Fix: filecmp.cmp(pkg_file, upstream_file) in the overlay loop → warning "override is byte-identical to upstream (dead shadow)".

F16 — LOW: README's rebuild guidance is wrong in both directions

custom_package/README.md:218–225 claims meson configure won't re-run resolution (changing -DCUSTOM_PKG dirties coredata → next ninja regen re-executes the full meson.build including run_command, rebuilding the shadow) while prescribing rm -rf of all build dirs — cargo-cult that masks the actual staleness rule (F3: copy mode; plus structural upstream changes not reflected in any tracked meson.build, where a new upstream header is served by the -Isrc/c47 fallback until the next reconfigure — benign until that file is later overridden). Fix: rewrite that section with the true rules once F3's warning lands. Verify the regen claim empirically during implementation; if some meson version does skip it, escalate F3's sync option.

What's genuinely correct (verified, for calibration, not reassurance): runtime + testSuite c47_src remapping works (softmenus.c compiles from the shadow); custom sources are compiled from the package dir and tracked directly by ninja (no staleness); precedence is deterministic last-wins matching the README; on Linux/symlink mode, content edits to overrides and upstream propagate correctly to bare ninja.

---
Consolidated fix plan (for a literal implementer)

Constraints honored: src/c47/meson.build and all src/*/meson.build consumers untouched. The only upstream-file edits are in the top-level meson.build — unavoidable because the generator variables are created in one upstream file and consumed in others, and the sole non-upstream interception point between those subdir() calls is the top-level Phase 2 block that already exists for exactly this purpose. Every new meson line sits inside if custom_pkg_list != [] → byte-for-byte no-op for vanilla builds.

Step 1 — forth-core generator stubs (inert now, required by Step 3).
File: packages/forth-core/items.c. Inside the #if defined(GENERATE_CATALOGS) || defined(GENERATE_TESTPGMS) block (mirroring line ~787ff), add:
void fnForthOuter(uint16_t unusedButMandatoryParameter) {} and void fnForthCall(uint16_t unusedButMandatoryParameter) {} (and remove/guard the top-of-file externs if the compiler flags a conflict — it won't; signatures match).
Verify: ./packages/forth-core/build-test.sh green (change is dead code until Step 3); syntax check under generator define: gcc -fsyntax-only -DGENERATE_CATALOGS -DPC_BUILD -DLINUX -DOS64BIT -Ibuild.sim/custom_pkg_shadow -Isrc/c47 -Isrc/generated $(pkg-config --cflags gtk+-3.0) packages/forth-core/items.c.

Step 2 — resolver hardening (F4, F9, F10, F11, F15, F3-warning, F12-sentinel).
File: tools/resolve_c47_src.py only. (a) missing pkg file / no upstream match → sys.exit(1); (b) rmtree then if os.path.isdir(shadow_dir): exit(1); use os.path.lexists + unconditional remove-in-try at overlay; (c) containment check on every dst_path and upstream_file via os.path.commonpath; (d) parser sanity: every parsed source must be isfile under src/c47 else exit 1 with the name; reject ( inside the captured files(...) group; (e) filecmp.cmp → "dead shadow" warning; (f) one-time stderr warning when copy fallback engages; (g) write DO_NOT_EDIT_shadow_tree.txt into the shadow root. Also gate a new --gen-lists flag that additionally emits GENCAT:<shadow path> / GENTST:<shadow path> lines parsed from generateCatalogs_src / generateTestPgms_src — emitted only with the flag so this step changes no meson-visible output.
Verify: python3 tools/resolve_c47_src.py --shadow src/c47/meson.build . /tmp/claude-1000/-home-stan-c43/6a16eb59-f125-4cc1-811e-87fae3e115c9/scratchpad/shadowtest "nosuchpkg:items.c"; echo $? → nonzero + clear message. Then ./packages/forth-core/build-test.sh green.

Step 3 — top-level meson.build Phase 2: remap generators + include order (F1, F2, F7, F13-warning).
File: meson.build (top-level), inside the existing if custom_pkg_list != [] Phase 2 block only. (a) pass --gen-lists to the resolver; route stdout lines by prefix: unprefixed → c47_src as today, GENCAT:→ set_variable('generateCatalogs_src', ...), GENTST: → set_variable('generateTestPgms_src', ...) (absolute shadow-path strings are valid meson sources). (b) set_variable('c47_inc', include_directories('custom_pkg_shadow', 'src/generateCatalogs', 'src/generateConstants', upstream_incs)). (c) in Phase 1's foreach, warn if a package declared none of the three variables.
Verify (all four, in order): ./packages/forth-core/build-test.sh green; python3 -c "import json;db=json.load(open('build.sim/compile_commands.json'));print([e['file'] for e in db if 'generateCatalogs.p' in e['output'] and e['file'].endswith('items.c')])" → path contains custom_pkg_shadow; grep -c FORTH build.sim/src/generateCatalogs/softmenuCatalogs.h ≥ 1; the softmenus.c TU's -Isrc/generateCatalogs index < -I../src/generated index (rerun my Step-0 python probe).

Step 4 — Makefile: test_asan + corpus guard (F5, F6).
File: Makefile. Delete the bare meson setup $(BUILD_PC) at line 148 (the flagged invocations at 151/153 remain, add --reconfigure if the dir may pre-exist). In the testPgms recipe, wrap the cp ... res/testPgms/ in $(if $(CUSTOM_PKG), @echo "CUSTOM_PKG active: skipping res/testPgms copy", cp ...).
Verify: make -n test_asan CUSTOM_PKG=packages/forth-core | grep "meson setup" → every setup line carries -DCUSTOM_PKG; make -n testPgms CUSTOM_PKG=packages/forth-core | grep -c "cp .*res/testPgms" → 0.

Step 5 — docs (F8, F3, F12, F14, F16).
File: custom_package/README.md. Add: the items.c-override stub rule; the copy-mode staleness rule ("symlink mode: edits propagate to bare ninja; copy mode: reconfigure required — the resolver warns when copy mode is active"); the shadow-is-write-through warning naming compile_commands.json; correct the Rebuilding section (drop the rm -rf ritual, state when reconfigure is actually needed); note the docs-target limitation.
Verify: proofread; ./packages/forth-core/build-test.sh still green (docs-only).

Order matters: Step 1 before Step 3 (else generateCatalogs link-fails with forth-core active); Step 2 before Step 3 (meson consumes the new flag). Steps 4–5 are independent. After Step 3, rerun the whole verification battery once with CUSTOM_PKG unset (make sim) to confirm the vanilla build is untouched — that's the no-op guarantee the upstream invariant demands.