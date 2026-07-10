# Command Reference

KamikazeOS shell commands in v2.3.0.

## General

| Command | Purpose |
|---|---|
| `help` | Show commands |
| `clear` | Clear the screen |
| `about` | Version and mission summary |
| `system` | Integrated v2.3 system readiness summary |
| `diagnose` | Compact full-system diagnostic and next manual command |
| `reboot` | Restart the machine |
| `panic-test` | Deliberately crash for panic-handler testing |

## Kernel Interrupts

| Command | Purpose |
|---|---|
| `interrupts` | Show IDT, PIC, timer, keyboard mode, counters, and last vector |
| `ticks` | Show the live 100 Hz PIT tick counter |
| `exceptions-test divide confirm` | Trigger a controlled divide-by-zero exception and halt |
| `exceptions-test invalid-opcode confirm` | Trigger a controlled invalid-opcode exception and halt |

Both exception tests export structured panic evidence. They do not attempt
recovery and require the literal `confirm` argument.

## Kernel Memory

| Command | Purpose |
|---|---|
| `mem` | Show physical memory totals, kernel bounds, heap region, and allocation mode |
| `memmap` | Show Multiboot memory regions or the conservative fallback map |
| `pages` | Show 4 KiB physical page tracker totals and reserved page counts |
| `heap` | Show fixed kernel heap allocator stats |
| `heap-test` | Run small safe bump allocation checks |
| `heap-alloc <bytes>` | Allocate bytes from the bounded kernel heap |
| `paging` | Show paging preparation and enable status |
| `page-map` | Show the identity map and every required runtime range |
| `pagefault-test confirm` | Intentionally fault on an unmapped address and halt; disposable sessions only |

Paging is enabled only through the boot-time validation gate. These commands
do not add page isolation, `free()`, malloc-compatible behavior, or userspace
address spaces.

## Kernel Tasks

| Command | Purpose |
|---|---|
| `tasks` | Show the fixed task table and tick accounting |
| `task-current` | Show the current logical kernel task |
| `task-yield` | Run one ready built-in task step cooperatively |
| `task-demo` | Run one safe three-step demo round |

These commands do not create processes or switch stacks. `task-yield` invokes
a fixed kernel-owned step function and returns to the shell.

## Kernel-Managed Apps

| Command | Purpose |
|---|---|
| `apps` | Show the fixed built-in app registry |
| `app <name>` | Show metadata and run status for one app |
| `run <name>` | Run a compile-time registered app through policy |
| `app-log` | Show app runs, failures, and blocked attempts |

Apps are kernel-managed built-ins, not Ring 3 userspace programs. Unknown app
names are blocked. TinyFS contents and package payloads are never executed.

## TinyFS

| Command | Purpose |
|---|---|
| `fs` | Show availability, format state, limits, LBA range, and last error |
| `fs-format confirm` | Wipe and initialize only TinyFS LBA 64-127 |
| `ls` | List TinyFS file metadata |
| `touch <name>` | Create an empty file |
| `write <name> <text>` | Store bounded text; remaining line is the content |
| `cat <name>` | Verify checksum and print file text |
| `rm <name>` | Delete the file and wipe its referenced data sectors |

Names are 1-24 characters using letters, digits, `.`, `_`, or `-`. Files are
limited to 2048 bytes. TinyFS never interprets file contents as code.

## GhostAttach And Logs

| Command | Purpose |
|---|---|
| `devices` | Device status table |
| `drivers` | Driver registry table |
| `ghostattach` | Explain driver recovery policy |
| `errors` | Warning/error/panic log entries |
| `log` | Full structured log |
| `logs-ai` | Machine-readable log block |
| `export-logs` | Send full boot report to serial |

## RecoveryEngine

| Command | Purpose |
|---|---|
| `recovery` | Recovery subsystem status |
| `actions` | Fixed action catalog |
| `suggestions` | Suggested actions |
| `approve <id>` | Approve and apply a recovery action |
| `rollback <id>` | Roll back a reversible action |
| `policy` | Recovery action policy |

## Persistence And Health

| Command | Purpose |
|---|---|
| `memory` | RecoveryMemory status |
| `safe-mode` | Safe-mode status |
| `schedule-safe-mode` | Schedule one-shot safe mode |
| `store` | RecoveryStore status |
| `journal` | Durable journal |
| `snapshots` | Boot health snapshots |
| `health` | Current health score and trend |
| `timeline` | Journal and snapshots as a timeline |
| `clear-memory confirm` | Erase CMOS recovery state |
| `clear-store confirm` | Erase durable store, journal, and CMOS |

## Packages

| Command | Purpose |
|---|---|
| `versions` | Module versions |
| `packages` | Recovery package registry |
| `package <id>` | Manifest, policy, signature, and state details |
| `approve-pkg <id>` | Approve package if policy allows |
| `apply-pkg <id>` | Apply simulated metadata update |
| `verify-pkg <id>` | Verify applied package |
| `rollback-pkg <id>` | Return to known-good slot |
| `manifests` | Package manifest table |
| `slots` | Module A/B slot metadata |
| `trust` | Trust anchor and signature mode |
| `seed-manifests confirm` | Persist built-in manifests to disk |
| `reset-packages confirm` | Clear package states and reset slots |
