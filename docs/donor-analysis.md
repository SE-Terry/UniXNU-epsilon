# donor-analysis.md — UniXNU donor project comparison

**Status:** Phase 0 (2026-11-16). Based on Tracks B, C, D (SM8150/XNU, zeta, iOS-27) + Track A (firmware).
**Verification note:** Every on-device **boot** result below is **self-reported (CLAIMED)** in a project README unless marked **VERIFIED-in-CI**, which means only that a *kernel build* succeeded in GitHub Actions — never that hardware booted. Nothing here was independently reproduced.

## 0. Headline conclusion

**No single repo is a complete donor.** Reuse splits across three layers, each with a different best source:

| Layer | Best source | Role |
|---|---|---|
| SoC / platform code | `guowenye/UniXNU-sm8150-common` + `guowenye/UniXNU-raphael` | Primary — ADT-driven SM8150 kits + XNU patches |
| Firmware / boot chain | `WOA-Project/mu_andromeda_platforms` (`SurfaceDuo1Pkg`) | Authoritative epsilon firmware; integrate UniXNU into it |
| Integration *pattern* + method | `wochgn/UniXNU-guacamole` (Aloha SurfaceDuo1Pkg patch) + `none24smu/UniXNU-zeta` (scripts) | How to embed UniXNU/DarwinBoot; ADT/log/probe methodology |

## 0.5 Prior art — furthest progress on this hardware (web search 2026-11-17)

Three senses of "this hardware":
- **Surface Duo 1 (epsilon — this device):** **no prior UniXNU/iOS port exists.** This project is the first.
- **SM8150 (same SoC, any device):** the proven ceiling is **iOS 16.7.16 to SpringBoard / home screen**, via the `ios-sweet` (xnu-8796, iPhone-X-era) lineage — OnePlus 7 Pro (guacamole), Redmi K20 Pro (raphael), Xiaomi Mi 9 (cepheus), Xiaomi Mi Mix 3 5G (andromeda, pushed 2026-11-09). All CLAIMED, all iOS 16, all low-star forks of one lineage. **No SM8150 device has gone past iOS 16.**
- **Surface Duo family, different chip:** the furthest any Duo has reached is the **Duo 2 (zeta, SM8350, `none24smu`)** at iOS 26.5.2 + an experimental iOS 27 branch. The 2026-10 press ("Surface Duo → iPhone Duo", iOS 26, "iPhone 17 Pro Max" = iPhone18,2 identity) is **that Duo 2 work, not the Duo 1.**

**Implication:** iOS 26/27 has only ever run on the newer **SM8350** (which has working GPU/UFS/SMP); **never on SM8150**. iOS 27 on epsilon would be unprecedented for the silicon, whereas iOS 16 is well-trodden. Reinforces the roadmap's sequencing (prove boot chain → iOS 16-era if pursuing userland → iOS 27 as the moonshot; ADR 0005/0006). Sources: techadvisor.com "iPhone Duo" article (Duo 2); github `giannoccarol/UniXNU-andromeda`, `aguerreiropereira/UniXNU-raphael`, `YangQi0408/UniXNU-cepheus`.

## 1. Donor-by-donor

### UniXNU/UniXNU @ `21d683dff6` — framework
- **What:** OpenCore-like UEFI env to boot iOS/macOS on non-Apple ARM64. 2929★. BSD-2-Clause-Patent.
- **Verified:** `profiles/os` ships exactly **one arm64 profile** (`ipados-17.7.11-21H461`, xnu-10063.141.1); the other three (`ipados-26.5.2`, `ipados-27.0.1`, `macos-27.3`) are **arm64e** (require FEAT_PAuth). `components.toml`, `devices.toml` (~30 SM8150 entries; only sheng/SM8550 arm64e-supported). `docs/porting/*` define format-2 device-repo contract.
- **Correction to CLAUDE.md:** there is **no "iOS 16" UniXNU profile**. iOS 16 exists only in the external `ios-sweet` line.
- **Reuse:** the whole framework + `ipados-17.7.11` profile is the base. ArmCpu, ArmDTPlatform, DarwinBoot, XnuPrelink, SoftwareAES, GopFramebuffer, RamBlock, VerifiedRoot components.

