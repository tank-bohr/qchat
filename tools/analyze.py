"""Run analyzers only on authored translation units, using their real build commands."""

import argparse
import json
import pathlib
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("build", type=pathlib.Path)
parser.add_argument("--clang-tidy", default="clang-tidy")
parser.add_argument("--cppcheck", default="cppcheck")
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
database = json.loads((args.build / "compile_commands.json").read_text())
entries = [
    entry
    for entry in database
    if any(
        pathlib.Path(entry["file"]).is_relative_to(root / directory)
        for directory in ("src", "tests")
    )
]
if not entries:
    raise SystemExit("No project translation units found")
analysis = args.build / "analysis"
analysis.mkdir(exist_ok=True)
for entry in entries:
    arguments = entry.get("arguments") or shlex.split(entry["command"])
    # Qt's GCC relocation switch has no clang equivalent; preserve all semantic flags.
    entry["arguments"] = [
        arg for arg in arguments if arg != "-mno-direct-extern-access"
    ]
    entry.pop("command", None)
filtered = analysis / "compile_commands.json"
filtered.write_text(json.dumps(entries))
for entry in entries:
    subprocess.run([args.clang_tidy, "-p", str(analysis), entry["file"]], check=True)
subprocess.run(
    [
        args.cppcheck,
        f"--project={filtered}",
        "--enable=warning,performance,portability",
        "--error-exitcode=1",
        "--inline-suppr",
        "--library=qt",
        "--suppress=missingIncludeSystem",
        "--suppress=unmatchedSuppression",
        "--check-level=exhaustive",
        f"--suppress=*:{root / 'build'}/*",
    ],
    check=True,
)
