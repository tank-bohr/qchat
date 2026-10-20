"""Fail release builds with unexpected runtime library dependencies."""

import argparse
import pathlib
import re
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument("binary", type=pathlib.Path)
parser.add_argument("--fully-static", action="store_true")
args = parser.parse_args()


def output(*command):
    return subprocess.check_output(command, text=True, errors="replace")


if sys.platform == "darwin":
    dependencies = [
        line.strip().split(" (")[0]
        for line in output("otool", "-L", str(args.binary)).splitlines()[1:]
    ]
    unexpected = [
        dep
        for dep in dependencies
        if not dep.startswith(("/System/Library/", "/usr/lib/"))
    ]
elif sys.platform == "win32":
    dependencies = re.findall(
        r"^\s+(\S+\.dll)\s*$",
        output("dumpbin", "/dependents", str(args.binary)),
        re.MULTILINE | re.IGNORECASE,
    )
    system = {
        "advapi32",
        "bcrypt",
        "crypt32",
        "comdlg32",
        "d3d11",
        "d3d12",
        "dcomp",
        "dwrite",
        "dwmapi",
        "dxgi",
        "gdi32",
        "imm32",
        "iphlpapi",
        "kernel32",
        "mpr",
        "netapi32",
        "ntdll",
        "ole32",
        "oleaut32",
        "opengl32",
        "powrprof",
        "propsys",
        "psapi",
        "runtimeobject",
        "secur32",
        "setupapi",
        "shell32",
        "shcore",
        "shlwapi",
        "user32",
        "userenv",
        "uxtheme",
        "version",
        "winmm",
        "winspool.drv",
        "ws2_32",
        "wtsapi32",
    }
    unexpected = [
        dep
        for dep in dependencies
        if dep.lower().removesuffix(".dll") not in system
        and not dep.lower().startswith(("api-ms-win-", "ext-ms-win-"))
    ]
else:
    dependencies = re.findall(
        r"\(NEEDED\).*\[(.*?)\]", output("readelf", "-d", str(args.binary))
    )
    # glibc and platform graphics/input services are provided by the desktop OS.
    system_pattern = re.compile(
        r"^(ld-linux.*|lib(c|m|dl|rt|pthread|resolv|"
        r"GL|GLX|GLdispatch|OpenGL|EGL|X11|X11-xcb|Xau|Xdmcp|Xext|"
        r"xcb[-\w]*|xkbcommon[-\w]*|SM|ICE))\.so(?:\..*)?$"
    )
    unexpected = (
        dependencies
        if args.fully_static
        else [dep for dep in dependencies if not system_pattern.fullmatch(dep)]
    )
    if args.fully_static and "INTERP" in output("readelf", "-l", str(args.binary)):
        unexpected.append("ELF program interpreter")

print(f"{args.binary}: {dependencies or 'no shared dependencies'}")
if unexpected:
    raise SystemExit(f"Unexpected shared dependencies: {unexpected}")
