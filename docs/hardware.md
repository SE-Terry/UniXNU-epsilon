# hardware.md — Surface Duo 1 (epsilon) subsystem inventory

**Status:** Phase 0 baseline (read-only research, 2026-11-16). No device was contacted.
**Primary source:** Track A research (porting-researcher, Opus 5.5), cross-checked against mainline Linux and UniXNU/zeta ports (Tracks B/C).

## Evidence labels

- **V-src** — read directly in source code, ACPI/DT tables, or build config.
- **SR** — self-reported in a README/status page; not independently verified.
- **PH** — PENDING-HARDWARE: must be confirmed by a read-only measurement on the actual device.

## Pinned firmware/reference sources

| Key | Source @ commit |
|---|---|
| MU | `WOA-Project/mu_andromeda_platforms@613e2eac87` (Surface Duo 1 firmware; `Platforms/SurfaceDuo1Pkg`) |
| BIN | `WOA-Project/SurfaceDuoBinaries@1f458b7914` (stock DXE blobs; MU submodule) |
| ACPI | `WOA-Project/SurfaceDuo-ACPI@ab2751d09d` + `acpi_oem_qcom_common@b4075a9255` |
| GUIDES | `WOA-Project/SurfaceDuo-Guides@e51bfe0cb6` |
| PATCHER | `WOA-Project/SurfaceDuoDualBootKernelImagePatcher@8de38e5804` |
| LINUX | `torvalds/linux@aebb727447` — `arch/arm64/boot/dts/qcom/{sm8150.dtsi,sm8150-microsoft-surface-duo.dts}` |

## Subsystem inventory

| Item | Finding | Status | Citation |
|---|---|---|---|
| **SoC** | Qualcomm SM8150 (msm-id 0x153). Boot DTB set includes SM8150 v1/v2 | V-src | MU `SurfaceDuo1.fdf`; `ImageResources/Epsilon/dtb` (index 11 = "SM8150 v2") |
| **CPU topology** | 8 cores, one DynamIQ cluster, MPIDR `0x000`–`0x700` | V-src | MU `AndromedaPkg/.../ArmPlatformLib.c`; ACPI `APIC.aml` |
| **Core classes** | CPU0–3 silver (A55, cap 488, domain 0); CPU4–6 gold (A76, cap 1024, domain 1); CPU7 gold+ (domain 2). **Kryo 485** | V-src | LINUX `sm8150.dtsi`; ACPI efficiency classes; `UefiPlatEpsilon.cfg` clock blocks |
| **ISA / PAC** | **ARMv8.2-A** (firmware built `-march=armv8.2-a+crypto+rcpc`). **No FEAT_PAuth expected** | flag V-src; ISA **PH** | MU `Andromeda.dsc.inc:640`. Confirm: no `paca`/`pacg` in `/proc/cpuinfo`; read `ID_AA64ISAR1_EL1` |
| **GIC** | GICv3 (GIC-500). GICD `0x17A00000`; GICR `0x17A60000` (0x100000, 8 frames); MSI `0x17A10000` (SPI base 0x340); maintenance PPI 9 | V-src | ACPI `APIC.aml`; LINUX; memory map `APSS_GIC500_*` |
| **Timer (PPIs)** | Arch-timer GSIVs: **17 secure, 18 NS-EL1, 19 virtual, 16 NS-EL2** (= PPI 1/2/3/0). MMIO frame `0x17C20000`; QTIMER `0x17C00000` | V-src | ACPI `GTDT.aml`; LINUX `timer{}` |
| **Timer freq** | 19.2 MHz assumed (only the XO is cited, in a comment) | **PH** | MU `RightDisplayDxe/Dpu.c:231`. Read `CNTFRQ_EL0` |
| **Exception level / hypervisor** | UEFI runs at **EL1**. Qualcomm **QHEE** resident at `0x85700000` (6 MiB). EL2/Hyper-V unavailable without MS-signed devcfg. QHEE has a PSCI MEM_PROTECT bug | V-src / SR | MU `ModuleEntryPoint.S`, `KernelErrataPatcherLib.c`; GUIDES `Status-SurfaceDuo1.md`. Read `CurrentEL` (PH) |
| **PSCI** | PSCI 1.0 via SMC | V-src | LINUX `psci{}` |
| **DRAM** | **6 GiB** (`PcdSystemMemorySize 0x180000000`). Banks: `0x80000000+0x3CC00000`; `0xC0000000+0x80000000`; `0x140000000+0xC0000000`. Carveout `0xBCC00000–0xC0000000` | V-src | MU `SurfaceDuo1.dsc`; `PlatformDdrBankMapLib.c` |
| **DRAM type** | LPDDR4X (not in any source read) | **PH** | — |
| **UFS** | Host `0x1D84000` (len `0x14000`), GSIV `0x129`, reset GPIO 175. Linux `1d84000.ufshc`. Driven by stock `UFSDxe` blob | V-src | ACPI `DSDT.dsl` UFS0; LINUX `&ufs_mem_hc`; MU `DXE.inc` |
| **UFS version** | MU README says "UFS 3.1" for all targets; for Duo 1 **unverified** (likely 3.0) | SR / PH | MU `README.md` |
| **Displays** | **Two** LG "LGSW43416" panels — "LG META2 DSI Command Mode 1350x1800 24bpp". Left "C3" on DSI0, right "R2" on DSI1. Command mode, DSC 1.1, 500 MHz bit clock. Right reset GPIO 6, TE GPIO 9 | V-src | ACPI `panelcfg.asl`/`panelcfg2.asl`; MU `RightDisplayDxe/HwCatalog.h` |
| **UEFI framebuffer** | **One** GOP, 1350x1800x32, from patched stock `DisplayDxe` (kept alive after EBS). DuoWOA "Display Reserved" at `0x80600000` (0x2400000). **GOP base = PH** | V-src / PH | MU `SurfaceDuo1.dsc`; `PatchedBinaries/Readme.md`; `UefiPlatEpsilon.cfg` |
| **Right display (UEFI)** | `RightDisplayDxe` (DSI1, own GOP, MDP autorefresh) added 2026-11-01, **disabled 2026-11-02** ("issues with duo 1") | V-src | MU history; `.fdf`/`.dsc` commented INF |
| **Touch / pen** | One Microsoft **N-Trig** digitizer, HID-over-SPI 0.99, `MSHW0162`, on QUP0 SE7 `0x89C000` @ 25 MHz. IRQ GPIO 3, sleep GPIO 1, reset GPIO 0. **Single-touch fallback only** natively; multitouch needs a heat-map processor | V-src | ACPI `cust_touch.asl`, DSDT `SPI8`; UEFI stock `SurfaceTouchHidDxe` |
| **Buttons** | Power = PON PWRKEY; Vol-down = PON RESIN; **Vol-up = PM8150 GPIO6** (active low) | V-src | LINUX duo.dts `gpio-keys` |
| **Hall / lid** | **TLMM GPIO 121 (0x79)**. Dual-boot shim reads `0x03D79004` bit0: high→Android, low→UEFI | V-src (2-way) | DSDT `GPH0`; PATCHER `Program.cs`; GUIDES `DualBoot.md` |
| **PMICs** | PM8150, PM8150B, PM8150L, PM8009 | V-src | LINUX duo.dts |
| **Batteries** | Two TI bq27742 fuel gauges (i2c1/i2c17 @ 0x55) | V-src | LINUX duo.dts |
| **Haptics** | DA7280 on i2c1 @ 0x4a, IRQ GPIO 42 | V-src | LINUX duo.dts |
| **GPU** | **Adreno 640** (ACPI `GPU0` QCOM053A) | V-src | LINUX `sm8150.dtsi:2253`; DSDT |
| **Debug UART** | GENI UART2 `0xA90000` (`earlycon=msm_geni_serial,0xa90000`). **No accessible pads documented on retail** | V-src / PH | ACPI `DBG2.aml`; MU `build_uefi_epsilon.sh` |
| **USB** | DWC3 `0xA600000`. Mainline sets `dr_mode="peripheral"`. UEFI bundles an XHCI host stack (Cambria blobs). Host mode on Duo = PH | V-src / PH | LINUX duo.dts; MU `DXE.inc` |
| **Watchdog** | DuoWOA PrePi **disables** APSS watchdog (`0x17C10000`) and both DSI controllers before DXE | V-src | MU `PlatformUtils.c` |
| **Windows feature state** | Displays, touch, Wi-Fi, cellular, GPU, UFS work under Windows; audio, camera, fingerprint do not | SR | GUIDES `Status-SurfaceDuo1.md` |

