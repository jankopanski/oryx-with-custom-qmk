# Moonlander Custom QMK Layout (`AWDDG`)

Custom 5-layer ZSA Moonlander firmware in [`AWDDG/`](./AWDDG/) built for **macOS virtual Colemak**, with a standalone thumb-gated Home-Row Mod engine (`hrm.h`/`hrm.c`), mirrored Number/Symbol layers, a symmetric Mod-Alpha Tri-Layer (Layer 4), macOS Navigation (Layer 3), One-Shot Shift, Auto Shift toggle, Repeat Key, and a 32-test QMK unit test suite.

> **Source of truth:** `AWDDG/` in this git repository is the source of truth for the layout. Do **not** edit the layout in Oryx and run the Oryx merge workflow — build locally and flash with Keymapp instead.
>
> **Hardware & OS note:**
> - **Board revision:** ZSA Moonlander **Rev A** (`zsa/moonlander/reva`, `stm32-dfu` bootloader, PID `0xDF11` / `57105`). Do **not** build `zsa/moonlander/revb` (it uses a different bootloader PID `0x2003` and Keymapp will reject it with `[DFU] no valid firmware data found for pid 57105`).
> - **Host layout:** macOS **virtual Colemak** (firmware sends standard QWERTY keycodes; macOS maps them to Colemak).

## Documentation

- [Layout & 5-Layer Reference](./docs/layout_analysis.md) — complete visual maps for Layers 0–4, shortcut table, and HRM state machine
- [Home-Row Mods & Timing Guide](./docs/HOME_ROW_MODS.md) — `hrm.c` behavior, Colemak mapping, `AS_TOGG`, `OSM(MOD_LSFT)`, `QK_REP`, and per-key tap/hold settings
- [HRM Unit Test Plan & Suite Catalog](./docs/hrm_test_plan.md) — all 32 Google Test cases and harness setup

## Local Test, Build & Flash Workflow

1. **Sync sources and run all 32 unit tests:**
   ```bash
   ./test.sh
   ```
2. **Build the Moonlander firmware binary (Rev A):**
   ```bash
   make -C ../qmk_firmware zsa/moonlander/reva:AWDDG
   ```
   Output binary: `../qmk_firmware/zsa_moonlander_reva_AWDDG.bin`
3. **Flash to your Moonlander:**
   Open [Keymapp](https://www.zsa.io/flash#flash-keymap) and flash `zsa_moonlander_reva_AWDDG.bin`.

---

## Upstream Origin (`oryx-with-custom-qmk`)

This repository was originally forked from ZSA's [oryx-with-custom-qmk](https://blog.zsa.io/oryx-custom-qmk-features) template. The `.github/workflows/fetch-and-build-layout.yml` workflow remains in the repo for reference, but local QMK builds are recommended now that `AWDDG/keymap.c` is maintained directly in code.
