"""Create runnable release files and checksums from a CMake install tree."""

import argparse
import hashlib
import pathlib
import shutil
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("install", type=pathlib.Path)
parser.add_argument("name")
parser.add_argument("--client", action="store_true")
parser.add_argument("--licenses", type=pathlib.Path)
parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("dist"))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
program = "qchat-client" if args.client else "qchat-server"
bundles = list(args.install.glob("*.app"))
if bundles:
    destination = args.output / f"{args.name}.zip"
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
        for file in sorted(bundles[0].rglob("*")):
            if file.is_file():
                archive.write(file, file.relative_to(args.install))
else:
    executable = args.install / "bin" / program
    if not executable.exists():
        executable = executable.with_suffix(".exe")
    destination = args.output / (args.name + executable.suffix)
    shutil.copy2(executable, destination)

digest = hashlib.sha256(destination.read_bytes()).hexdigest()
(args.output / f"{args.name}.sha256").write_text(f"{digest}  {destination.name}\n")
with zipfile.ZipFile(
    args.output / f"{args.name}-notices.zip", "w", zipfile.ZIP_DEFLATED
) as archive:
    for name in ("LICENSE", "README.md", "THIRD_PARTY.md", "vcpkg.json"):
        archive.write(name)
    for file in sorted(pathlib.Path("third_party").glob("*.txt")):
        archive.write(file)
    if args.licenses:
        for file in sorted(args.licenses.rglob("copyright")):
            archive.write(
                file, pathlib.Path("dependencies") / file.relative_to(args.licenses)
            )
