# KamikazeOS — Phase 0: Vision Analysis & Architecture Decision

*Status: decided. This document records the reasoning so future contributors
(and future you) understand why the project is shaped this way.*

---

## 1. What kind of OS should this be?

There are four candidate paths. Honest trade-offs:

### Option A - Focused research OS prototype from scratch (bootloader + kernel in C/asm)
- **Pros:** You actually learn operating systems — memory, interrupts, drivers,
  the things the entire KamikazeOS vision depends on. Maximum portfolio
  credibility ("I wrote a kernel" is rare). Total control over the log format,
  panic handler, and driver model, which we need for GhostAttach.
- **Cons:** Slow. Years away from running real applications. No real hardware
  driver ecosystem. The AI layer can't run *inside* it for a long time.

### Option B — Linux-based custom OS (custom distro / kernel modules)
- **Pros:** Real hardware support day one. The self-healing ideas (systemd
  watchdogs, A/B partitions, signed packages, udev for device attach) already
  have production-grade building blocks. This is how you'd ship a real product.
- **Cons:** You learn *Linux plumbing*, not OS internals. "I configured a
  distro" is a weaker learning story. The interesting kernel-level work is
  mostly done for you.

### Option C — Browser-based WebOS
- **Pros:** Fast, pretty demos, easy AI integration.
- **Cons:** It is not an operating system. No drivers, no kernel, no boot, no
  hardware. It cannot honestly carry the KamikazeOS identity. Rejected as the
  core, but its *ideas* survive as the Phase 6 dashboard.

### Option D — Hybrid (chosen) ✅
**A real research kernel from scratch (Option A) + the AI Recovery Core running
outside the kernel on the host (initially), + an explicit future pivot point
where the proven concepts can be re-implemented on Linux (Option B) if this
ever needs to become a shippable product.**

Why this wins for a beginner builder who wants both learning and a portfolio:

1. **The kernel work is the learning.** Phases 1–3 teach boot, VGA, interrupts,
   device detection, and driver policy — exactly the skills the vision needs.
2. **The AI work is the product story.** Phases 4–5 build the genuinely novel
   part (structured AI-readable diagnostics, risk-ranked repair, policy engine,
   rollback) as a host-side tool. That architecture — *AI advises through
   controlled tools, never rewrites the kernel live* — is also the correct
   safety architecture, not just a beginner shortcut.
3. **Nothing is wasted on a pivot.** The log schema, policy engine, risk
   taxonomy, and AI analyzer are kernel-agnostic. If KamikazeOS later rebases
   onto Linux for real hardware, those components move over unchanged.

---

## 2. The three-layer architecture

```
┌─────────────────────────────────────────────────────────────┐
│  AI RECOVERY CORE          (userspace / host-side at first)  │
│  log analysis · explanations · risk ranking · policy engine  │
│  rollback points · upgrade recommendations                   │
│        │  acts ONLY through controlled, audited tools        │
├────────▼────────────────────────────────────────────────────┤
│  GHOSTATTACH DRIVER LAYER                                    │
│  device identification · verified driver registry            │
│  exact match → generic fallback → isolate                    │
│  boot-loop prevention · structured failure logs              │
├─────────────────────────────────────────────────────────────┤
│  KERNEL LAYER (small, boring, safe)                          │
│  boot · memory · interrupts · processes · syscalls           │
│  panic handler · safe mode · recovery mode                   │
└─────────────────────────────────────────────────────────────┘
```

**Core rule: intelligence flows down only through narrow, verified channels.**
The kernel never trusts the AI. The AI requests actions; the kernel/policy
layer decides whether they're allowed.

### Kernel Layer
Deliberately small and conventional. Innovation budget is spent above it.
Every subsystem logs in the structured format from day one (Phase 2), because
AI-readable diagnostics are the foundation everything else stands on.

### GhostAttach
A deterministic (non-AI) driver policy engine:
1. Enumerate hardware → extract (bus, class, vendor ID, device ID, signature).
2. Look up the **local verified registry** (signed manifest of known drivers).
3. Exact match → attach. No exact match → attach **safe generic fallback** for
   that device class (e.g. VESA framebuffer, generic HID) if one exists.
4. Driver fails to attach → **isolate the device**, log a structured failure,
   and continue booting. A driver that crashed last boot is quarantined this
   boot (boot-loop prevention via an attach-attempt counter persisted before
   the attach is tried).
5. **Never** auto-load an unverified kernel driver. Unknown devices are
   reported, not guessed at.

