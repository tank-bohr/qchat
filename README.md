# QChat

A public, account-free desktop chat: a C++23 Linux server, Qt 6/QML clients, Apache
Thrift 0.25.0 RPC over framed TCP with the binary protocol, and CMake builds.
No passwords, tokens, permissions, or verified identities. Anyone who can reach
the server can read and post; display names may be duplicated.

## Run

Download the artifact for your platform from a successful GitHub Actions run or
a tagged GitHub release. Linux/Windows clients contain Qt, QML resources, plugins,
Thrift, and application dependencies in one executable. macOS ships a single
`.app` inside a zip. The executable is unsigned; signing/notarization credentials
are not configured.

```sh
chmod +x qchat-server-linux-x86_64 qchat-client-linux-x86_64
./qchat-server-linux-x86_64 --bind 0.0.0.0 --port 9090
./qchat-client-linux-x86_64
```

Enter the server's IPv4/IPv6 address, port, and a display name, then join.
Ctrl+Enter sends a message. Hosts must currently be entered as numeric IP
addresses, which also prevents DNS lookup from blocking worker shutdown.
The server defaults to loopback (`127.0.0.1:9090`); `--bind` explicitly enables
remote access. SIGINT/SIGTERM stop it and interrupt connected sockets.

Messages are plain text, transmitted without TLS, and held only in memory.
There is one public room, retaining the last 1,000 messages. Restarting the server
clears history. No delivery retry happens automatically: a lost response may mean
the message was already accepted, so the client preserves the draft and reports
uncertain delivery.

## Static distribution and supported systems

| Artifact | Platform | Runtime boundary |
| --- | --- | --- |
| Server | Linux x86_64 / aarch64 | Fully static musl ELF; no interpreter or shared libraries |
| Client | Linux x86_64 | Ubuntu 24.04 or compatible glibc baseline; X11/XWayland and system graphics libraries |
| Client | macOS arm64 / x86_64 | macOS 13+; Apple system frameworks |
| Client | Windows x86_64 | Windows 10 (1809+)/11; Windows system DLLs, static MSVC runtime |

Qt, Thrift, and vendored libraries are statically linked. Desktop operating-system
libraries cannot all be eliminated, particularly on macOS. Linux graphics drivers
can load their own system libraries. There are no separately installed Qt/QML
libraries or application DLLs. CI rejects unexpected directly linked libraries
and tests the installed application. It does not claim universal Linux portability.

## Build

CMake 3.28+, Ninja, a C++23-capable compiler (GCC 13+, recent Clang/AppleClang, or
Visual Studio 2022), Python 3.11+ for tests, and the dependencies are required.
C++23 is selected because it is the latest published ISO standard listed by
the [C++ standards organization](https://isocpp.org/std/the-standard).

For a static desktop build, clone vcpkg at the exact revision in `vcpkg.json`,
bootstrap it, and set `VCPKG_ROOT` to that checkout. The workflow lists system
development packages for each OS.

```sh
cmake --preset release -DVCPKG_TARGET_TRIPLET=x64-linux -DVCPKG_HOST_TRIPLET=x64-linux
cmake --build --preset release
ctest --preset release
cmake --build build/release --target all_qmllint
cmake --install build/release --prefix out --component client
```

Use `x64-windows-static`, `arm64-osx`, or `x64-osx` on other systems and set
`-DQCHAT_BUILD_SERVER=OFF`. On macOS set `MACOSX_DEPLOYMENT_TARGET=13.0` before
configuring. Qt and Thrift builds are substantial; CI caches their binaries.
The release preset uses the overlay triplets in `cmake/triplets` to build static
dependencies in Release only. Debug, coverage, and sanitizer builds of QChat can
reuse these libraries on Linux. CI sets `VCPKG_OVERLAY_TRIPLETS` for every build,
cleans completed dependency build trees, and saves the dependency cache before
configuring the application. The first static dependency build can take a long time.

For faster development with an installed Qt 6.8+ and Thrift:

```sh
cmake --preset dev -DCMAKE_PREFIX_PATH=/path/to/dependencies
cmake --build --preset dev
ctest --preset dev
```

The `dev` preset explicitly permits shared dependencies; releases require static
Qt and Thrift. Generated Thrift files remain in the build directory.
`tools/build-thrift.sh PREFIX [WORK_DIRECTORY]` builds a pinned minimal static
Thrift compiler/runtime with a verified source checksum; Boost headers, flex,
bison, curl, CMake, and Ninja must be installed.

For the fully static server, use the Alpine/musl build in `.github/workflows/ci.yml`
with `QCHAT_BUILD_CLIENT=OFF` and `QCHAT_FULLY_STATIC_SERVER=ON`. Building with
`-static` against glibc is not the supported release recipe.

## Testing and CI

Every push to `main`, pull request, and manual workflow runs:

- Store validation, UTF-8 edge cases, byte limits, cursor pagination/expiry,
  restart reset behavior, retention, and concurrent readers/writers.
- Real TCP/Thrift integration with multiple clients, Unicode, typed errors,
  oversized frames, reconnection, and continued service after malformed input.
- Qt model invariants and retention, controller validation, two asynchronous
  clients, disconnect/reconnect, server loss, and embedded QML startup.
- Server process tests for invalid arguments, startup, and signal shutdown with
  an open connection.
- Packaging checksums/notices and rejection of incomplete or corrupted dependency sources.
- Warnings as errors, clang-format, clang-tidy, cppcheck, qmllint, ShellCheck,
  Ruff, actionlint, AddressSanitizer/UBSan, ThreadSanitizer, and an 85% application
  library line-coverage gate. Generated and third-party source is excluded from
  project lint/coverage. Server/network sanitizers run separately from GUI tests.
- Static desktop builds on all four OS/architecture combinations, fully static
  servers on two Linux architectures, dependency audits, checksums, and artifacts.

Pushing a `v*` tag publishes release assets only after all required jobs pass.
Ordinary CI runs upload artifacts without publishing a release.
Build status and downloadable artifacts are available in the repository's Actions tab.

## Protocol and limits

`protocol/chat.thrift` is the contract. `post(displayName, text)` returns a
server-assigned sequence number and timestamp. `poll(serverId, afterId, limit)`
returns up to 100 messages and a continuation cursor. A server instance ID and
`reset` flag let the client discard history after restart, cursor expiry, or
connecting to another server. Clients poll every 500 ms and drain backlog faster.

The store is protected by a mutex, validates UTF-8 and control characters, and
limits display names to 64 bytes and messages to 4,096 bytes. The network caps
frames at 1 MiB, protocol strings at 8 KiB, container lengths at 100, nesting at
16, and simultaneous connections at 128. Socket I/O has deadlines; idle server
connections expire after ten seconds. These bounds do not provide user-based
rate limiting or protection against distributed resource exhaustion.

The UI uses a dedicated network thread and renders incoming content as plain
text. Client and server history are bounded. Application errors leave the
connection usable; transport failures require an explicit reconnect.

See [THIRD_PARTY.md](THIRD_PARTY.md) for static dependency redistribution details.
