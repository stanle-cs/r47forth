.PHONY: all clean sim test test_asan dmcp dmcpr47 dmcp5 dmcp5r47 docs testPgms both_asan dist_windows dist_macos dist_linux dist_dmcp dist_dmcpr47 dist_dmcp5 dist_dmcp5r47 repeattest simc47 simr47 t47 check-custom-pkg-sim check-custom-pkg-dmcp check-custom-pkg-dmcp5 pkg_build

all: sim
both: sim simr47

EXE =
ifeq ($(OS),Windows_NT)
  EXE = .exe
endif

BUILD_PC = build.sim
DIST_DIR_PC = build.sim
XVFB =
FORCENEW_TESTPGMS =
GMP_MESON_BUILD  = subprojects/gmp-6.2.1/meson.build
GMP_MESON_SOURCE = subprojects/packagefiles/gmp-6.2.1/meson.build
DMCP_PACKAGE = 4

ifneq ($(filter t47,$(MAKECMDGOALS)),)
  BUILD_PC    = build.sim.t47
  DIST_DIR_PC = build.sim.t47
  T47CP_C47 = cp c47$(EXE) t47$(EXE)
  T47CP_R47 = cp r47$(EXE) t47$(EXE)
else
  T47CP_C47 = :
  T47CP_R47 = :
endif

$(GMP_MESON_BUILD): $(GMP_MESON_SOURCE)
	rm -rf subprojects/gmp-6.2.1

