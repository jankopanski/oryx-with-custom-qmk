# Moonlander Custom QMK Layout (`AWDDG`)

Custom 5-layer ZSA Moonlander firmware in [`AWDDG/`](./AWDDG/) built for **macOS virtual Colemak**, with a standalone thumb-gated Home-Row Mod engine (`hrm.h`/`hrm.c`), mirrored Number/Symbol layers, a symmetric Mod-Alpha Tri-Layer (Layer 4), macOS Navigation (Layer 3), One-Shot Shift, Auto Shift toggle, Repeat Key, and a 32-test QMK unit test suite.

> **Source of truth:** `AWDDG/` in this git repository is the source of truth for the layout. Do **not** edit the layout in Oryx and run the Oryx merge workflow — build locally and flash with Keymapp instead.

## Documentation

- [Layout & 5-Layer Reference](./docs/layout_analysis.md) — complete visual maps for Layers 0–4, shortcut table, and HRM state machine
- [Home-Row Mods & Timing Guide](./docs/HOME_ROW_MODS.md) — `hrm.c` behavior, Colemak mapping, `AS_TOGG`, `OSM(MOD_LSFT)`, `QK_REP`, and per-key tap/hold settings
- [HRM Unit Test Plan & Suite Catalog](./docs/hrm_test_plan.md) — all 32 Google Test cases and harness setup

## Local Test, Build & Flash Workflow

1. **Sync sources and run all 32 unit tests:**
   ```bash
   ./test.sh
   ```
2. **Build the Moonlander firmware binary:**
   ```bash
   make -C ../qmk_firmware zsa/moonlander/revb:AWDDG
   ```
   Output binary: `../qmk_firmware/zsa_moonlander_revb_AWDDG.bin`
3. **Flash to your Moonlander:**
   Open [Keymapp](https://www.zsa.io/flash#flash-keymap) and flash `zsa_moonlander_revb_AWDDG.bin`.

---

## Upstream Origin (`oryx-with-custom-qmk`)

This repository was originally forked from ZSA's [oryx-with-custom-qmk](https://blog.zsa.io/oryx-custom-qmk-features) template. The `.github/workflows/fetch-and-build-layout.yml` workflow remains in the repo for reference, but local QMK builds are recommended now that `AWDDG/keymap.c` is maintained directly in code.
