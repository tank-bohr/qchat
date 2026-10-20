"""Check release audits, packaging, and source verification without network access."""

import contextlib
import hashlib
import io
import json
import pathlib
import runpy
import subprocess
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]


class BuildToolsTests(unittest.TestCase):
    def audit_windows_imports(self, libraries):
        imports = "\n".join(f"    {library}" for library in libraries)
        dumpbin = (
            "Dump of file qchat-client.exe\n"
            "  Image has the following dependencies:\n"
            f"\n{imports}\n\n  Summary\n"
        )
        with (
            patch.object(sys, "platform", "win32"),
            patch.object(sys, "argv", ["audit_binary.py", "qchat-client.exe"]),
            patch("subprocess.check_output", return_value=dumpbin),
            contextlib.redirect_stdout(io.StringIO()),
        ):
            runpy.run_path(str(ROOT / "tools/audit_binary.py"), run_name="__main__")

    def test_windows_audit_accepts_system_apis(self):
        self.audit_windows_imports(
            [
                "KERNEL32.dll",
                "UIAutomationCore.DLL",
                "AUTHZ.dll",
                "icuuc.dll",
                "WINHTTP.dll",
                "ncrypt.dll",
                "api-ms-win-core-winrt-l1-1-0.dll",
            ]
        )

    def test_windows_audit_rejects_application_and_runtime_dlls(self):
        for library in (
            "Qt6Core.dll",
            "thrift.dll",
            "MSVCP140.dll",
            "VCRUNTIME140.dll",
            "icuuc78.dll",
            "unknown.dll",
        ):
            with self.subTest(library=library):
                with self.assertRaisesRegex(
                    SystemExit, "Unexpected shared dependencies"
                ):
                    self.audit_windows_imports(["KERNEL32.dll", library])

    def test_package_checksum_and_notices(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = pathlib.Path(temporary)
            binary = folder / "install/bin/qchat-server"
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b"test executable")
            output = folder / "dist"
            subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "tools/package.py"),
                    str(folder / "install"),
                    "test-server",
                    "--output",
                    str(output),
                ],
                cwd=ROOT,
                check=True,
                capture_output=True,
            )
            self.assertEqual((output / "test-server").read_bytes(), binary.read_bytes())
            self.assertEqual(
                (output / "test-server.sha256").read_text().split()[0],
                hashlib.sha256(binary.read_bytes()).hexdigest(),
            )
            with zipfile.ZipFile(output / "test-server-notices.zip") as archive:
                self.assertIn("LICENSE", archive.namelist())
                self.assertIn("THIRD_PARTY.md", archive.namelist())

    def test_source_archive_rejects_missing_metadata_and_bad_checksums(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = pathlib.Path(temporary)
            share = folder / "share"
            vcpkg = folder / "vcpkg"
            command = [
                sys.executable,
                str(ROOT / "tools/collect_qt_sources.py"),
                str(share),
                str(vcpkg),
                str(folder / "sources.tar.gz"),
            ]
            result = subprocess.run(
                command, capture_output=True, text=True, check=False
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Missing Qt source metadata", result.stderr)
            record = share / "qtbase/vcpkg.spdx.json"
            record.parent.mkdir(parents=True)
            record.write_text(
                json.dumps(
                    {
                        "packages": [
                            {
                                "SPDXID": "SPDXRef-resource-0",
                                "packageFileName": "source.tar.gz",
                                "downloadLocation": "https://example.invalid/source.tar.gz",
                                "checksums": [
                                    {"algorithm": "SHA512", "checksumValue": "0" * 128}
                                ],
                            }
                        ]
                    }
                )
            )
            cached = vcpkg / "downloads/source.tar.gz"
            cached.parent.mkdir(parents=True)
            cached.write_bytes(b"corrupted archive")
            result = subprocess.run(
                command, capture_output=True, text=True, check=False
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("checksum mismatch", result.stderr)
            self.assertFalse((folder / "sources.tar.gz").exists())


if __name__ == "__main__":
    unittest.main()
