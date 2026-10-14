"""Headless QEMU driver for KamikazeOS regression tests.

Boots the kernel with `-display none`, the QEMU monitor on stdio (works
inside sandboxes that kill listening sockets), the serial log captured to a
file, and a DISPOSABLE disk image. Keystrokes are injected with the
monitor's `sendkey`; evidence is read back from the serial log, which the
kernel writes line-by-line as structured `code=...;severity=...` events.
"""

from __future__ import annotations

import os
import json
import re
import shutil
import subprocess
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

KEYMAP = {" ": "spc", "-": "minus", ".": "dot", "_": "shift-minus",
          "/": "slash", "=": "equal", ",": "comma", ";": "semicolon",
          ":": "shift-semicolon", "'": "apostrophe"}


def qemu_binary() -> str:
    found = shutil.which("qemu-system-i386")
    if found:
        return found
    default = r"C:\Program Files\qemu\qemu-system-i386.exe"
    if os.path.exists(default):
        return default
    raise FileNotFoundError("qemu-system-i386 not found")


class BootFailed(RuntimeError):
    pass


class QemuSession:
    """One QEMU process = one power-on. Use as a context manager."""

    def __init__(self, elf: Path, image: Path, serial_log: Path,
                 boot_timeout: float = 60.0, read_error_lbas: tuple[int, ...] = ()):
        self.elf = Path(elf)
        self.image = Path(image)
        self.serial_log = Path(serial_log)
        self.boot_timeout = boot_timeout
        self.read_error_lbas = read_error_lbas
        self.proc: subprocess.Popen | None = None
        self.monitor_out: list[bytes] = []
        self.offset = 0          # serial-log read cursor for wait_for()

    # ------------------------------------------------------------ process
    def __enter__(self) -> "QemuSession":
        self.start()
        return self

    def __exit__(self, *exc) -> None:
        self.quit()

    def start(self) -> None:
        # Append mode keeps evidence from earlier power-ons in the same log.
        self.offset = self.serial_log.stat().st_size if self.serial_log.exists() else 0
        args = [qemu_binary(), "-kernel", str(self.elf), "-m", "64",
                "-display", "none", "-monitor", "stdio",
                "-chardev", f"file,id=ser0,path={self.serial_log},append=on",
                "-serial", "chardev:ser0"]
        if self.read_error_lbas:
            # Inject read failures at QEMU's block layer, without changing
            # any image bytes. This affects only this disposable session.
            backend = {"driver": "raw", "node-name": "faultdisk",
                       "file": {"driver": "blkdebug",
                       "image": {"driver": "file", "filename": str(self.image)},
                       "inject-error": [{"event": "read_aio", "errno": 5,
                                         "sector": lba, "once": False,
                                         "immediately": True}
                                        for lba in self.read_error_lbas]}}
            args += ["-blockdev", json.dumps(backend),
                     "-device", "ide-hd,drive=faultdisk,bus=ide.0,unit=0"]
        else:
            args += ["-drive", f"file={self.image},format=raw,if=ide"]
        self.proc = subprocess.Popen(args, stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT)
        threading.Thread(target=self._drain, daemon=True).start()
        if not self.wait_for(r"\[KAMIKAZEOS_BOOT_REPORT_END\]",
                             timeout=self.boot_timeout, required=False):
            raise BootFailed(f"no boot report within {self.boot_timeout}s; "
                             f"tail: {self.tail(800)!r}")
        # The clean-boot journal record and the health snapshot are written
        # right after the report; give the ATA flushes time to land.
        time.sleep(1.5)

    def _drain(self) -> None:
        assert self.proc and self.proc.stdout
        for line in iter(self.proc.stdout.readline, b""):
            self.monitor_out.append(line)

    def monitor(self, command: str) -> None:
        assert self.proc and self.proc.stdin
        self.proc.stdin.write((command + "\n").encode("ascii"))
        self.proc.stdin.flush()

    def quit(self) -> None:
        if not self.proc:
            return
        if self.proc.poll() is None:
            try:
                self.monitor("quit")
                self.proc.wait(timeout=15)
            except Exception:
                self.proc.kill()
                self.proc.wait(timeout=15)
        self.proc = None

    # ------------------------------------------------------------- input
    def type_line(self, text: str, key_delay: float = 0.03) -> None:
        for ch in text:
            if ch in KEYMAP:
                key = KEYMAP[ch]
            elif ch.isalpha() and ch.isupper():
                key = "shift-" + ch.lower()
            elif ch.isalnum():
                key = ch
            else:
                raise ValueError(f"no sendkey mapping for {ch!r}")
            self.monitor(f"sendkey {key}")
            time.sleep(key_delay)
        self.monitor("sendkey ret")

    def run(self, command: str, expect: str | None = None,
            timeout: float = 30.0, settle: float = 0.6) -> str:
        """Type a shell command. With `expect`, wait for that regex in the
        NEW serial output and return the matching text; otherwise wait
        `settle` seconds (for commands that log nothing)."""
        start = self.serial_size()
        self.type_line(command)
        if expect is None:
            time.sleep(settle)
            return ""
        return self.wait_for(expect, timeout=timeout, since=start, live=True)

    def export_report(self, timeout: float = 60.0) -> dict:
        start = self.serial_size()
        self.type_line("export-logs")
        self.wait_for(r"code=EXP-000", timeout=timeout, since=start)
        text = self.read_serial()[start:]
        return parse_last_report(text)

    # ------------------------------------------------------------ serial
    def serial_size(self) -> int:
        return len(self.read_serial())

    def read_serial(self) -> str:
        try:
            return self.serial_log.read_bytes().decode("ascii", "replace")
        except FileNotFoundError:
            return ""

    def tail(self, n: int) -> str:
        return self.read_serial()[-n:]

    def wait_for(self, pattern: str, timeout: float = 30.0,
                 since: int | None = None, required: bool = True,
                 live: bool = False) -> str:
        """Wait for a regex in serial output. live=True ignores report
        bodies, whose [ERROR_LOG] section repeats earlier events."""
        rx = re.compile(pattern)
        base = self.offset if since is None else since
        deadline = time.time() + timeout
        while time.time() < deadline:
            text = self.read_serial()[base:]
            if live:
                text = strip_reports(text)
            m = rx.search(text)
            if m:
                return m.group(0)
            if self.proc and self.proc.poll() is not None:
                break
            time.sleep(0.1)
        if required:
            raise TimeoutError(f"pattern {pattern!r} not seen; tail: "
                               f"{self.tail(600)!r}")
        return ""

    def boot_text(self) -> str:
        """Serial text produced since this power-on started."""
        return self.read_serial()[self.offset:]


