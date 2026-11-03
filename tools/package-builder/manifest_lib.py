#!/usr/bin/env python3
"""Shared KamikazeOS offline manifest tooling.

This module mirrors kernel/src/package_manifest.c. Keep the constants in sync
with the kernel: host tools prepare sectors, but the kernel remains the source
of truth and validates every imported manifest again at boot.
"""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from trust_anchor import (
    TRUST_ANCHOR_FINGERPRINT,
    TRUST_ANCHOR_ID,
    TRUST_ANCHOR_NAME,
    TRUSTED_PUBLIC_KEY_RAW_HEX,
    TRUSTED_PUBLIC_KEY_SHA256,
)

SECTOR_SIZE = 512
LBA_MANIFESTS = 32
MANIFEST_MAX = 8
MANIFEST_LBA_END = LBA_MANIFESTS + MANIFEST_MAX - 1

PM_ID_MAX = 10
PM_NAME_MAX = 32
PM_DESC_MAX = 48
PM_TARGET_MAX = 20
PM_VER_MAX = 8

MF_MAGIC = b"KMFT"
MF_FMT_VERSION = 1

O_MAGIC = 0
O_FMTVER = 4
O_ID = 5
O_NAME = O_ID + PM_ID_MAX
O_DESC = O_NAME + PM_NAME_MAX
O_TARGET = O_DESC + PM_DESC_MAX
O_FROM = O_TARGET + PM_TARGET_MAX
O_TO = O_FROM + PM_VER_MAX
O_RISK = O_TO + PM_VER_MAX
O_REV = O_RISK + 1
O_REQ = O_REV + 1
O_SIG = O_REQ + 1
O_ANCHOR = O_SIG + 1
O_MVER = O_ANCHOR + 1
O_PAYLOAD = O_MVER + 1
O_MODULE = O_PAYLOAD + 1
O_CHECKSUM = O_MODULE + 1
O_SIG_PRESENT = O_CHECKSUM + 1
O_SIG_ALG = O_SIG_PRESENT + 1
O_SIG_KEY_HASH = O_SIG_ALG + 1
O_SIG_PAYLOAD_HASH = O_SIG_KEY_HASH + 17

SIG_ALGORITHMS = {"none": 0, "ed25519": 1}
SIG_ALGORITHM_NAMES = {v: k for k, v in SIG_ALGORITHMS.items()}

RISKS = {"low": 0, "medium": 1, "high": 2}
RISK_NAMES = {v: k for k, v in RISKS.items()}
SIGNATURES = {
    "unsigned": 0,
    "signed_placeholder": 1,
    "invalid": 2,
    "trusted": 3,
}
SIGNATURE_NAMES = {v: k for k, v in SIGNATURES.items()}
PAYLOADS = {
    "metadata_only": 0,
    "module_slot_switch_simulated": 1,
    "blocked_driver_install": 2,
    "unknown": 3,
}
PAYLOAD_NAMES = {v: k for k, v in PAYLOADS.items()}
MODULES = {
    "Kernel": 0,
    "GhostAttach": 1,
    "RecoveryEngine": 2,
    "RecoveryMemory": 3,
    "RecoveryStore": 4,
    "AI Recovery Core Rules": 5,
    "Recovery Center Schema": 6,
}
MODULE_NAMES = {v: k for k, v in MODULES.items()}
UPDATABLE_MODULES = {
    "GhostAttach", "RecoveryEngine", "AI Recovery Core Rules",
    "Recovery Center Schema",
}

MANIFEST_REQUIRED_FIELDS = {
    "package_id", "name", "description", "target_component",
    "target_module", "from_version", "to_version", "risk", "reversible",
    "requires_approval", "signature_status", "trust_anchor_id",
    "manifest_version", "payload_type",
}
MANIFEST_OPTIONAL_FIELDS = {"checksum", "signature"}


