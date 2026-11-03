#!/usr/bin/env python3
"""Focused validation tests for the offline manifest boundary."""

import copy
import json
import unittest
from pathlib import Path

from manifest_lib import Manifest, load_manifest, validate_manifest


class ManifestValidationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.safe = load_manifest(
            Path("packages/examples/PKG-101-ghostattach-policy.json")
        )

    def test_signed_example_passes(self):
        self.assertTrue(validate_manifest(self.safe)[0])

    def test_report_delimiter_is_rejected(self):
        manifest = copy.deepcopy(self.safe)
        manifest.name = "bad;field"
        ok, errors, _ = validate_manifest(manifest)
        self.assertFalse(ok)
        self.assertTrue(any("report delimiter" in error for error in errors))

    def test_unknown_enum_fails_without_exception(self):
        manifest = copy.deepcopy(self.safe)
        manifest.risk = "unexpected"
        manifest.checksum = None
        ok, errors, _ = validate_manifest(manifest)
        self.assertFalse(ok)
        self.assertTrue(any("risk must be" in error for error in errors))

    def test_string_boolean_is_not_coerced(self):
        data = json.loads(
            Path("packages/examples/PKG-101-ghostattach-policy.json").read_text()
        )
        data["reversible"] = "false"
        with self.assertRaises(ValueError):
            Manifest.from_json(data)

    def test_unknown_top_level_field_is_rejected(self):
        data = self.safe.to_json()
        data["source"] = "disk_imported"
        with self.assertRaises(ValueError):
            Manifest.from_json(data)


if __name__ == "__main__":
    unittest.main()
