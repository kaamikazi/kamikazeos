# Command Reference

KamikazeOS shell commands in v2.6.0.

## General

| Command | Purpose |
|---|---|
| `help` | Show commands |
| `clear` | Clear the screen |
| `about` | Version and mission summary |
| `system` | Integrated v2.6 system readiness summary |
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

## Kernel Syscalls

| Command | Purpose |
|---|---|
| `syscalls` | Show gate state, vector, counters, and the fixed syscall table |
| `syscall-test` | Invoke every registered service with controlled kernel arguments |
| `syscall <id>` | Invoke one ID using its safe built-in test arguments |

The v2.6 syscall gate is prepared for the fixed CPL3 test routine after
privilege and user-memory validation, but the audit hold prevents entry.
Shell tests originate in Ring 0. The dispatcher
accepts bounded scalars and registry indexes, not raw pointers. Unknown IDs fail with
`SYSCALL_ERR_UNKNOWN`; no syscall can approve packages, write disk sectors,
change policy, load apps, or access arbitrary kernel memory.

## Privilege Foundation

| Command | Purpose |
|---|---|
| `gdt` | Show kernel, user, and TSS selectors and descriptor validation |
| `tss` | Show TSS load state, SS0, ESP0, and transition-stack bounds |
| `privilege` | Show current CPL, syscall gate DPL, and privilege prerequisites |
| `ring3-readiness` | Show privilege and minimal user-mapping readiness |
| `ring3` | Show controlled execution state and last result |
| `user-memory` | Show the fixed user code/stack bounds and permission checks |
| `ring3-test` | Fixed linked routine; refused by the default post-verification hold |
| `user-fault-test confirm` | Independently disabled; future destructive CPL3 fault test only |

`prerequisites_valid=yes` means the privilege foundation and minimal user-page
checks passed. It does not authorize execution. One disposable round trip was
approved and verified; another run or an approved
`user-fault-test confirm` must be the final command in a disposable VM.

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
| `rollback-pkg <id>` | Return to known-good slot; refused (`PKG-REJECT-001`) unless this package still owns the running slot and its from-version is still in the rollback slot |
| `manifests` | Package manifest table |
| `slots` | Module A/B slot metadata |
| `trust` | Trust anchor and signature mode |
| `seed-manifests confirm` | Persist missing built-in manifests into empty manifest slots (never overwrites), then re-evaluate packages |
| `reset-packages confirm` | Clear package states and reset slots |
