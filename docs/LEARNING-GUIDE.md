# KamikazeOS — Complete Learning Guide

*Everything this project can teach you, organized as a path from "I've never
written a kernel" to "I built a self-healing OS with a durable update pipeline."*

You don't need to read this top to bottom. It's a map. Each section says **what
the concept is**, **where it lives in our code**, **why it matters**, and **how
to go deeper**. Follow the [Suggested study order](#15-suggested-study-order) if
you want a route.

---

## 0. How to use this guide

- Every `file.c` reference is a real file in this repo — open it and read the
  comments. The code was written to be *read*, with heavy comments explaining
  not just what but **why**.
- "🔑 Key idea" boxes are the things worth truly understanding.
- "🧪 Try it" boxes are hands-on exercises.
- "📚 Go deeper" points you to the canonical external resources.
- The single best companion to this guide is [docs/00-architecture.md](docs/00-architecture.md)
  (the design reasoning) and [docs/01-roadmap.md](docs/01-roadmap.md) (the phase
  history). This guide is the *concepts*; those are the *decisions*.

---

## 1. The big picture — what KamikazeOS is

KamikazeOS is an experimental **AI-native, self-healing operating system**. It
boots on bare (virtual) hardware, and instead of just printing errors when
something breaks, it has a layered recovery system that can detect problems,
explain them, suggest fixes, apply *only safe and reversible* repairs with human
approval, remember what hurt it across reboots, and update its own components
through a controlled pipeline.

The whole thing is built around one safety principle you should internalize
before anything else:

> 🔑 **Key idea — the AI is the maintenance brain, never the hand on the wheel.**
> The AI/analyzer can read logs, explain, rank risk, and *recommend* commands.
> It can never apply a fix itself. Every change goes through a small,
> deterministic, auditable policy gate, and a human approves it. This isn't a
> limitation we'll remove later — it's the architecture.

### The layers (learn these names — the whole project is organized by them)

| Layer | What it does | Where |
|---|---|---|
| **Kernel** | boot, screen, keyboard, memory, the shell | [kernel/src/](kernel/src) |
| **GhostAttach** | device + driver registry, attach policy, fallbacks, isolation | [ghostattach.c](kernel/src/ghostattach.c) |
| **RecoveryEngine** | fixed catalog of human-approved repair actions | [recovery_actions.c](kernel/src/recovery_actions.c) |
| **RecoveryMemory** | short-term recovery state in CMOS NVRAM | [recovery_memory.c](kernel/src/recovery_memory.c) |
| **RecoveryStore** | durable long-term state + journal on a disk image | [recovery_store.c](kernel/src/recovery_store.c) |
| **BootSnapshot** | per-boot health scores, compared across time | [boot_snapshot.c](kernel/src/boot_snapshot.c) |
| **RecoveryPackage** | controlled update pipeline (manifests, A/B slots) | [recovery_package.c](kernel/src/recovery_package.c) |
| **AI Recovery Core** | host-side log analyzer (Python, outside the kernel) | [ai-recovery/analyze_logs.py](ai-recovery/analyze_logs.py) |
| **Recovery Center** | read-only web dashboard | [recovery-center/](recovery-center/index.html) |

---

## 2. Foundations — what an OS actually is

Before the code, the mental model.

- **What runs first.** When a machine powers on, firmware (BIOS/UEFI) hands
  control to a *bootloader*, which loads a *kernel* into memory and jumps to it.
  We skip writing a disk bootloader by using the **Multiboot** standard: we put
  a magic header in our kernel and QEMU's `-kernel` flag loads us directly. See
  [kernel/boot/boot.S](kernel/boot/boot.S).
- **Real OS vs. "web OS" vs. Linux-based.** We chose a **focused research kernel from
  scratch** (you learn OS internals) plus a **host-side AI layer** (the product
  story). The reasoning — and why a browser "OS" isn't an OS — is in
  [docs/00-architecture.md §1](docs/00-architecture.md).
- **Kernel space vs. user space.** Our shell runs *inside* the kernel for now
  (there's no userspace yet). Real OSes isolate programs from the kernel. That
  separation needs processes + syscalls, which is future work.
- **Privilege & protection.** The CPU has rings (0 = kernel, 3 = apps). We run
  in ring 0 the whole time. Understanding why that's dangerous is half of OS
  security.

> 📚 **Go deeper:** [OSDev Wiki — Introduction](https://wiki.osdev.org/Introduction)
> and "Operating Systems: Three Easy Pieces" (free online book, OSTEP).

---

## 3. Bare-metal boot — getting your code to run

This is Phase 1. The most magical moment in the whole project: going from
nothing to *your own code running on a CPU*.

**Concepts:**
- **The Multiboot header** — a magic number + flags + checksum that a loader
  scans for. [boot.S](kernel/boot/boot.S) defines it.
- **Assembly entry point** — before C can run you need a *stack*. `boot.S` sets
  `esp`, then calls `kmain`. There is no C runtime; you provide everything.
- **The linker script** — [kernel/linker.ld](kernel/linker.ld) tells the linker
  where each section goes in memory (kernel at 1 MiB — below that is a minefield
  of legacy regions). This is where you learn what `.text`, `.rodata`, `.bss`
  actually are.
- **Protected mode** — the loader puts us in 32-bit protected mode already.

> 🔑 **Key idea — there is no magic floor.** "Freestanding" means nothing is
> under you. No `printf`, no `malloc`, not even `memcpy` unless you write it
> (the compiler still emits calls to `memcpy`/`memset` for struct copies — see
> [string.c](kernel/src/string.c), which had to define them by hand).

> 🧪 **Try it:** read `boot.S` line by line. Then in `kmain`, change the boot
> banner text in [kernel.c](kernel/src/kernel.c), rebuild, and watch it change.

> 📚 **Go deeper:** [OSDev — Bare Bones](https://wiki.osdev.org/Bare_Bones) is
> almost exactly our Phase 1.

---

## 4. Talking to hardware — port I/O and the screen

**Concepts:**
- **Memory-mapped I/O** — the VGA text screen is just memory at address
  `0xB8000`. Write two bytes (char + color) and a character appears. That's the
  entire "display driver" — [vga.c](kernel/src/vga.c). Scrolling, the cursor,
  colors all build on this.
- **Port I/O** — a *separate* address space reached only with the `in`/`out`
  CPU instructions, for older devices. We wrap them in inline assembly in
  [io.h](kernel/src/io.h) (`inb`, `outb`, and later `inw`/`outw` for the disk).
- **Inline assembly** — how C reaches instructions it has no syntax for. Every
  `__asm__ volatile (...)` in the project is worth understanding.

> 🔑 **Key idea — a "driver" is just code that knows a device's protocol.**
> VGA = write to magic memory. Keyboard = read a port. Serial = configure a
> chip then poll a "ready" bit. None of it is mysterious once you see it.

> 🧪 **Try it:** in `vga.c`, change the default color in `vga_init`. Then make
> the panic screen blue instead of red in [panic.c](kernel/src/panic.c).

> 📚 **Go deeper:** [OSDev — Printing to Screen](https://wiki.osdev.org/Printing_To_Screen),
> [OSDev — I/O Ports](https://wiki.osdev.org/Port_IO).

---

## 5. Input — the PS/2 keyboard

**Concepts:**
- **Polling vs. interrupts.** We *poll*: loop asking "is a key ready?" Simple
  but wastes CPU. Real systems use interrupts (IRQ1). [keyboard.c](kernel/src/keyboard.c)
  is deliberately polling-based to stay beginner-friendly.
- **Scancodes → ASCII.** The keyboard sends *scancodes* (key numbers), not
  letters. A lookup table maps them. Shift is tracked as state. Key *release*
  events have the high bit set.
- **Detecting absence.** The driver probes the controller; if nothing answers,
  it logs an error and the OS keeps booting (a server with no keyboard is still
  a server). This "detect, don't assume" habit is everywhere in the project.

> 🧪 **Try it:** add a new key mapping (e.g. make the tilde key print something
> custom) in the scancode tables.

> 📚 **Go deeper:** [OSDev — PS/2 Keyboard](https://wiki.osdev.org/PS/2_Keyboard).

---

## 6. The shell — a command loop

[shell.c](kernel/src/shell.c) is where it becomes interactive. Worth studying
as a clean example of:
- **A read-eval loop:** read a line (echoing keystrokes, handling backspace),
  match against a command table, dispatch.
- **Buffer-overflow safety:** the line buffer is fixed size and `read_line`
  refuses to store more than capacity. *This is a security lesson in 5 lines* —
  unbounded input is how buffer overflows happen.
- **Argument parsing without a standard library:** splitting `"approve ACT-001"`
  into command + argument by hand, no `strtok`, no allocation.
- **A function-pointer dispatch table** (`struct command { name; handler; }`).

> 🔑 **Key idea — unknown input is surfaced and logged, never silently ignored.**
> An unknown command prints a helpful message *and* writes a log entry. "Never
> swallow errors" is a recurring rule (see the safety principles in §11).

---

## 7. Structured logging & error handling — the bedrock of the whole AI idea

This is the single most important *design* decision in the project, introduced
in Phase 2. Understand it deeply.

**Concepts:**
- **Structured vs. unstructured logs.** Minimal kernels often `print("error!")`.
  We log *structured events*: every entry has a stable **code**, a **severity**,
  a **component**, a **message**, and a **recovery hint**. See
  [errors.c](kernel/src/errors.c) / [errors.h](kernel/src/errors.h).
- **A stable error-code taxonomy.** `KMZ-KBD-0001`, `GA-DRV-101`,
  `PKG-BLOCK-001`… A stable code means a machine (or AI) can reason about an
  event without parsing free-text English. **This is what makes the OS
  "AI-readable."**
- **A ring buffer.** Fixed-size storage that overwrites the oldest entry when
  full — no dynamic memory needed. A fundamental embedded/kernel data structure.
- **The recovery-hint field.** Each component states its own best-known remedy
  at the moment of failure. That gives the later AI ground truth instead of
  guesswork.
- **A controlled crash (`kpanic`).** When the kernel can't safely continue, it
  logs a PANIC, paints a diagnostic screen, and halts — see
  [panic.c](kernel/src/panic.c). Compare to a Linux "kernel panic" or Windows
  BSOD; same idea.

> 🔑 **Key idea — diagnostics before cosmetics.** `kpanic` records the event
> *first*, then draws the screen. If the machine dies a microsecond later, the
> evidence already exists. This ordering discipline shows up again with NVRAM
> and disk writes.

> 🧪 **Try it:** run `panic-test` in the OS. Then read the panic record in
> `build/serial.log`. You just saw evidence survive a crash.

---

## 8. Serial output — getting data *out* of the machine

**Concepts:**
- **The COM1 UART** ([serial.c](kernel/src/serial.c)) — the simplest pipe from
  inside the VM to a file on the host. Configure the chip (baud rate, 8N1), then
  poll a "transmitter empty" bit and write bytes.
- **Why it matters:** every log entry is streamed to serial *the moment it
  happens*. Run QEMU with `-serial file:build/serial.log` and the host gets a
  live, permanent record that survives panics. **The screen is for humans; the
  serial port is for machines.** This file is the input to the entire AI layer.
- **A loopback self-test** — the driver tests itself at boot (send a byte in
  loopback mode, check it comes back) before trusting the hardware.

> 📚 **Go deeper:** [OSDev — Serial Ports](https://wiki.osdev.org/Serial_Ports).

---

## 9. Device & driver management — GhostAttach (a policy engine)

Phase 3. This is where you learn how an OS decides *what driver runs what
hardware* — and how to do it **safely**.

**Concepts:**
- **Modeling hardware in structs** — [device.h](kernel/src/device.h),
  [driver.h](kernel/src/driver.h): a device has a type, bus, vendor/device IDs,
  a required driver, a fallback driver, and a status. A driver has a version, a
  *verified* flag, and a *risk level*.
- **State machines.** Devices move through states (`ATTACHED`, `FALLBACK`,
  `MISSING_DRIVER`, `ISOLATED`…). Drivers too. Learning to think in explicit
  states is a core systems skill.
- **A deterministic attach policy** ([ghostattach.c](kernel/src/ghostattach.c)):
  1. exact verified driver → attach
  2. no exact? a **verified fallback** may stand in
  3. **unverified driver → blocked, always**
  4. no verified driver → device stays off (`missing_driver`)
  5. driver fails to attach → device **isolated** (quarantined) so a bad driver
     can't cause a boot loop
  6. every decision is logged with a `GA-*` code

> 🔑 **Key idea — there is no AI in this layer, on purpose.** GhostAttach is a
> dumb, predictable policy machine. The AI may later *advise* ("a verified
> driver exists upstream"), but it goes through this gate like everyone else and
> can never bypass it. Keeping intelligence *out* of the dangerous path is a
> deliberate safety architecture.

> 🧪 **Try it:** run `devices` and `drivers` in the OS. Notice `generic_storage`
> is `blocked_unverified` and `selftest_driver` is `failed` → its device
> `isolated`. The registry is built so *every* policy branch fires every boot.

---

## 10. Persistence — making the OS remember

This is a three-phase arc (6 → 7) and a genuinely deep topic.

### 10a. CMOS / RTC NVRAM (short-term memory) — [recovery_memory.c](kernel/src/recovery_memory.c)
- The real-time-clock chip has a small battery-backed memory reachable via
  ports `0x70`/`0x71`. We store a tiny fixed-layout state block there.
- **Magic + version + checksum.** You never trust persisted data blindly — if
  the magic bytes or checksum don't match, treat it as "no memory" and start
  fresh. This guards against corruption, a different kernel's data, or a blank
  machine.
- **Honest about limits:** CMOS survives in-VM reboots but QEMU recreates it on
  a full process restart. The UI says so. *Labeling what's real vs. simulated is
  a discipline, not an afterthought.*

### 10b. Disk storage via ATA PIO (long-term memory) — [ata.c](kernel/src/ata.c)
- **The first real storage driver.** Talk to an IDE disk through ports
  `0x1F0–0x1F7` in **PIO mode** (the CPU moves every byte itself, polling status
  bits). Read/write one 512-byte sector at a time.
- **Bounded waits.** Every wait loop has a budget so a missing/stuck disk can
  *never* hang the boot. If no disk answers, fall back to CMOS and keep going.
- **Durability = flush.** After writing, issue a cache-flush command and wait,
  or your data may sit in a volatile cache. (This caused a real bug — see §13.)

### 10c. A fixed-layout "filesystem" — [recovery_store.c](kernel/src/recovery_store.c)
- We don't build a real filesystem. We assign **fixed sectors** to fixed
  purposes:

  | LBA | Contents |
  |---|---|
  | 0 | untouched (avoid boot-sector confusion) |
  | 1 | recovery state block |
  | 2–15 | append-only recovery journal |
  | 16–31 | boot snapshots |
  | 32–39 | package manifests |
  | 40–47 | reserved |
  | 48–49 | persistent package state, primary and mirror |
  | 50 | RecoveryStore state mirror when present within capacity |
  | 51–55 | reserved |
  | 56–63 | A/B module slot metadata |

- **An append-only journal.** Recovery history is *evidence*; evidence you can
  silently rewrite is worthless. When full, appending *stops* rather than
  overwriting. Every entry is checksummed.

> 🔑 **Key idea — layered persistence with graceful degradation.** Disk first
> (survives everything), CMOS fallback (survives reboots), fresh (nothing). Each
> save writes both tiers so losing one degrades cleanly. The UI always reports
> which backend is active: `disk+CMOS` / `CMOS_only` / `fresh`.

> 📚 **Go deeper:** [OSDev — ATA PIO Mode](https://wiki.osdev.org/ATA_PIO_Mode),
> [OSDev — CMOS](https://wiki.osdev.org/CMOS).

---

## 11. Safety engineering — the policy gates

This is the intellectual heart of the project and the most transferable skill.
Across the whole OS, **every risky action passes a small, deterministic,
human-auditable gate**. Study these two files — each is ~20 lines on purpose:

- [recovery_policy.c](kernel/src/recovery_policy.c) — for recovery actions.
- [package_policy.c](kernel/src/package_policy.c) — for update packages.

**The rules they encode (memorize the shape):**
- high-risk → **blocked**
- irreversible → **blocked**
- unsigned / untrusted → **blocked**
- checksum mismatch → **blocked**
- everything else → **requires human approval**

> 🔑 **Key idea — make the security-critical code small enough to audit by
> hand, and give it no state.** A policy function with no memory, no learning,
> and no exceptions is one you can *prove* correct by reading it. Compare to
> sprawling permission systems you can't reason about.

**The "canary" pattern.** There's always a deliberately forbidden item in each
registry — `ACT-999` (install unknown driver), `PKG-003` (unsigned high-risk
driver). They exist so the *block path runs and is visible every boot*. If a
canary ever applies, you know the gate is broken. This is a beautiful testing
idea: **build the failure case into the system so it's continuously verified.**

**Defense in depth.** The gate is re-checked at approval *and* at apply, even
though init already marked blocked items. One extra comparison buys you safety
against a future code change that forgets.

> 🧪 **Try it:** run `approve ACT-999` and `approve-pkg PKG-003`. Watch them get
> refused, see the `REC-BLOCK-999` / `PKG-BLOCK-001` journal entries, and
> confirm no state changed.

---

## 12. The recovery loop — detect, explain, approve, apply, verify, roll back

This is the product vision made real, built across Phases 5–10.

- **Fixed action catalog** ([recovery_actions.c](kernel/src/recovery_actions.c)):
  the *complete* list of things recovery can ever do. Suggesting something not in
  it is impossible by construction.
- **The human-approved lifecycle:** `suggestions` → `approve <id>` → applied +
  logged → `rollback <id>`. Reversibility is enforced; rollback restores the
  *recorded prior state*, not an assumed one.
- **Health scoring** ([boot_snapshot.c](kernel/src/boot_snapshot.c)): a
  deterministic 0–100 score (start at 100, subtract for panics, missing drivers,
  isolated devices, warnings, criticals). Simple enough to recompute by hand —
  *a score you can't verify is a score you can't trust.* Snapshots let the
  analyzer answer "is this boot healthier than the last?"
- **Controlled updates** ([recovery_package.c](kernel/src/recovery_package.c)):
  the full pipeline — **manifest** (disk, trust-verified) → `approve-pkg` →
  `apply-pkg` (switches an **A/B module slot**, old slot kept as the way back) →
  `verify-pkg` (version + health checks) → `rollback-pkg`. Designed exactly like
  a real A/B updater (think Android/ChromeOS), at the metadata level.
- **A trust anchor** ([package_manifest.c](kernel/src/package_manifest.c)): the
  root of update trust. Honestly labeled — signature verification is *simulated*
  (the enforcement path is real; the Ed25519 crypto is future work).

> 🔑 **Key idea — A/B slots are how real systems update without bricking.** You
> never overwrite the running thing. You install into the inactive slot, switch
> the pointer, verify, and keep the old slot as a guaranteed rollback. If verify
> fails (or the machine dies mid-update), the known-good slot is right there.

> 🧪 **Try it (the capstone demo):** `approve-pkg PKG-001` → `apply-pkg PKG-001`
> → `slots` (see the active slot flipped) → restart QEMU fully → `packages`
> (still applied, pending verify — *it survived a full restart*) →
> `verify-pkg PKG-001`.

---

## 13. Debugging real systems — the bugs we actually hit

These are gold. Every one taught a lesson you only learn by hitting it.

1. **SSE instructions triple-faulted the boot.** clang's i686 target emits
   vectorized (SSE) instructions at `-O2`, but the kernel never enabled the SSE
   CPU feature → illegal opcode → triple fault → silent reboot. **Fix:**
   `-mno-sse -msoft-float` in the build. *Lesson: the compiler assumes a richer
   CPU than a bare kernel has set up.*

2. **QEMU's pipe monitor froze the whole VM.** A Windows named-pipe monitor
   blocks the emulator until something drains it. Looked like "kernel won't
   boot" (CPU stuck at the reset vector). *Lesson: distinguish "my code is
   broken" from "my test harness is broken."*

3. **Missing `memcpy`/`memset` at link time.** The compiler emits calls to them
   for struct assignment even with no standard library. **Fix:** implement them
   yourself ([string.c](kernel/src/string.c)). *Lesson: freestanding means
   freestanding.*

4. **ATA cache-flush timed out and dropped ~1 write in 10.** QEMU implements the
   flush command as a real host `fsync`, which on a loaded Windows host can take
   longer than a fast poll loop's budget. Journal entries silently vanished
   (this had been corrupting v0.7/v0.8 journals too!). **Fix:** a *patient*
   flush wait + one retry, and *loud logging* on write failure instead of silent
   loss. *Lesson: silent failure is the worst failure; and "durable" has subtle
   timing requirements.*

5. **A string literal overflowed a fixed struct field** (caught by a compiler
   warning, `-Wunterminated-string-initialization`). *Lesson: fixed-size buffers
   need fixed-size content; heed warnings.*

> 🔑 **Key idea — verification is part of building, not after it.** Almost every
> phase shipped *because* it was booted in QEMU, driven with injected
> keystrokes, and screenshotted. Several of these bugs were invisible until run.
> "It compiles" is not "it works."

---

## 14. The host side — Python analyzer & the dashboard

The AI layer lives **outside the kernel**, which is both the safe design and an
easier place to learn from.

- **The analyzer** ([ai-recovery/analyze_logs.py](ai-recovery/analyze_logs.py)):
  parses the structured serial log, classifies issues (info/warning/critical),
  maps stable codes to causes + safe next actions via a rule table, detects
  cross-boot patterns (repeated panics, health trends, "safe mode improved
  health by +N"), and **only ever recommends IDs that exist in the OS's own
  registries** — it cannot invent an action or apply anything. The
  `analyze_with_ai_later()` stub documents exactly where a real LLM would plug
  in (structured logs only, a fixed fix-catalog, advice not action).
- **The format contract.** Kernel and analyzer agree on one line format:
  `code=X;severity=Y;component=Z;message=M;hint=H`. Values never contain `;` or
  `=`. *Lesson: a simple, strict, documented data contract between components is
  worth more than a clever one.*
- **The dashboard** ([recovery-center/](recovery-center/index.html)): static
  HTML/CSS/JS, no build system, no server required. It reads a JSON file
  produced by [export_dashboard_json.py](ai-recovery/export_dashboard_json.py).
  Its buttons **only copy shell commands** — there is literally no code path
  that applies anything. The read-only safety notice comes from the data file so
  UI and contract can't drift apart.

> 🔑 **Key idea — the same safety rule binds the AI and the UI.** Neither gets
> special powers. They explain and recommend; the human types the command inside
> the OS. "Read-only by construction" beats "read-only by promise."

---

## 15. Suggested study order

If you want a route through it all:

1. **Run it first.** Build and boot (`scripts/build.ps1` + `scripts/run.ps1`, or
   `make && make run`). Type `help`, then every command. Get a feel for the
   product before the code.
2. **Boot path:** [boot.S](kernel/boot/boot.S) → [linker.ld](kernel/linker.ld) →
   `kmain` in [kernel.c](kernel/src/kernel.c).
3. **Talking to hardware:** [io.h](kernel/src/io.h) → [vga.c](kernel/src/vga.c)
   → [keyboard.c](kernel/src/keyboard.c) → [serial.c](kernel/src/serial.c).
4. **The shell:** [shell.c](kernel/src/shell.c) (read `shell_run` and the
   command table).
5. **The AI-native core:** [errors.c](kernel/src/errors.c) +
   [panic.c](kernel/src/panic.c). Understand the structured log deeply.
6. **Policy & devices:** [device.h](kernel/src/device.h) /
   [driver.h](kernel/src/driver.h) → [ghostattach.c](kernel/src/ghostattach.c).
7. **Safety gates:** [recovery_policy.c](kernel/src/recovery_policy.c) →
   [recovery_actions.c](kernel/src/recovery_actions.c). This is the conceptual
   peak — take your time.
8. **Persistence:** [recovery_memory.c](kernel/src/recovery_memory.c) →
   [ata.c](kernel/src/ata.c) → [recovery_store.c](kernel/src/recovery_store.c).
9. **Health & time:** [boot_snapshot.c](kernel/src/boot_snapshot.c).
10. **Updates:** [package_manifest.c](kernel/src/package_manifest.c) →
    [module_slots.c](kernel/src/module_slots.c) →
    [recovery_package.c](kernel/src/recovery_package.c).
11. **Host side:** [ai-recovery/analyze_logs.py](ai-recovery/analyze_logs.py) →
    the dashboard.
12. **Read the decisions:** [docs/00-architecture.md](docs/00-architecture.md)
    and [docs/01-roadmap.md](docs/01-roadmap.md) to see *why* it's shaped this
    way.

---

## 16. The toolchain & build (practical skills)

- **Cross-compiling** with `clang --target=i686-pc-none-elf` — clang is a
  cross-compiler out of the box (no custom GCC build). Flags that matter:
  `-ffreestanding`, `-nostdlib`, `-fno-stack-protector`, `-mno-sse`,
  `-fno-builtin`. Read [scripts/build.ps1](scripts/build.ps1) and the
  [Makefile](Makefile) — both build the same kernel.
- **Linking** with `ld.lld` driven by a custom linker script.
- **Emulation** with `qemu-system-i386`. The `-kernel` flag (Multiboot),
  `-serial file:` (capture logs), `-drive ...,if=ide` (the recovery disk).
- **Make targets worth knowing:** `make`, `make run`, `make clean` (keeps the
  recovery image!), `make reset-store`, `make analyze`, `make dashboard-json`.

> 🧪 **Try it:** read the `CFLAGS` line in the Makefile and look up what each
> flag does. You'll understand more about C compilation than most working
> programmers.

---

## 17. Concepts checklist (tick these off)

By the end you should be able to explain, in your own words:

- [ ] What a bootloader does and what Multiboot is
- [ ] Why a kernel needs a stack before C runs
- [ ] What a linker script controls
- [ ] The difference between memory-mapped I/O and port I/O
- [ ] How VGA text mode, a PS/2 keyboard, and a UART each work
- [ ] Polling vs. interrupts (and why we poll)
- [ ] What "freestanding" means and why you implement `memcpy` yourself
- [ ] Why structured logs with stable codes enable machine/AI reasoning
- [ ] A ring buffer, and why no dynamic allocation in a kernel
- [ ] How a deterministic attach policy keeps drivers safe
- [ ] Magic + version + checksum for trusting persisted data
- [ ] ATA PIO sector read/write and why flushing matters for durability
- [ ] A fixed-sector layout as a stand-in for a filesystem
- [ ] An append-only journal and why history must be immutable
- [ ] Risk/reversibility/approval policy gates
- [ ] The canary pattern for continuously testing the block path
- [ ] A/B slots and rollback-to-known-good updates
- [ ] Why the AI/dashboard are read-only by construction
- [ ] A strict text data contract between OS and host tools

---

## 18. Where to go next (beyond this project)

Real next steps, roughly in order of difficulty:

1. **Interrupts (IDT)** — replace polling with IRQ-driven keyboard, handle CPU
   exceptions properly. The biggest "real kernel" upgrade.
2. **Memory management** — a physical frame allocator, then paging. This is the
   deep end of OS internals.
3. **Userspace + syscalls** — run programs outside ring 0; the real
   kernel/user boundary.
4. **A real PCI scan** — replace GhostAttach's static registry with actual bus
   enumeration (ports `0xCF8`/`0xCFC`).
5. **Real cryptography** — Ed25519 signature verification for packages, turning
   the simulated trust anchor into a real one.
6. **A real LLM in the analyzer** — wire `analyze_with_ai_later()` to the Claude
   API, with the structured-logs-only / fixed-catalog guardrails intact.

> 📚 **Canonical resources:** the [OSDev Wiki](https://wiki.osdev.org) (your
> primary reference), **OSTEP** (free OS textbook), the **Intel SDM** Volume 3
> (the x86 system-programming bible), and Philipp Oppermann's
> ["Writing an OS in Rust"](https://os.phil-opp.com) blog (different language,
> excellent explanations of the same concepts).

---

*This guide covers KamikazeOS up to v1.0.0 "Ship of Theseus." The code is the
real teacher — this is just the map to it. Read the comments; they explain the
why, which is the part worth learning.*
