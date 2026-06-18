# Kernel Interrupts

KamikazeOS v1.6.0 "Kernel Discipline" adds the first real interrupt and CPU
exception foundation to the 32-bit x86 kernel.

## Boot Sequence

The Multiboot entry disables maskable interrupts before entering C. After the
console, serial log, durable recovery state, and Multiboot checks are ready,
the kernel:

1. Builds and loads a 256-entry Interrupt Descriptor Table with `lidt`.
2. Remaps the two legacy 8259 PICs to vectors 32-47.
3. Masks all PIC lines, configures PIT channel 0 for 100 Hz, and unmasks IRQ0.
4. Probes the PS/2 controller and unmasks IRQ1 when the keyboard is present.
5. Enables interrupts with `sti`.

All other PIC IRQ lines remain masked. There is no APIC support yet.

## Exception Handling

Assembly stubs normalize CPU stack frames and call a common C dispatcher.
Dedicated names and diagnostics exist for all vectors 0-31, including:

- vector 0: divide by zero
- vector 6: invalid opcode
- vector 13: general protection fault
- vector 14: page fault placeholder
- unknown/reserved exception vectors

The handler prints the exception name, vector, CPU error code, and instruction
pointer; writes a structured `CPU-EXCEPTION` event; streams a
`[KERNEL_EXCEPTION_PANIC]` evidence line to serial; records panic state; then
halts with interrupts disabled. v1.6 never attempts recovery from a CPU
exception.

## Timer

The PIT runs in rate-generator mode at 100 Hz. IRQ0 increments a 32-bit tick
counter and sends an end-of-interrupt acknowledgement to the PIC. The counter
is diagnostic uptime evidence, not a scheduler clock.

## Keyboard

IRQ1 reads PS/2 set-1 scancodes, maintains Shift state, translates the existing
basic US key map, and writes characters into a fixed 64-byte ring buffer. The
shell blocks with `hlt` while the buffer is empty, so PIT ticks continue.

If IRQ mode cannot be enabled, the original polling path remains available and
reports `keyboard_mode=polling_fallback`.

## Operator Commands

```text
interrupts
ticks
exceptions-test divide confirm
exceptions-test invalid-opcode confirm
```

The exception tests deliberately halt the VM after exporting evidence. The
required `confirm` suffix prevents accidental activation.

## Report Contract

Serial reports contain `[KERNEL_INTERRUPTS]`, `[KERNEL_TIMING]`, and
`[KERNEL_EXCEPTIONS]`. The analyzer and Recovery Center consume these sections
read-only and never attempt repair.

## Current Limits

- no userspace or privilege separation
- no scheduler or context switching
- no processes or real process management
- no virtual memory or page-table ownership
- no syscall interface
- no APIC, SMP, or per-CPU interrupt state
- no recovery from CPU exceptions
