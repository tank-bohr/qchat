"""Exercise the actual Linux entry point, argument handling, and signal shutdown."""

import select
import signal
import socket
import subprocess
import sys

binary = sys.argv[1]
assert (
    subprocess.run([binary, "--help"], capture_output=True, timeout=5).returncode == 0
)
for args in (
    ["--port", "0"],
    ["--port", "65536"],
    ["--port", "9x"],
    ["--port"],
    ["--bind", "invalid"],
    ["--unknown"],
):
    result = subprocess.run([binary, *args], capture_output=True, timeout=5)
    assert result.returncode == 1, (args, result)

# Retry port selection: the OS allocates a free port, but another process could win the race.

for attempt in range(5):
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    with subprocess.Popen(
        [binary, "--port", str(port)], stdout=subprocess.PIPE, stderr=subprocess.PIPE
    ) as server:
        try:
            assert select.select([server.stdout], [], [], 5)[0], (
                "No startup notification"
            )
            line = server.stdout.readline()
            if not line and server.poll() is not None:
                continue
            assert b"QChat listening" in line, line
            with socket.create_connection(("127.0.0.1", port), timeout=2):
                server.send_signal(signal.SIGTERM)
                assert server.wait(timeout=5) == 0
            break
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()
else:
    raise AssertionError("Could not start server")