# ----------------------------------------------------------------- report

def parse_last_report(text: str) -> dict:
    """Parse the LAST complete boot report into {section: [row dicts]}.
    Metadata lines before the first section land under 'META'."""
    start = text.rfind("[KAMIKAZEOS_BOOT_REPORT_START]")
    end = text.find("[KAMIKAZEOS_BOOT_REPORT_END]", start)
    if start < 0 or end < 0:
        raise ValueError("no complete boot report in text")
    sections: dict[str, list[dict]] = {"META": []}
    current = "META"
    for raw in text[start:end].splitlines()[1:]:
        line = raw.strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1]
            sections.setdefault(current, [])
            continue
        row = {}
        for field in line.split(";"):
            if "=" in field:
                k, _, v = field.partition("=")
                row[k] = v
        if row:
            sections[current].append(row)
    return sections


def section_kv(report: dict, name: str) -> dict:
    """Merge a key=value-per-line section into one dict."""
    merged: dict = {}
    for row in report.get(name, []):
        merged.update(row)
    return merged


def strip_reports(text: str) -> str:
    """Remove boot-report bodies: their [ERROR_LOG] section re-emits the
    log ring, which would double-count live events."""
    return re.sub(r"\[KAMIKAZEOS_BOOT_REPORT_START\].*?\[KAMIKAZEOS_BOOT_REPORT_END\]",
                  "", text, flags=re.S)


def events(text: str, code: str | None = None) -> list[dict]:
    """Live `code=` events only (report copies excluded)."""
    out = []
    for line in strip_reports(text).splitlines():
        if not line.startswith("code="):
            continue
        row = {}
        for field in line.strip().split(";"):
            k, _, v = field.partition("=")
            row[k] = v
        if code is None or row.get("code") == code:
            out.append(row)
    return out
