# KamikazeOS Makefile
#
# Works on Linux, macOS, WSL, and Windows (GNU make + LLVM on PATH, e.g.
#   winget install ezwinports.make LLVM.LLVM
# On Windows you can also just use scripts\build.ps1 / scripts\run.ps1.)
#
# Pipeline:  *.c, *.S  --clang-->  *.o  --ld.lld + linker.ld-->  kamikaze.elf
#
# Key flags explained:
#   --target=i686-pc-none-elf   cross-compile: 32-bit x86, NO operating
#                               system, ELF output (clang cross-compiles
#                               out of the box; no custom GCC build needed)
#   -march=i686 -mno-sse ...    plain i686 only: the kernel hasn't enabled
#                               SSE (CR4 setup), so vectorized instructions
#                               clang would emit at -O2 triple-fault at boot
#   -ffreestanding              "there is no C standard library below us"
#   -fno-stack-protector        the stack-guard runtime doesn't exist here
#   -nostdlib                   don't link any libc or startup files

# Plain := (not ?=): make has BUILT-IN defaults CC=cc and LD=ld, so ?= would
# never apply. A command-line `make CC=...` still overrides these.
CC      := clang
LD      := ld.lld
QEMU    := qemu-system-i386

BUILD   := build
KERNEL  := kernel
ELF     := $(BUILD)/kamikaze.elf

CFLAGS  := --target=i686-pc-none-elf -m32 \
           -march=i686 -mno-sse -mno-sse2 -mno-mmx -msoft-float \
           -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
           -Wall -Wextra -O2 -g

SRCS    := $(wildcard $(KERNEL)/src/*.c)
OBJS    := $(BUILD)/boot.o $(patsubst $(KERNEL)/src/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all run run-serial clean reset-store analyze dashboard-json dashboard-open import-example-package export-manifests

# The durable recovery disk: a 1 MiB raw image QEMU attaches as the primary
# IDE disk. The kernel keeps its RecoveryStore here (state + journal), so
# recovery memory survives full QEMU restarts. `make clean` deliberately
# KEEPS this file — your recovery history is data, not a build artifact.
STORE := $(BUILD)/recovery.img

$(STORE): | $(BUILD)
	dd if=/dev/zero of=$(STORE) bs=512 count=2048

all: $(ELF)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: $(KERNEL)/boot/boot.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(KERNEL)/src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJS) $(KERNEL)/linker.ld
	$(LD) -T $(KERNEL)/linker.ld -nostdlib -static -o $@ $(OBJS)
	@echo "Build OK -> $@"

# Boot in QEMU. -serial file:... captures the kernel's live AI-readable log
# stream on the host. The recovery image rides along as the primary IDE
# disk. (No -no-reboot anymore: the shell's `reboot` command should really
# reboot so persistence can be observed across restarts.)
run: $(ELF) $(STORE)
	$(QEMU) -kernel $(ELF) -m 64 -serial file:$(BUILD)/serial.log \
	        -drive file=$(STORE),format=raw,if=ide

# Headless variant: no display window, just the serial capture — handy for
# CI-style runs where only build/serial.log matters.
run-serial: $(ELF) $(STORE)
	$(QEMU) -kernel $(ELF) -m 64 -display none -serial file:$(BUILD)/serial.log \
	        -drive file=$(STORE),format=raw,if=ide

# clean removes build artifacts but PRESERVES build/recovery.img —
# durable recovery state is data, not a build artifact.
clean:
	rm -f $(BUILD)/*.o $(BUILD)/*.elf $(BUILD)/serial.log

# reset-store wipes the durable recovery disk (state + journal history).
reset-store:
	rm -f $(STORE)

# Host-side AI Recovery Core: analyze the captured serial log.
analyze:
	python ai-recovery/analyze_logs.py $(BUILD)/serial.log

# Regenerate the Recovery Center dashboard data from the serial log.
dashboard-json:
	python ai-recovery/export_dashboard_json.py $(BUILD)/serial.log recovery-center/data/recovery-report.json

# The dashboard is a plain local file — no server needed.
dashboard-open:
	@echo "open this file in your browser:"
	@echo "  recovery-center/index.html"

# v1.1 offline manifest tooling. These host tools write only the package
# manifest sector range (LBA 32-39); the kernel still validates imports.
import-example-package: $(STORE)
	python tools/package-builder/import_manifest.py packages/examples/PKG-101-ghostattach-policy.json $(STORE) --slot 0

export-manifests:
	python tools/package-builder/export_manifests.py $(STORE) --out build/exported-manifests.json