@dataclass
class Manifest:
    package_id: str
    name: str
    description: str
    target_component: str
    target_module: str
    from_version: str
    to_version: str
    risk: str
    reversible: bool
    requires_approval: bool
    signature_status: str
    trust_anchor_id: int
    manifest_version: int
    payload_type: str
    checksum: int | None = None
    signature: dict[str, Any] | None = None

    @classmethod
    def from_json(cls, data: dict[str, Any]) -> "Manifest":
        if not isinstance(data, dict):
            raise ValueError("manifest root must be a JSON object")
        missing = MANIFEST_REQUIRED_FIELDS - set(data)
        extra = set(data) - MANIFEST_REQUIRED_FIELDS - MANIFEST_OPTIONAL_FIELDS
        if missing:
            raise ValueError(f"missing manifest fields: {', '.join(sorted(missing))}")
        if extra:
            raise ValueError(f"unknown manifest fields: {', '.join(sorted(extra))}")
        if not isinstance(data["reversible"], bool) or \
                not isinstance(data["requires_approval"], bool):
            raise ValueError("reversible and requires_approval must be JSON booleans")
        return cls(
            package_id=str(data["package_id"]),
            name=str(data["name"]),
            description=str(data["description"]),
            target_component=str(data["target_component"]),
            target_module=str(data["target_module"]),
            from_version=str(data["from_version"]),
            to_version=str(data["to_version"]),
            risk=str(data["risk"]),
            reversible=data["reversible"],
            requires_approval=data["requires_approval"],
            signature_status=str(data["signature_status"]),
            trust_anchor_id=int(data["trust_anchor_id"]),
            manifest_version=int(data["manifest_version"]),
            payload_type=str(data["payload_type"]),
            checksum=(None if data.get("checksum") is None else int(data["checksum"])),
            signature=data.get("signature"),
        )

    def to_json(self, include_checksum: bool = True) -> dict[str, Any]:
        out = {
            "package_id": self.package_id,
            "name": self.name,
            "description": self.description,
            "target_component": self.target_component,
            "target_module": self.target_module,
            "from_version": self.from_version,
            "to_version": self.to_version,
            "risk": self.risk,
            "reversible": self.reversible,
            "requires_approval": self.requires_approval,
            "signature_status": self.signature_status,
            "trust_anchor_id": self.trust_anchor_id,
            "manifest_version": self.manifest_version,
            "payload_type": self.payload_type,
        }
        if include_checksum:
            out["checksum"] = self.expected_checksum()
        if self.signature:
            out["signature"] = self.signature
        return out

    def target_module_id(self) -> int:
        if self.target_module not in MODULES:
            raise ValueError(f"unknown target_module: {self.target_module}")
        return MODULES[self.target_module]

    def expected_checksum(self) -> int:
        text_fields = (
            self.package_id,
            self.name,
            self.description,
            self.target_component,
            self.from_version,
            self.to_version,
        )
        total = 0
        for field in text_fields:
            for ch in field:
                total = (total + ord(ch)) & 0xFF
        total = (total + RISKS[self.risk]) & 0xFF
        total = (total + int(self.reversible)) & 0xFF
        total = (total + int(self.requires_approval)) & 0xFF
        total = (total + SIGNATURES[self.signature_status]) & 0xFF
        total = (total + self.trust_anchor_id) & 0xFF
        total = (total + self.manifest_version) & 0xFF
        total = (total + PAYLOADS[self.payload_type]) & 0xFF
        total = (total + self.target_module_id()) & 0xFF
        return total

    def signed_payload(self) -> bytes:
        """Canonical payload signed by host tools.

        Excludes checksum and signature fields so signatures stay stable while
        the deterministic checksum is recomputed.
        """
        data = self.to_json(include_checksum=False)
        data.pop("signature", None)
        return json.dumps(data, sort_keys=True, separators=(",", ":")).encode("utf-8")

    def payload_sha256(self) -> str:
        return hashlib.sha256(self.signed_payload()).hexdigest()


def put_str(sec: bytearray, offset: int, value: str, cap: int) -> None:
    raw = value.encode("ascii")
    sec[offset : offset + cap] = b"\0" * cap
    sec[offset : offset + min(len(raw), cap - 1)] = raw[: cap - 1]


def get_str(sec: bytes, offset: int, cap: int) -> str:
    raw = sec[offset : offset + cap].split(b"\0", 1)[0]
    return raw.decode("ascii", errors="replace")


def load_manifest(path: Path) -> Manifest:
    return Manifest.from_json(json.loads(path.read_text()))