### guowenye/UniXNU-sm8150-common @ `f8bc872ce6` — **PRIMARY SoC donor**
- **What:** shared SM8150 platform in UniXNU canonical SoC layout (`soc.toml`, `configs/sm8150.config`, `dts/sm8150.dtsi`, `kits/Sm8150Platform/`). HW data from Xiaomi `raphael-p-oss@663db50e`. BSD-2-Clause-Patent.
- **Verified (source):** kits read **all** MMIO bases/timer INTIDs/extents from the **ADT** — grep found **no hard-coded board MMIO**. `RaphaelBootCpu`/`RaphaelGicLocal` (GICv3 DS=0 boot CPU + arch timer), `RaphaelUfsTransport`/`RaphaelUfsBlock` (takes over firmware-initialised UFSHCI, read-only extents, checks SMMU not translating + other CPUs off via PSCI AFFINITY_INFO), `RaphaelDiagnosticWatchdog`, `RaphaelPmgrCompat`.
- **Limits:** **thin** — its `patches/xnu/series` is **empty**; the real kernel patches live in raphael. Class/compatible names are raphael-specific (`unixnu,raphael-ufs-ro`) → must rename for epsilon. "These results do not qualify another SM8150 device" (its own README).
- **Reuse decision:** **adopt as the SoC layer**, paired with raphael. Re-derive every address from epsilon's ADT.

### guowenye/UniXNU-raphael @ `1c435f2291` — XNU patches + recipes (paired with the above)
- **What:** Redmi K20 Pro device repo, format 2. Packaging pinned to UniXNU `c96b2997` (359 behind 21d683d).
- **Verified:** holds `patches/xnu/raphael-{bootcpu,cpu-ownership,ufs-controller,pmgr-compat}.patch`; build scripts; `porting.toml`.
- **CLAIMED:** iPadOS 17.7.11 Hello UI → lock screen (2026-11-07), single CPU, touch ineffective. CI checks **docs only** (no public boot logs).
- **Key asset — the non-destructive storage model:** OS/SystemOS as **immutable, unencrypted files inside Android `/data`**, read via extents, **no GPT change**, Data volume in RAM. This is the *only* donor storage model compatible with preserving Android+Windows. Open Q: works under Duo's **FBE-encrypted** `/data`? (`raphael:development/docs/HANDSET-STORAGE.zh-CN.md`)
- **Reuse decision:** **adopt** the XNU patch series + storage model as the templates; rename to epsilon.

### wochgn/UniXNU-guacamole @ `c8a9d18135` — **firmware-integration pattern + boot reference**
- **What:** OnePlus 7 Pro (SM8150), format 1. UniXNU `b6ae99c` (841 behind). Kernel from external `wochgn/ios-guacamole` (`ios-sweet`, xnu-8796, 16 KiB pages, HFS). **No LICENSE file.**
- **VERIFIED-in-CI:** `ios-guacamole` kernel *build* (run 39613287817).
- **CLAIMED:** r15 reaches iOS 16.7.16 userland on panel via `fastboot boot`; `ios-guacamole` shim boot shows home screen, 8/8 CPUs, touch (one in-repo photo). GPU blocked.
- **Reuse decision:** **study, don't copy the kernel** (different lineage, 16 KiB, flashes `userdata` → violates preservation). **Do** reuse `patches/aloha-guacamole.patch` as the **pattern** for adding `UniXNU`/`UniXNUDxe`/`DarwinBoot` INFs + a payload-RAM-disk DXE to a `SurfaceDuo1Pkg` DSC, and the payload lessons (FAT32 ≥64 MiB / ≥65525 clusters; `LogicalPartition=TRUE`; `fastboot boot` only). Note its UEFI base is **Project-Aloha `mu_aloha_platforms`**, a generic SM8150 `SurfaceDuo1Pkg` covering ~26 boards but **not the actual Surface Duo 1** (see §2).

