# Command Reference

KamikazeOS shell commands in v1.6.0.

## General

| Command | Purpose |
|---|---|
| `help` | Show commands |
| `clear` | Clear the screen |
| `about` | Version and mission summary |
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