def validate_manifest(m: Manifest) -> tuple[bool, list[str], list[str]]:
    errors: list[str] = []
    warnings: list[str] = []

    def check_ascii_len(label: str, value: str, cap: int) -> None:
        try:
            raw = value.encode("ascii")
        except UnicodeEncodeError:
            errors.append(f"{label} must be ASCII")
            return
        if len(raw) >= cap:
            errors.append(f"{label} too long: max {cap - 1} bytes")
        if not raw:
            errors.append(f"{label} must not be empty")
        if any(byte < 0x20 or byte > 0x7E or byte in (ord(";"), ord("="))
               for byte in raw):
            errors.append(f"{label} contains a control character or report delimiter")

    check_ascii_len("package_id", m.package_id, PM_ID_MAX)
    check_ascii_len("name", m.name, PM_NAME_MAX)
    check_ascii_len("description", m.description, PM_DESC_MAX)
    check_ascii_len("target_component", m.target_component, PM_TARGET_MAX)
    check_ascii_len("from_version", m.from_version, PM_VER_MAX)
    check_ascii_len("to_version", m.to_version, PM_VER_MAX)

    if not m.package_id.startswith("PKG-"):
        errors.append("package_id must start with PKG-")
    if not m.package_id[4:].isdigit():
        errors.append("package_id must end in decimal digits")
    if m.risk not in RISKS:
        errors.append(f"risk must be one of: {', '.join(RISKS)}")
    if m.signature_status not in SIGNATURES:
        errors.append(f"signature_status must be one of: {', '.join(SIGNATURES)}")
    if m.payload_type not in PAYLOADS:
        errors.append(f"payload_type must be one of: {', '.join(PAYLOADS)}")
    if m.target_module not in MODULES:
        errors.append(f"target_module must be one of: {', '.join(MODULES)}")
    elif m.target_module not in UPDATABLE_MODULES:
        errors.append("target_module must have a managed A/B slot")
    if not 0 <= m.trust_anchor_id <= 255:
        errors.append("trust_anchor_id must fit in one byte")
    if m.manifest_version != 1:
        errors.append("manifest_version must be 1")
    if not m.requires_approval:
        errors.append("unsafe: packages must require explicit human approval")
    if m.from_version == m.to_version:
        errors.append("from_version and to_version must differ")
    enums_valid = m.risk in RISKS and m.signature_status in SIGNATURES and \
        m.payload_type in PAYLOADS and m.target_module in MODULES
    if m.checksum is not None and enums_valid and m.checksum != m.expected_checksum():
        errors.append(
            f"checksum mismatch: manifest has {m.checksum}, expected {m.expected_checksum()}"
        )

    sig_ok, sig_reason = verify_manifest_signature(m)
    if m.signature_status in ("unsigned", "invalid", "signed_placeholder"):
        errors.append("unsafe: imported manifests must have trusted Ed25519 signatures")
    if not sig_ok:
        errors.append(f"signature invalid: {sig_reason}")
    if m.trust_anchor_id != TRUST_ANCHOR_ID:
        errors.append(f"unsafe: trust_anchor_id must be {TRUST_ANCHOR_ID}")
    if m.risk == "high":
        errors.append("unsafe: high-risk packages are blocked")
    if not m.reversible:
        errors.append("unsafe: irreversible packages are blocked")
    if m.payload_type in ("blocked_driver_install", "unknown"):
        errors.append(f"unsafe: payload type {m.payload_type} is blocked")
    warnings.append("host signature verification is real Ed25519")
    warnings.append("kernel-side signature verification is simulated in v1.2")

    return not errors, errors, warnings


def verify_manifest_signature(m: Manifest, public_key_pem: bytes | None = None) -> tuple[bool, str]:
    sig = m.signature or {}
    if sig.get("algorithm") != "ed25519":
        return False, "missing ed25519 signature"
    try:
        signature = bytes.fromhex(str(sig["signature"]))
        embedded_key = bytes.fromhex(str(sig["public_key"]))
        pinned_key = bytes.fromhex(TRUSTED_PUBLIC_KEY_RAW_HEX)
        if embedded_key != pinned_key:
            return False, "signer key does not match pinned KMZ-ROOT-1"
        if sig.get("key_id") != TRUST_ANCHOR_NAME:
            return False, "signature key_id does not match KMZ-ROOT-1"
        expected_payload_hash = m.payload_sha256()
        if sig.get("payload_sha256") != expected_payload_hash:
            return False, "payload hash mismatch"
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
        if public_key_pem:
            pub = serialization.load_pem_public_key(public_key_pem)
            supplied_raw = pub.public_bytes(
                encoding=serialization.Encoding.Raw,
                format=serialization.PublicFormat.Raw,
            )
            if supplied_raw != pinned_key:
                return False, "supplied public key is not the pinned trust anchor"
        else:
            pub = Ed25519PublicKey.from_public_bytes(pinned_key)
        pub.verify(signature, m.signed_payload())
        actual_key_hash = hashlib.sha256(
            pub.public_bytes(
                encoding=serialization.Encoding.Raw,
                format=serialization.PublicFormat.Raw,
            )
        ).hexdigest()
        if actual_key_hash != TRUSTED_PUBLIC_KEY_SHA256 or \
                sig.get("public_key_sha256") != TRUSTED_PUBLIC_KEY_SHA256:
            return False, "public key fingerprint mismatch"
        return True, "ok"
    except Exception as exc:
        return False, str(exc) or "verification failed"


