# KamikazeOS v2.4.0 "Syscall Door"

v2.4 adds the first controlled system-call foundation to KamikazeOS. A DPL0
`int 0x80` gate enters a normalized assembly stub and fixed eight-entry kernel
dispatcher. Calls accept only bounded scalar values and registry indexes; no
call accepts a raw pointer from its caller.

The release adds `syscalls`, `syscall-test`, and `syscall <id>`, plus
`[SYSCALLS]` and `[SYSCALL_TABLE]` reports. The offline analyzer explains
failures without automatic repair, and Recovery Center displays syscall state
from exported JSON without calling the OS or changing disk state.

This remains an experimental OS research prototype, not a production or
general-purpose operating system. The syscall interface is kernel-internal
preparation only. There is no Ring 3 execution, userspace pointer handling,
process isolation, ELF loading, arbitrary code execution, networking,
downloads, binary patching, automatic repair, or kernel self-modification.

The v2.3 paging guarantees remain intact: paging is enabled only after strict
identity-map validation, and page faults capture CR2 evidence before halting.