clean: $(GMP_MESON_BUILD)
	rm -f wp43$(EXE)
	rm -f c47$(EXE)
	rm -f r47$(EXE)
	rm -f t47$(EXE)
	rm -rf wp43-windows* wp43-macos* wp43-linux* wp43-dm42*
	rm -rf c47-windows* c47-macos* c47-linux* c47-dmcp* r47-dmcp*
	rm -rf build build.sim build.sim.t47 build.dmcp build.dmcp.* build.dmcp5 build.rel build.rel.debug pkg_dist
	rm -f src/generated/*.c src/generated/constantPointers.h src/generated/softmenuCatalogs.h
	rm -rf PROGRAMS/ALLPGMS
	rm -f src_files_stamp testPgms_stamp

MESON_SETUP_SIM = meson setup $(BUILD_PC) --buildtype=custom -DRASPBERRY=`tools/onARaspberry` -DDECNUMBER_FASTMUL=true -DCUSTOM_PKG=$(CUSTOM_PKG)

build.sim:
	$(MESON_SETUP_SIM)
	@echo "$(CUSTOM_PKG)" > $(BUILD_PC)/.custom_pkg_stamp

# check-custom-pkg-sim (package-manager surface, PROPOSED_SPEC_CHANGES.md
# revision 2, New Decision 7): `build.sim` above is a directory-existence-
# gated Make target — once $(BUILD_PC) exists, its recipe is SKIPPED on
# every later invocation, so switching CUSTOM_PKG (including to/from empty)
# between `make sim`/`make test`/etc. calls would otherwise silently keep
# building against the PREVIOUS package's stale shadow tree: no error, no
# rebuild, no signal at all. This phony target runs on EVERY invocation
# (unlike build.sim itself) and forces --reconfigure through directly when
# the requested CUSTOM_PKG differs from the stamp left by the last
# successful setup. Set CUSTOM_PKG_RECONFIGURE=1 to force the same
# reconfigure when the package name is unchanged but refresh has changed
# patches/ or introduced a files/ entry that the existing shadow does not
# contain yet. This preserves the incremental build directory while
# rematerializing the complete package overlay and regenerating its source
# list.
#
# Independent of f=1: f=1 only controls whether the GMP subproject is
# force-rebuilt, in build.dmcp/build.dmcp5's own recipe below — it has no
# relationship to CUSTOM_PKG or the shadow tree. `make sim f=1
# CUSTOM_PKG=packages/other-pkg` still gets a forced reconfigure here for
# the package-overlay reason, independent of whatever f=1 does elsewhere.
check-custom-pkg-sim:
	@if [ -d "$(BUILD_PC)" ]; then \
		STAMP="$(BUILD_PC)/.custom_pkg_stamp"; \
		if [ ! -f "$$STAMP" ]; then \
			LAST="<unknown-pre-existing-build>"; \
		else \
			LAST="$$(cat "$$STAMP")"; \
		fi; \
		if [ "$$LAST" != "$(CUSTOM_PKG)" ] || [ "$(CUSTOM_PKG_RECONFIGURE)" = "1" ]; then \
			if [ "$(CUSTOM_PKG_RECONFIGURE)" = "1" ]; then \
				echo "CUSTOM_PKG_RECONFIGURE=1: rematerializing package overlay in $(BUILD_PC)"; \
			else \
				echo "CUSTOM_PKG changed ('$$LAST' -> '$(CUSTOM_PKG)'): forcing reconfigure of $(BUILD_PC)"; \
			fi; \
			$(MESON_SETUP_SIM) --reconfigure && \
			echo "$(CUSTOM_PKG)" > "$$STAMP"; \
		fi; \
	fi

build.sim.t47:
	meson setup $(BUILD_PC) --buildtype=custom -DRASPBERRY=`tools/onARaspberry` -DDECNUMBER_FASTMUL=true -Dc_args="-DT47"

both_asan: clean
ifeq ($(OS),Windows_NT)
	@echo "Warning: AddressSanitizer not supported on Windows MinGW, building without ASAN"
	meson setup $(BUILD_PC) --buildtype=custom -DDECNUMBER_FASTMUL=true -Dc_args="-Wno-deprecated-declarations"
else
	meson setup $(BUILD_PC) --buildtype=custom -DDECNUMBER_FASTMUL=true -Dc_args="-Wno-deprecated-declarations" -Db_sanitize=address
endif
	cd $(BUILD_PC) && ninja sim
	cd $(BUILD_PC) && ninja simr47
	cp $(BUILD_PC)/src/c47-gtk/c47$(EXE) ./
	cp $(BUILD_PC)/src/c47-gtk/r47$(EXE) ./
ifneq ($(OS),Windows_NT)
	@ASAN_OK=true; \
	if ! otool -L ./c47 2>/dev/null | grep -q asan && ! ldd ./c47 2>/dev/null | grep -q asan; then \
		echo "\033[1;31m"; \
		echo "WARNING: ASAN NOT ENABLED IN c47! The c47 binary was not built with AddressSanitizer."; \
		echo "\033[0m"; \
		ASAN_OK=false; \
	else \
		echo "\033[1;32m ASAN successfully enabled in c47\033[0m"; \
	fi; \
	if ! otool -L ./r47 2>/dev/null | grep -q asan && ! ldd ./r47 2>/dev/null | grep -q asan; then \
		echo "\033[1;31m"; \
		echo "WARNING: ASAN NOT ENABLED IN r47! The r47 binary was not built with AddressSanitizer."; \
		echo "\033[0m"; \
		ASAN_OK=false; \
	else \
		echo "\033[1;32m ASAN successfully enabled in r47\033[0m"; \
	fi; \
	if [ "$$ASAN_OK" = "false" ]; then exit 1; fi
endif

build.rel:
	meson setup $(BUILD_PC) --buildtype=release -DCI_COMMIT_TAG=$(CI_COMMIT_TAG) -DDECNUMBER_FASTMUL=true

build.rel.debug:
	meson setup $(BUILD_PC) --buildtype=custom  -DCI_COMMIT_TAG=$(CI_COMMIT_TAG) -DDECNUMBER_FASTMUL=true

MESON_SETUP_DMCP  = meson setup build.dmcp.p$(DMCP_PACKAGE) --cross-file=src/c47-dmcp/cross_arm_gcc.build -DDMCPVERSION=dmcp -DCI_COMMIT_TAG=$(CI_COMMIT_TAG) -DDECNUMBER_FASTMUL=true -DDMCP_PACKAGE=$(DMCP_PACKAGE) -DCUSTOM_PKG=$(CUSTOM_PKG)
MESON_SETUP_DMCP5 = meson setup build.dmcp5 --cross-file=src/c47-dmcp5/cross_arm_gcc.build -DDMCPVERSION=dmcp5 -DCI_COMMIT_TAG=$(CI_COMMIT_TAG) -DDECNUMBER_FASTMUL=true -DCUSTOM_PKG=$(CUSTOM_PKG)

build.dmcp:
	$(if $(f),test -d build.dmcp.p$(DMCP_PACKAGE) ||,rm -rf build.dmcp.p$(DMCP_PACKAGE);) meson setup build.dmcp.p$(DMCP_PACKAGE)  --cross-file=src/c47-dmcp/cross_arm_gcc.build  -DDMCPVERSION=dmcp  -DCI_COMMIT_TAG=$(CI_COMMIT_TAG) -DDECNUMBER_FASTMUL=true -DDMCP_PACKAGE=$(DMCP_PACKAGE) -DCUSTOM_PKG=$(CUSTOM_PKG)
	@echo "$(CUSTOM_PKG)" > build.dmcp.p$(DMCP_PACKAGE)/.custom_pkg_stamp

build.dmcp5:
	$(if $(f),test -d build.dmcp5 ||,rm -rf build.dmcp5;) meson setup build.dmcp5 --cross-file=src/c47-dmcp5/cross_arm_gcc.build -DDMCPVERSION=dmcp5 -DCI_COMMIT_TAG=$(CI_COMMIT_TAG) -DDECNUMBER_FASTMUL=true -DCUSTOM_PKG=$(CUSTOM_PKG)
	@echo "$(CUSTOM_PKG)" > build.dmcp5/.custom_pkg_stamp

# check-custom-pkg-dmcp / -dmcp5: same New-Decision-7 reasoning as
# check-custom-pkg-sim above. The primary case these matter for is f=1
# (build.dmcp/build.dmcp5 above already wipe-and-reconfigure unconditionally
# WITHOUT f=1 — f=1 is exactly what makes them keep/reuse an existing build
# dir, which is also when a CUSTOM_PKG change could otherwise go unnoticed).
check-custom-pkg-dmcp:
	@if [ -d "build.dmcp.p$(DMCP_PACKAGE)" ]; then \
		STAMP="build.dmcp.p$(DMCP_PACKAGE)/.custom_pkg_stamp"; \
		if [ ! -f "$$STAMP" ]; then \
			LAST="<unknown-pre-existing-build>"; \
		else \
			LAST="$$(cat "$$STAMP")"; \
		fi; \
		if [ "$$LAST" != "$(CUSTOM_PKG)" ] || [ "$(CUSTOM_PKG_RECONFIGURE)" = "1" ]; then \
			if [ "$(CUSTOM_PKG_RECONFIGURE)" = "1" ]; then \
				echo "CUSTOM_PKG_RECONFIGURE=1: rematerializing package overlay in build.dmcp.p$(DMCP_PACKAGE)"; \
			else \
				echo "CUSTOM_PKG changed ('$$LAST' -> '$(CUSTOM_PKG)'): forcing reconfigure of build.dmcp.p$(DMCP_PACKAGE)"; \
			fi; \
			$(MESON_SETUP_DMCP) --reconfigure && \
			echo "$(CUSTOM_PKG)" > "$$STAMP"; \
		fi; \
	fi

check-custom-pkg-dmcp5:
	@if [ -d "build.dmcp5" ]; then \
		STAMP="build.dmcp5/.custom_pkg_stamp"; \
		if [ ! -f "$$STAMP" ]; then \
			LAST="<unknown-pre-existing-build>"; \
		else \
			LAST="$$(cat "$$STAMP")"; \
		fi; \
		if [ "$$LAST" != "$(CUSTOM_PKG)" ] || [ "$(CUSTOM_PKG_RECONFIGURE)" = "1" ]; then \
			if [ "$(CUSTOM_PKG_RECONFIGURE)" = "1" ]; then \
				echo "CUSTOM_PKG_RECONFIGURE=1: rematerializing package overlay in build.dmcp5"; \
			else \
				echo "CUSTOM_PKG changed ('$$LAST' -> '$(CUSTOM_PKG)'): forcing reconfigure of build.dmcp5"; \
			fi; \
			$(MESON_SETUP_DMCP5) --reconfigure && \
			echo "$(CUSTOM_PKG)" > "$$STAMP"; \
		fi; \
	fi

sim: check-custom-pkg-sim $(BUILD_PC)
	cd $(BUILD_PC) && ninja sim
	cp $(BUILD_PC)/src/c47-gtk/c47$(EXE) ./
	$(T47CP_C47)
	install -C $(BUILD_PC)/src/generateCatalogs/softmenuCatalogs.h src/generated/
	install -C $(BUILD_PC)/src/generateConstants/constantPointers.h src/generated/
	install -C $(BUILD_PC)/src/generateConstants/constantPointers.c src/generated/
	install -C $(BUILD_PC)/src/generateConstants/constantPointers2.c src/generated/
	install -C $(BUILD_PC)/src/ttf2RasterFonts/rasterFontsData.c src/generated/

simc47: sim

simr47: check-custom-pkg-sim $(BUILD_PC)
	cd $(BUILD_PC) && ninja simr47
	cp $(BUILD_PC)/src/c47-gtk/r47$(EXE) ./
	$(T47CP_R47)
	install -C $(BUILD_PC)/src/generateCatalogs/softmenuCatalogs.h src/generated/
	install -C $(BUILD_PC)/src/generateConstants/constantPointers.h src/generated/
	install -C $(BUILD_PC)/src/generateConstants/constantPointers.c src/generated/
	install -C $(BUILD_PC)/src/generateConstants/constantPointers2.c src/generated/
	install -C $(BUILD_PC)/src/ttf2RasterFonts/rasterFontsData.c src/generated/
	
ifeq ($(MAKECMDGOALS),t47)
t47: simr47
else
t47:
	@:
endif

dmcp: check-custom-pkg-dmcp build.dmcp
	cd build.dmcp.p$(DMCP_PACKAGE) && ninja dmcp

dmcpr47: check-custom-pkg-dmcp build.dmcp
	cd build.dmcp.p$(DMCP_PACKAGE) && ninja dmcp_r47

dmcp5: check-custom-pkg-dmcp5 build.dmcp5
	cd build.dmcp5 && ninja dmcp5

dmcp5r47: check-custom-pkg-dmcp5 build.dmcp5
	cd build.dmcp5 && ninja dmcp5_r47

docs: build.sim
	cd $(BUILD_PC) && ninja docs

testPgms: build.sim
	cd $(BUILD_PC) && ninja testPgms
	$(if $(CUSTOM_PKG),@echo "CUSTOM_PKG active: skipping res/testPgms copy",mkdir -p res/testPgms && cp $(BUILD_PC)/src/generateTestPgms/testPgms.bin res/testPgms/)

test: clean check-custom-pkg-sim build.sim testPgms
	cd $(BUILD_PC) && ninja test

# pkg_build PKG=<dir> (PROPOSED_SPEC_CHANGES.md revision 2, New Decision
# 5): the sole sanctioned way to produce a distributable package
# artifact — test-gated (a package that fails its own test suite
# produces no artifact) and size-limited against the actual assembled
# zip, not an estimate.
#
# NOTE on the PKG variable name: `PKG` is also used elsewhere in this
# Makefile for the numbered DMCP build-variant pattern targets
# (build.dmcp.p$(PKG), dmcp_pkg$(PKG), dist_dmcp_pkg$(PKG) — unrelated
# to CUSTOM_PKG package overlays). Because Make expands $(PKG) in
# target names at parse time, invoking `make pkg_build dmcp_pkg1
# PKG=packages/my-pkg` in the SAME command line would corrupt those
# other targets' names. In practice pkg_build is invoked alone
# (`make pkg_build PKG=packages/my-pkg`), which is unaffected — flagged
# here, and in the implementation report, as a known naming collision
# rather than silently risked.
# Distributable-artifact tripwire, NOT a firmware budget. The original 200000
# came from a "DM42-class flash/RAM" rationale; the target is R47 specifically
# (ruled 2026-07-25), so that rationale is void. Zip bytes are not flash bytes
# either: forth-core's payload is ~75% test_dict_reloc.c, which is the self-test
# suite and compiles to nothing on device (PC_BUILD && FORTH_DEBUG_SELFTEST) --
# its real flash cost is measured per stage and reported in the commit.
# What is left worth catching is a package that has accidentally swallowed a
# build directory or a binary, which is an order of magnitude away, not 15%.
PKG_MAX_SIZE = 1000000

pkg_build:
	@if [ -z "$(PKG)" ]; then \
		echo "ERROR: pkg_build requires PKG=<package-dir>, e.g. make pkg_build PKG=packages/my-pkg" >&2; \
		exit 1; \
	fi
	@if [ ! -d "$(PKG)" ]; then \
		echo "ERROR: pkg_build: package directory not found: $(PKG)" >&2; \
		exit 1; \
	fi
	$(MAKE) clean
	$(MAKE) test CUSTOM_PKG=$(PKG)
	python3 tools/pkg_patch_refresh.py $(PKG)
	@mkdir -p "$(CURDIR)/pkg_dist"
	@rm -f "$(CURDIR)/pkg_dist/$(notdir $(PKG)).zip"
	@cd $(PKG) && zip -r -q "$(CURDIR)/pkg_dist/$(notdir $(PKG)).zip" \
		$(if $(wildcard $(PKG)/patches),patches) \
		$(if $(wildcard $(PKG)/files),files)
	@# GPL-3 sec.4/5: a conveyed copy must carry the licence. The zip leaves the
	@# repo behind, so COPYING has to travel inside it.
	@cd "$(CURDIR)" && zip -q -g "$(CURDIR)/pkg_dist/$(notdir $(PKG)).zip" COPYING
	@ZIP="$(CURDIR)/pkg_dist/$(notdir $(PKG)).zip"; \
	if [ ! -f "$$ZIP" ]; then \
		echo "ERROR: pkg_build: $$ZIP was not created (package has neither patches/ nor files/?)" >&2; \
		exit 1; \
	fi; \
	ACTUAL_SIZE=$$(stat -c%s "$$ZIP" 2>/dev/null || stat -f%z "$$ZIP"); \
	if [ "$$ACTUAL_SIZE" -gt "$(PKG_MAX_SIZE)" ]; then \
		echo "ERROR: pkg_build: $$ZIP is $$ACTUAL_SIZE bytes, exceeds PKG_MAX_SIZE=$(PKG_MAX_SIZE) bytes" >&2; \
		rm -f "$$ZIP"; \
		exit 1; \
	fi; \
	echo "pkg_build: $$ZIP is $$ACTUAL_SIZE bytes (limit $(PKG_MAX_SIZE))"

test_asan: clean testPgms
ifeq ($(OS),Windows_NT)
	@echo "Warning: AddressSanitizer not supported on Windows MinGW, building without ASAN"
	meson setup $(BUILD_PC) --buildtype=custom -DRASPBERRY=`tools/onARaspberry` -DDECNUMBER_FASTMUL=true -Dc_args="-Wno-deprecated-declarations" -DCUSTOM_PKG=$(CUSTOM_PKG) --reconfigure
else
	meson setup $(BUILD_PC) --buildtype=custom -DRASPBERRY=`tools/onARaspberry` -DDECNUMBER_FASTMUL=true -Dc_args="-Wno-deprecated-declarations" -Db_sanitize=address -DCUSTOM_PKG=$(CUSTOM_PKG) --reconfigure
endif
	cd $(BUILD_PC) && ninja test

# ----------------------------
# Incremental repeattest

# Stamp file updated if any .c or .h changes
SRC_FILES := $(shell find src -name '*.c' -o -name '*.h')
src_files_stamp: $(SRC_FILES)
	touch $@

testPgms_stamp: build.sim src_files_stamp
	cd $(BUILD_PC) && ninja testPgms
	mkdir -p res/testPgms
	cp $(BUILD_PC)/src/generateTestPgms/testPgms.bin res/testPgms/
	touch $@

repeattest: check-custom-pkg-sim build.sim testPgms_stamp
	cd $(BUILD_PC) && ninja test

build.rel/wiki: build.rel
	rm -fr $(BUILD_PC)/wiki
	git clone https://gitlab.com/rpncalculators/c43.wiki.git $(BUILD_PC)/wiki

ifeq ($(CI_COMMIT_TAG),)
  WIN_DIST_DIR = c47-windows
  MAC_DIST_DIR = c47-macos
  LINUX_DIST_DIR = c47-linux
  DMCP_DIST_DIR = c47-dmcp
  DMCPR47_DIST_DIR = r47-dmcp
  DMCP5_DIST_DIR = c47-dmcp5
  DMCP5R47_DIST_DIR = r47-dmcp5
  VERSION = $(shell git describe --match=NeVeRmAtCh --always --abbrev=8 --dirty=-mod)
else
  WIN_DIST_DIR = c47-windows-$(CI_COMMIT_TAG)
  MAC_DIST_DIR = c47-macos-$(CI_COMMIT_TAG)
  LINUX_DIST_DIR = c47-linux-$(CI_COMMIT_TAG)
  DMCP_DIST_DIR = c47-dmcp-$(CI_COMMIT_TAG)
  DMCPR47_DIST_DIR = r47-dmcp-$(CI_COMMIT_TAG)
  DMCP5_DIST_DIR = c47-dmcp5-$(CI_COMMIT_TAG)
  DMCP5R47_DIST_DIR = r47-dmcp5-$(CI_COMMIT_TAG)
  VERSION = $(CI_COMMIT_TAG)
  #
  FORCENEW_TESTPGMS = 1
endif

dist_install_PC: sim simr47
	mkdir -p $(DIST_DIR_PC)/res/
	cp $(BUILD_PC)/src/c47-gtk/c47$(EXE) $(DIST_DIR_PC)/
	cp $(BUILD_PC)/src/c47-gtk/r47$(EXE) $(DIST_DIR_PC)/
	cp -r res/PROGRAMS $(DIST_DIR_PC)/res/
	cp -r res/SCRIPTS $(DIST_DIR_PC)/res/
	cp -r res/STATE $(DIST_DIR_PC)/res/
	cp res/c47_pre.css $(DIST_DIR_PC)/res/
	cp res/C47.png $(DIST_DIR_PC)/res/
	cp res/C47short.png $(DIST_DIR_PC)/res/
	cp res/R47.png $(DIST_DIR_PC)/res/
	cp res/R47short.png $(DIST_DIR_PC)/res/
	cp res/fonts/C47__StandardFont.ttf $(DIST_DIR_PC)/

dist_testPgms_PC: testPgms dist_install_PC
	mkdir -p $(DIST_DIR_PC)/res/testPgms/
	cp res/testPgms/testPgms.bin res/testPgms/testPgms.txt $(DIST_DIR_PC)/res/testPgms/
	cd $(DIST_DIR_PC) && $(XVFB) ./c47$(EXE) --writeexportall
	cd $(DIST_DIR_PC)/PROGRAMS/ && zip -r ../res/testPgms/testPgms.zip ALLPGMS
	cp $(DIST_DIR_PC)/res/testPgms/testPgms.zip res/testPgms/

dist_windows: BUILD_PC = build.rel
dist_windows: DIST_DIR_PC = $(WIN_DIST_DIR)
dist_windows: build.rel/wiki dist_testPgms_PC
	rm -rf $(WIN_DIST_DIR)/PROGRAMS
	mkdir -p $(WIN_DIST_DIR)/res/tone
	cp res/tone/*.wav $(WIN_DIST_DIR)/res/tone/
	cp res/c47.reg $(WIN_DIST_DIR)/
	cp res/c47.cmd $(WIN_DIST_DIR)/
	cp $(BUILD_PC)/wiki/Installation-on-Windows.md $(WIN_DIST_DIR)/readme.txt
	#zip the package
	zip -r c47-windows.zip $(WIN_DIST_DIR)
	rm -rf $(WIN_DIST_DIR)

dist_macos: BUILD_PC = build.rel
dist_macos: DIST_DIR_PC = $(MAC_DIST_DIR)
dist_macos: dist_testPgms_PC
	rm -rf $(MAC_DIST_DIR)/PROGRAMS
	#zip the package
	zip -r c47-macos.zip $(MAC_DIST_DIR)
	rm -rf $(MAC_DIST_DIR)

dist_linux: BUILD_PC = build.rel.debug
dist_linux: DIST_DIR_PC = $(LINUX_DIST_DIR)
dist_linux: dist_testPgms_PC
	rm -rf $(LINUX_DIST_DIR)/PROGRAMS
	# debug setting (defined by custom meson buildtype) as workaround for issue #470
	strip $(LINUX_DIST_DIR)/c47 # workaround #470
	cp res/c47.xpm $(LINUX_DIST_DIR)/res/
	#zip the package
	zip -r c47-linux.zip $(LINUX_DIST_DIR)
	rm -rf $(LINUX_DIST_DIR)

DIST_DIR_DM = $(DMCP_DIST_DIR)
PKG =
dist_install_DM$(PKG): _DIST_DIR_DM = $(DIST_DIR_DM)$(if $(PKG),-pkg$(PKG),)
dist_install_DM$(PKG): build.rel/wiki
	mkdir -p $(_DIST_DIR_DM)/resources
	cp -r res/offimg/Egypt/. $(_DIST_DIR_DM)/offimg
	cp -r res/offimg/Norway/. $(_DIST_DIR_DM)/offimg
	cp -r res/offimg/Netherlands/. $(_DIST_DIR_DM)/offimg
	cp -r res/offimg/From\ WP43/. $(_DIST_DIR_DM)/offimg
	cp -r res/offimg/General/. $(_DIST_DIR_DM)/offimg
	cp -r res/offimg/HP\ related/. $(_DIST_DIR_DM)/offimg
	cp -r res/offimg/C47/. $(_DIST_DIR_DM)/offimg
	cp -r res/PROGRAMS $(_DIST_DIR_DM)
	cp -r res/STATE $(_DIST_DIR_DM)
	cp res/keymaps/keymap_DM42.bin $(_DIST_DIR_DM)/resources

ifeq ($(FORCENEW_TESTPGMS),)
  DIST_TESTPGMS_DM = dist_testPgms_DM
else
  DIST_TESTPGMS_DM = dist_testPgms_forcenew_DM
endif

dist_testPgms_DM: dist_install_DM$(PKG)
	mkdir -p $(DIST_DIR_DM)
	mkdir -p $(DIST_DIR_DM)/resources
	cp res/testPgms/testPgms.bin res/testPgms/testPgms.txt res/testPgms/testPgms.zip $(DIST_DIR_DM)/resources

dist_testPgms_forcenew_DM: dist_testPgms_PC dist_install_DM$(PKG)
	mkdir -p $(DIST_DIR_DM)
	mkdir -p $(DIST_DIR_DM)/resources
	cp $(BUILD_PC)/res/testPgms/testPgms.bin $(BUILD_PC)/res/testPgms/testPgms.txt $(BUILD_PC)/res/testPgms/testPgms.zip $(DIST_DIR_DM)/resources

dist_dmcp5: DIST_DIR_DM = $(DMCP5_DIST_DIR)
dist_dmcp5: dmcp5 $(DIST_TESTPGMS_DM)
	cp build.dmcp5/src/c47-dmcp5/C47.pg5 $(DIST_DIR_DM)
	cp res/dmcp5/SwissMicros/DM42_qspi_3.x.bin $(DIST_DIR_DM)/resources
	zip -r $(DIST_DIR_DM)/resources/C47.map.zip build.dmcp5/src/c47-dmcp5/C47.map
	cp res/dmcp5/install_C47_on_DM42n.txt $(DIST_DIR_DM)
	cp res/PACKAGES.md $(DIST_DIR_DM)/PACKAGES.txt
	zip -r c47-dmcp5.zip $(DIST_DIR_DM)
	rm -rf $(DIST_DIR_DM)

dist_dmcpr47: DIST_DIR_DM = $(DMCPR47_DIST_DIR)
dist_dmcpr47: dmcpr47 $(DIST_TESTPGMS_DM)
	cp build.dmcp.p$(DMCP_PACKAGE)/src/c47-dmcp/R47.pgm build.dmcp.p$(DMCP_PACKAGE)/src/c47-dmcp/R47_qspi.bin $(DMCPR47_DIST_DIR)
	cp res/keymaps/keymap_R47.bin $(DMCPR47_DIST_DIR)
	zip -r $(DMCPR47_DIST_DIR)/resources/R47.map.zip build.dmcp.p$(DMCP_PACKAGE)/src/c47-dmcp/C47.map
	cp $(BUILD_PC)/wiki/Installation-on-a-DM42.md $(DMCPR47_DIST_DIR)/install_C47_on_DM42_readme_on_wiki.txt
	zip -r r47-dmcp.zip $(DMCPR47_DIST_DIR)
	rm -rf $(DMCPR47_DIST_DIR)

dist_dmcp5r47: DIST_DIR_DM = $(DMCP5R47_DIST_DIR)
dist_dmcp5r47: dmcp5r47 $(DIST_TESTPGMS_DM)
	mkdir -p $(DMCP5R47_DIST_DIR)/resources/
	cp build.dmcp5/src/c47-dmcp5/R47.pg5 $(DMCP5R47_DIST_DIR)
	cp res/keymaps/keymap_R47.bin $(DMCP5R47_DIST_DIR)/resources
	cp res/dmcp5/SwissMicros/DM42_qspi_3.x.bin $(DMCP5R47_DIST_DIR)/resources
	zip -r $(DMCP5R47_DIST_DIR)/resources/R47.map.zip build.dmcp5/src/c47-dmcp5/C47.map
	cp res/dmcp5/install_R47_on_DM32.txt $(DMCP5R47_DIST_DIR)/resources
	cp res/dmcp5/update_R47.txt $(DMCP5R47_DIST_DIR)
	cp res/combo/R47_combo.py $(DMCP5R47_DIST_DIR)/
	cp res/combo/DMCP5_flash_3.57.bin $(DMCP5R47_DIST_DIR)/
	cd $(DMCP5R47_DIST_DIR) && python3 R47_combo.py $(VERSION)
	rm $(DMCP5R47_DIST_DIR)/R47_combo.py
	rm $(DMCP5R47_DIST_DIR)/DMCP5_flash_3.57.bin
	zip -r r47-dmcp5.zip $(DMCP5R47_DIST_DIR)
	rm -rf $(DMCP5R47_DIST_DIR)

#
# DMCP package 1, 2 and 3 separate builds
#

.PHONY: dmcp_pkg1 dmcp_pkg2 dmcp_pkg3 dmcp_pkgs_all
.PHONY: dist_dmcp_pkg1 dist_dmcp_pkg2 dist_dmcp_pkg3
.PHONY: dist_dmcp_pkgs_1_2 dist_dmcp_pkgs_small dist_dmcp_pkgs_all

build.dmcp.p$(PKG): DMCP_PACKAGE = $(PKG)
build.dmcp.p$(PKG):
	$(if $(f),test -d build.dmcp.p$(PKG) ||,rm -rf build.dmcp.p$(PKG);) meson setup build.dmcp.p$(PKG) \
	  --cross-file=src/c47-dmcp/cross_arm_gcc.build \
	  -DDMCPVERSION=dmcp \
	  -DCI_COMMIT_TAG=$(CI_COMMIT_TAG) \
	  -DDECNUMBER_FASTMUL=true \
	  -DDMCP_PACKAGE=$(PKG) \
	  -DCUSTOM_PKG=$(CUSTOM_PKG)

dmcp_pkg$(PKG): build.dmcp.p$(PKG)
	cd build.dmcp.p$(PKG) && ninja dmcp

dist_dmcp_pkg$(PKG): dmcp_pkg$(PKG)
dist_dmcp_pkg$(PKG): _DIST_DIR_DM = $(DIST_DIR_DM)-pkg$(PKG)
dist_dmcp_pkg$(PKG): _BUILD_DIR_DM = build.dmcp.p$(PKG)
dist_dmcp_pkg$(PKG): dist_install_DM$(PKG) $(DIST_TESTPGMS_DM)
	cp $(_BUILD_DIR_DM)/src/c47-dmcp/C47.pgm $(_BUILD_DIR_DM)/src/c47-dmcp/C47_qspi.bin $(_DIST_DIR_DM)
	zip -r $(_DIST_DIR_DM)/resources/C47.map.zip $(_BUILD_DIR_DM)/src/c47-dmcp/C47.map
	cp $(BUILD_PC)/wiki/Installation-on-a-DM42.md $(_DIST_DIR_DM)/install_C47_on_DM42_readme_on_wiki.txt
	cp res/PACKAGES.md $(_DIST_DIR_DM)/PACKAGES.txt
	zip -r c47-dmcp-pkg$(PKG).zip $(_DIST_DIR_DM)
	rm -rf $(_DIST_DIR_DM)

dmcp_pkgs_all:
	$(MAKE) PKG=1 dmcp_pkg1
	$(MAKE) PKG=2 dmcp_pkg2
	$(MAKE) PKG=3 dmcp_pkg3

dist_dmcp_pkgs_all:
	$(MAKE) PKG=1 dist_dmcp_pkg1
	$(MAKE) PKG=2 dist_dmcp_pkg2
	$(MAKE) PKG=3 dist_dmcp_pkg3

dist_dmcp_pkgs_1_2:
	$(MAKE) PKG=1 dist_dmcp_pkg1
	$(MAKE) PKG=2 dist_dmcp_pkg2

dist_dmcp_pkgs_small:
	$(MAKE) PKG=2 dist_dmcp_pkg2
	$(MAKE) PKG=3 dist_dmcp_pkg3

#dist_dmcp: dist_dmcp_pkgs_1_2
#dist_dmcp: dist_dmcp_pkgs_small

# this syntax is only needed, if target is not one of dist_dmcp_pkgs_* pre-defined targets
dist_dmcp:
	$(MAKE) PKG=$(DMCP_PACKAGE) dist_dmcp_pkg$(DMCP_PACKAGE)