### Lux12138/unixnu-nabu @ `39cff5f6b2` — secondary reference
- Xiaomi Pad 5 (SM8150-AC/SD860, 12 GiB). Built on a diverged `Lux12138/UniXNU` fork. **CLAIMED** launchd/SpringBoard/backboardd up, IOMobileFramebuffer provider missing. **CI kernel builds FAIL** (last 8). Useful only for: the `NABU_VTIMER_INTID 19` patch (confirms SM8150 virtual-timer INTID) and ArmCpu-timer parameterisation idea. Not a primary donor.

### sxl47/UniXNU-cepheus @ `2eae25aaf6` — early-console reference
- Xiaomi Mi 9 (SM8150, 6 GB — same RAM as Duo). **VERIFIED-in-CI** kernel build (xnu-8796 line). **CLAIMED** stuck in early `arm_init` (white block). Lesson worth keeping: measured GOP framebuffer at `0x90000000` ≠ DT's `0x9c000000`, and early markers lost to write-back cache → **treat the FB region as possibly write-back**. Not a donor otherwise.

### none24smu/UniXNU-zeta @ `eb489285a1` — **method donor** (Surface Duo 2, SM8350 — different SoC)
- **What:** most advanced public UniXNU port on a non-PAC Qualcomm phone. Format 1, no `porting.toml`. Kernel patches live in fork `none24smu/UniXNU@port/ios27-compat`.
- **CLAIMED:** iOS 26.5.2 userland (launchd/SpringBoard/backboardd, slow home screen); `ios-27` branch claims 27.1 lock/home screen. README is **internally inconsistent** (status table says Stage 6 incomplete while later sections claim 1200 s stability). Tooling **VERIFIED in source**; outcomes not.
- **Reuse — METHOD, not hardware values:** address-free `ZetaBootMemoryDxe` (publishes boot-image ramdisk as payload volume); `gen-*-memory-map.py`/`gen-*-config-map.py` + byte-equality `check-*.py`; `make-*-dtb.py`; ADT generator structure + its **required `/chosen` bootloader-contract properties** (random-seed 256 B or early_random panics; unique-chip-id or gethostuuid hangs; `/product` or AppleImage4 panics; NVRAM proxy; `kern.maxvnodes`); kernel-side `native-dwc3-console`; timed-reset probe methodology; the offline arm64e→arm64 pipeline (long-term only). Fork patches reusable for any Armv8.2-no-PAuth SM8150: `native-shared-region-{plain-auth,small-pages}`, `native-dyld-pager-plain-auth`, `native-csr-missing-asmb`, `native-smp-secondary` (Aff1 numbering matches SM8150), `architectural-timer-entry`.
- **⚠ Do NOT reuse:** every SM8350 hardware constant; and note **`ArmCpu` hardcodes virtual-timer INTID 27** (SM8350) — breaks on SM8150 (needs 19). Firmware USB CDC log driver (`ZetaUsbSerialDxe`) is **unpublished** (absent from all public uefi-zeta branches).
- **iOS 27 reference (primary-target track):** the `none24smu/UniXNU-zeta@ios-27` branch (CLAIMED iOS 27.1 lock/home screen) + fork `@port/ios27-compat` are the **compatibility reference for epsilon's iOS 27 goal** — its arm64e.x1 feature handling (PAuth_LR/CPA/CSSC/RCPC2/JSCVT trap-emulation) and its cache-rewrite scale (4,702 images, ~49M pointer edits) must be **independently re-validated for SM8150** (Kryo 485/Adreno 640 ≠ Kryo 680/Adreno 660). Do not adopt its unverified "iPhoneDuo iPhone19,4/V68AP" identity or IPSW. See `decisions/0005`.

### none24smu/uefi-zeta @ `765fbe8d71` (branch `zeta`) — SM8350 UEFI, structure reference only.

### Research task — Check none24smu's Duo 2 work

**Status:** Baseline checked 2026-11-17. Continue in parallel with T5 and M2/M3. This is a host-side source review and does not gate the epsilon diagnostic.