## Address cross-check (SM8150 vs SM8350/zeta)

These SoC addresses are the **same** on SM8150 and SM8350 in upstream Linux DT, but must still be re-derived from epsilon's live DT before use: GIC `0x17A00000`/`0x17A60000`, MPIDR `0x000..0x700`, USB DWC3 `0x0A600000` (+QSCRATCH `0x0A6F8800`), UFS host `0x01D84000`, SPMI arbiter windows, apps_smmu `0x15000000`, CNTFRQ 19.2 MHz.

**Differ — do NOT reuse zeta values:** UFS SMMU stream ID **0x300** (SM8150) vs 0xE0 (SM8350); PMIC family PM8150 vs PMK8350; DRAM map; framebuffer (Duo 1 has two panels); GPU Adreno 640 vs 660; CPU Kryo 485 vs 680; **timer PPIs** (see compatibility.md B1).

## Partition layout (LUN0, from guides — verify on device)

`ssd, persist, metadata, frp, misc, … userdata (sda6), ESP (sda7), Windows (sda8)`. **No spare/unallocated slot for an on-disk root.** Source: GUIDES `Install/Client/Partitioning-SurfaceDuo1.md`. Exact geometry is **PH** until the read-only GPT audit.

## Must-measure-on-hardware list (read-only)

PAC bits (`ID_AA64ISAR1_EL1`, cpuinfo `paca`/`pacg`); `CNTFRQ_EL0`; `CurrentEL`; which timer PPI actually fires; GOP framebuffer base; live FDT (`/sys/firmware/fdt`) + `/proc/iomem`; per-LUN GPT; `fastboot getvar all`. See phase1-plan.md §Recovery and `progress.md` P0-TA for the full non-destructive diagnostic plan.
