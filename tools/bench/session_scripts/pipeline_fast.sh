#!/bin/bash
# storm DOWN FIRST (original design, restored 2026-11-03): the storm's 5 s C/O
# controller-reset writes and any probe traffic both open ACM0, and pyserial
# dies on the multiple-access collision — that crash burned install cycle 2 at
# 00:33 (spec 17 §5m tooling note). Cycle 1 survived the same overlap by luck.
pkill -f "storm_dumb[12].py"; sleep 2
python3 -c "import sys; sys.path.insert(0,'/home/chadk/Documents/REA Projects/tools/bench'); import tcu_bench; tcu_bench.teensy_passive(['/dev/ttyACM0'], b'B0\r')"
echo "[pipe] $(date +%H:%M:%S) storm down (LED slow), patient probe starting"
# patient probe: 10 60 every 30 s up to ~5 min after the edge window
for i in $(seq 1 10); do
  sleep 30
  echo "[pipe] $(date +%H:%M:%S) probe try $i"
  python3 - <<'PYEOF' && break
import sys
sys.path.insert(0, "/home/chadk/Documents/REA Projects/tools/bench")
sys.path.insert(0, "/home/chadk/Documents/acdp2-re/tools")
import tcu_bench
from tcu_write import UdsCapped
class A:
    iface = "slcan"; bus1 = "/dev/ttyACM0"; bus2 = None
    teensy_mode = None; keep_rate = True
for cmd in (b"C\r", b"P0\r", b"S9\r", b"O\r"):
    tcu_bench.teensy_passive([A.bus1], cmd)
b = tcu_bench.Buses(A())
u = UdsCapped(b, 1)
r = u.request(bytes.fromhex("1060"), timeout=2.0)
b.shutdown()
sys.exit(0 if (r and r[:2] == b"\x50\x60") else 1)
PYEOF
done
echo "[pipe] $(date +%H:%M:%S) session answered (or probe window exhausted), chaining"
python3 /tmp/fastprobe.py 2>&1 | stdbuf -o0 grep -v "tx pt\|rx pt"
echo "[pipe] $(date +%H:%M:%S) DONE"