| Source | Revision checked |
|---|---|
| [UniXNU-zeta `ios-27`](https://github.com/none24smu/UniXNU-zeta/commit/9aa4db9b887ac9dbe3db322c167d765fde77c1d0) | `9aa4db9b887ac9dbe3db322c167d765fde77c1d0` |
| [UniXNU-zeta `main`](https://github.com/none24smu/UniXNU-zeta/commit/eb489285a1d9153872f00c93ebb519a269b249b5) | `eb489285a1d9153872f00c93ebb519a269b249b5` |
| [UniXNU kernel `port/ios27-compat`](https://github.com/none24smu/UniXNU/commit/2f8f7a64f2da4fccf592a67f7f89bde47dce4153) | `2f8f7a64f2da4fccf592a67f7f89bde47dce4153` |
| [uefi-zeta `zeta`](https://github.com/none24smu/uefi-zeta/commit/765fbe8d71798f4f4a1721ef2e7e5985aca6e520) | `765fbe8d71798f4f4a1721ef2e7e5985aca6e520` |

The iOS 27 donor branch still matches the previous pinned review. Its README pins the consolidated kernel to [`7f981f88ca`](https://github.com/none24smu/UniXNU/commit/7f981f88ca0388079af5f0ff01d9eb544b96a5cb), one commit ahead of the kernel branch tip listed above. Use the documented build pin when tracing the reported result. The author reports reaching Lock Screen and Home on Duo 2 with touch navigation and display performance still unresolved.

**Work:** Review the pinned donor boot and build notes, then compare upstream changes from this baseline as new commits appear. Start with the RAM payload and loader contract, early boot logging, and memory-map validation for M2/M3. For the iOS 27 feasibility track, record which observations concern the donor's 27.1 device identity and which also apply to epsilon's 24A446 candidate. Track display and input changes as later references.

**Deliverable:** For each relevant finding, record the source commit or CI run, the evidence level (source, build, or donor-reported hardware), the implication for epsilon, and the smallest host-side check that could validate it. Keep SM8350-specific assumptions out of the SM8150 configuration until independently verified. Review only. Preserve the current device and firmware baseline.

## 2. The firmware question (decision needed — see decisions/0003)

Two candidate UEFI bases:
- **(A) `WOA-Project/mu_andromeda_platforms` `SurfaceDuo1Pkg@613e2eac`** — the **authoritative Surface Duo 1** firmware: real patched `DisplayDxe` (left panel), N-Trig touch, stock `UFSDxe`, proven dual-boot with Windows. **But it is NOT Mu-Silicium**, and its `BootConfigShim` overwrites x0, so UniXNU's FDT-based payload discovery is lost (see compatibility.md B-FW1).
- **(B) Project-Aloha `mu_aloha_platforms` `SurfaceDuo1Pkg`** (what guacamole uses) — a Mu-Silicium-lineage generic SM8150 package that follows UniXNU's contract better, but covers ~26 *other* SM8150 boards and **does not contain the real Surface Duo 1** (no epsilon display/touch binaries).

**Recommendation:** build on **(A)** and solve the payload-discovery gap with a small PrePi/shim fix (or an FV-embedded payload that avoids initrd entirely), reusing **(B)/guacamole only as the integration pattern**. Rationale: (A) is the only base that actually drives the Duo's two panels, touch and UFS. See `decisions/0003`.

## 3. Reuse decision matrix

| Repo | SoC match | Evidence | Reuse |
|---|---|---|---|
| UniXNU/UniXNU | framework | mature | **Base framework + ipados-17.7.11 profile** |
| sm8150-common + raphael | ✅ SM8150 | source-solid, boot CLAIMED | **Primary SoC layer + XNU patches + storage model** |
| guacamole / ios-guacamole | ✅ SM8150 | build CI-verified, boot CLAIMED | **Pattern + payload lessons; study kernel; don't adopt** |
| nabu | ✅ SM8150-AC | CI fails | Reference (timer INTID) |
| cepheus | ✅ SM8150 | build CI-verified | Reference (early console / WB framebuffer) |
| zeta + fork | ❌ SM8350 | tooling source-verified, boot CLAIMED | **Method/scripts + non-PAC kernel patches; no HW constants** |
| mu_andromeda SurfaceDuo1Pkg | ✅ epsilon | source-solid | **Firmware base (A)** |
