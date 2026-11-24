# Bench logs

CAN captures from the bench spare, candump `-L` format (`can0` = bus 1, TCU pins 5/6;
`can1` = bus 2, pins 3/4). Replay with the host simulator:
`TCU_SIM_CANLOG=bench_logs/<file> build/host/tcu_sim`.

| File | Content |
|---|---|
| `20261017_stock_first_power.log` | stock firmware, no vehicle; ignition switched on during the capture (first frames ≈ 15 s in), about 5 s of traffic |
| `jl_shifter_20261124/` | a JL Wrangler shifter alone on the shifter bus, captured by `tools/bench/jl_shifter_capture.py` (its own format: host time in s, ID, DLC, data; `SENT` = frames the Teensy sent, `ESTATE` = Teensy error state). `waketcm_01_lever.log` P/R/N/D, `waketcm_03_manual.log` manual gate and taps, `after_tcm_stop_ack.log` the stop after 0x300 ends; lab record 2026-11-24 |

These are traffic captures only. Before the repository goes public, check them for unit
identification data (the payload of `0x639` is not yet understood).