def manifest_to_sector(m: Manifest) -> bytes:
    sec = bytearray(SECTOR_SIZE)
    sec[O_MAGIC : O_MAGIC + 4] = MF_MAGIC
    sec[O_FMTVER] = MF_FMT_VERSION
    put_str(sec, O_ID, m.package_id, PM_ID_MAX)
    put_str(sec, O_NAME, m.name, PM_NAME_MAX)
    put_str(sec, O_DESC, m.description, PM_DESC_MAX)
    put_str(sec, O_TARGET, m.target_component, PM_TARGET_MAX)
    put_str(sec, O_FROM, m.from_version, PM_VER_MAX)
    put_str(sec, O_TO, m.to_version, PM_VER_MAX)
    sec[O_RISK] = RISKS[m.risk]
    sec[O_REV] = 1 if m.reversible else 0
    sec[O_REQ] = 1 if m.requires_approval else 0
    sec[O_SIG] = SIGNATURES[m.signature_status]
    sec[O_ANCHOR] = m.trust_anchor_id
    sec[O_MVER] = m.manifest_version
    sec[O_PAYLOAD] = PAYLOADS[m.payload_type]
    sec[O_MODULE] = m.target_module_id()
    sec[O_CHECKSUM] = m.expected_checksum()
    sig = m.signature or {}
    sig_ok, _reason = verify_manifest_signature(m)
    sec[O_SIG_PRESENT] = 1 if sig else 0
    sec[O_SIG_ALG] = SIG_ALGORITHMS.get(sig.get("algorithm", "none"), 0)
    key_hash = str(sig.get("public_key_sha256", ""))[:16].encode("ascii")
    payload_hash = str(sig.get("payload_sha256", ""))[:16].encode("ascii")
    sec[O_SIG_KEY_HASH : O_SIG_KEY_HASH + 17] = b"\0" * 17
    sec[O_SIG_PAYLOAD_HASH : O_SIG_PAYLOAD_HASH + 17] = b"\0" * 17
    sec[O_SIG_KEY_HASH : O_SIG_KEY_HASH + len(key_hash)] = key_hash
    sec[O_SIG_PAYLOAD_HASH : O_SIG_PAYLOAD_HASH + len(payload_hash)] = payload_hash
    if sig_ok:
        sec[O_SIG_PRESENT] = 2
    return bytes(sec)


def sector_to_manifest(sec: bytes) -> Manifest | None:
    if len(sec) != SECTOR_SIZE:
        raise ValueError("sector must be exactly 512 bytes")
    if sec[O_MAGIC : O_MAGIC + 4] != MF_MAGIC or sec[O_FMTVER] != MF_FMT_VERSION:
        return None
    risk = RISK_NAMES.get(sec[O_RISK], "high")
    signature_status = SIGNATURE_NAMES.get(sec[O_SIG], "invalid")
    payload = PAYLOAD_NAMES.get(sec[O_PAYLOAD], "unknown")
    module = MODULE_NAMES.get(sec[O_MODULE], "Kernel")
    sig_present = sec[O_SIG_PRESENT]
    sig_alg = SIG_ALGORITHM_NAMES.get(sec[O_SIG_ALG], "none")
    signature = None
    if sig_present:
        signature = {
            "algorithm": sig_alg,
            "kernel_sector_marker": "host_verified" if sig_present == 2 else "present_unverified",
            "public_key_sha256_prefix": get_str(sec, O_SIG_KEY_HASH, 17),
            "payload_sha256_prefix": get_str(sec, O_SIG_PAYLOAD_HASH, 17),
        }
    return Manifest(
        package_id=get_str(sec, O_ID, PM_ID_MAX),
        name=get_str(sec, O_NAME, PM_NAME_MAX),
        description=get_str(sec, O_DESC, PM_DESC_MAX),
        target_component=get_str(sec, O_TARGET, PM_TARGET_MAX),
        target_module=module,
        from_version=get_str(sec, O_FROM, PM_VER_MAX),
        to_version=get_str(sec, O_TO, PM_VER_MAX),
        risk=risk,
        reversible=bool(sec[O_REV]),
        requires_approval=bool(sec[O_REQ]),
        signature_status=signature_status,
        trust_anchor_id=sec[O_ANCHOR],
        manifest_version=sec[O_MVER],
        payload_type=payload,
        checksum=sec[O_CHECKSUM],
        signature=signature,
    )


def read_manifest_slots(image: Path) -> list[Manifest | None]:
    with image.open("rb") as f:
        f.seek(LBA_MANIFESTS * SECTOR_SIZE)
        return [sector_to_manifest(f.read(SECTOR_SIZE)) for _ in range(MANIFEST_MAX)]


def write_manifest_slot(image: Path, slot: int, manifest: Manifest) -> None:
    if slot < 0 or slot >= MANIFEST_MAX:
        raise ValueError(f"slot must be 0..{MANIFEST_MAX - 1}")
    with image.open("r+b") as f:
        f.seek((LBA_MANIFESTS + slot) * SECTOR_SIZE)
        f.write(manifest_to_sector(manifest))