GhostAttach itself contains no AI. It is a policy machine the AI can *consult
and advise* ("this unknown device is probably X, here is a verified driver you
could approve"), never bypass.

### AI Recovery Core
Runs **outside the kernel** — first on the host machine reading exported logs
(Phase 4), later as a sandboxed userspace service (much later). It:
- Parses structured logs, panics, driver failures, performance counters.
- Explains issues in human language.
- Proposes fixes **ranked by risk**, each mapped to a pre-built, signed,
  reversible repair action — never arbitrary code.
- Submits proposals to the **policy engine**, which is dumb on purpose:

| Risk class | Examples | Policy |
|---|---|---|
| **Low** | restart a service, clear a cache, re-attach a fallback driver, rotate a log | Auto-apply *if admin policy allows*, always logged, always reversible |
| **Medium** | swap driver version, change boot parameter, apply verified patch | Requires explicit user/admin approval |
| **High** | kernel module change, partition/boot record change, security policy change | Approval + rollback point + post-change verification, or blocked outright |
| **Unsafe** | unsigned code, unverified source, policy bypass | **Always blocked.** Not approvable in software. |

- Creates a rollback point before *any* change; a failed repair triggers
  automatic rollback and/or safe-mode boot.
- "Learns from repeated failures" means: keeps a per-machine incident history
  and stops re-proposing fixes that failed before — **not** online model
  training on the user's machine.
- "Monitors new standards/updates" means: checks **signed release manifests
  from configured trusted sources** and recommends upgrades. It never fetches
  and runs random code from the internet.

---

## 3. Honest capability ledger

### Possible NOW (Phases 1–4)
- Real bootable research kernel in QEMU with shell, structured logging, panic handler.
- Deterministic device detection + registry matching + fallback policy (GhostAttach v0.1).
- Exporting logs and having an LLM (e.g. Claude) genuinely explain failures and
  propose fixes for a human to approve. This works today and is the demo-able core.

### Possible LATER (Phases 5–6+, with serious work)
- Signed update packages, version tracking, A/B-style rollback inside the research OS prototype.
- Policy engine auto-applying low-risk repairs in the emulator.
- The same AI recovery architecture rebased onto Linux for real hardware.
- An in-OS (userspace, sandboxed) recovery agent instead of a host-side one.

### UNREALISTIC or UNSAFE (will not be built, say so when asked)
- AI writing or hot-patching kernel code on a live system. ❌
- AI auto-downloading drivers from the open internet. ❌
- "Fixing anything" — many failures (dead hardware, corrupted firmware) are not
  software-fixable; the honest behavior is *diagnose, explain, isolate*. ❌
- Supporting real consumer hardware with a from-scratch kernel in any
  reasonable timeframe. That's what the Linux pivot is for. ❌
- An LLM running inside the kernel. ❌ (Wrong place even if it were possible.)

---

## 4. Non-negotiable safety invariants

1. Kernel stays small; AI stays out of the dangerous path.
2. AI advises and acts only through pre-built, signed, audited repair tools.
3. Every fix is logged. Every risky change is reversible. Rollback before change.
4. Driver loading requires verification — no exceptions, including for the AI.
5. Unknown errors are never swallowed: log → classify → surface.
6. A failed repair triggers rollback and/or safe mode, never a retry loop.
7. The policy engine is deterministic and simple enough to audit by hand.

---

## 5. Toolchain decision (beginner-friendly, Windows-native)

- **Target:** 32-bit x86 (i686), Multiboot v1. QEMU loads Multiboot ELF
  kernels directly with `-kernel` — no GRUB, no ISO building, no WSL required.
- **Compiler:** `clang` (LLVM) — it is a cross-compiler out of the box
  (`--target=i686-pc-none-elf`), unlike GCC which would need a custom
  cross-build. One `winget install LLVM.LLVM` and you're done.
- **Linker:** `ld.lld` (ships with LLVM), driven by a small linker script.
- **Assembly:** GAS-style `.S` files assembled by clang itself, using
  `.intel_syntax noprefix` so the assembly reads like the friendlier Intel
  syntax. No separate assembler needed.
- **Emulator:** `qemu-system-i386`.
- **Build:** plain PowerShell scripts (`scripts/build.ps1`) — no make needed yet.

Risks & limitations to keep in mind: 32-bit x86 is a teaching target, not a
modern one (x86_64/UEFI comes later or never); QEMU is forgiving in ways real
hardware is not; and everything before Phase 5 has no persistence (the "disk"
is RAM, logs vanish at poweroff — Phase 2/5 address export and storage).
