"""Bundle the exact Qt sources and patched build recipes recorded by vcpkg."""

import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

parser = argparse.ArgumentParser()
parser.add_argument("share", type=pathlib.Path)
parser.add_argument("vcpkg", type=pathlib.Path)
parser.add_argument("output", type=pathlib.Path)
args = parser.parse_args()
metadata = sorted(args.share.glob("qt*/vcpkg.spdx.json"))
if not metadata:
    raise SystemExit(
        "Missing Qt source metadata; refusing to publish an incomplete source archive"
    )
args.output.parent.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory() as temporary:
    staging = pathlib.Path(temporary)
    for record in metadata:
        found = False
        port = record.parent.name
        for package in json.loads(record.read_text())["packages"]:
            if not package.get("SPDXID", "").startswith("SPDXRef-resource-"):
                continue
            checksums = {
                item["algorithm"]: item["checksumValue"]
                for item in package.get("checksums", [])
            }
            url = package.get("downloadLocation", "")
            if not url.startswith("https://") or "SHA512" not in checksums:
                raise SystemExit(f"Unverifiable source resource in {record}: {url}")
            name = pathlib.Path(package["packageFileName"]).name
            destination = staging / f"{port}-{name}"
            cached = args.vcpkg / "downloads" / name
            if cached.is_file():
                shutil.copyfile(cached, destination)
            else:
                with urllib.request.urlopen(url, timeout=120) as response:
                    with destination.open("wb") as output:
                        shutil.copyfileobj(response, output)
            with destination.open("rb") as source:
                digest = hashlib.file_digest(source, "sha512").hexdigest()
            if digest != checksums["SHA512"].lower():
                raise SystemExit(f"Source checksum mismatch: {destination.name}")
            found = True
        if not found:
            raise SystemExit(f"No corresponding sources recorded in {record}")
        shutil.copyfile(record, staging / f"{port}.spdx.json")
    # Contains the exact patches, toolchains, and port definitions used by the build.
    subprocess.run(
        [
            "git",
            "-C",
            str(args.vcpkg),
            "archive",
            "--format=tar.gz",
            f"--output={staging / 'vcpkg-build-recipes.tar.gz'}",
            "HEAD",
        ],
        check=True,
    )
    with tarfile.open(args.output, "w:gz") as archive:
        for file in sorted(staging.iterdir()):
            archive.add(file, arcname=file.name)
print(f"Created {args.output}")
