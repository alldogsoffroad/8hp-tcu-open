# Bench logs

CAN captures from the bench spare, candump `-L` format (`can0` = bus 1, TCU pins 5/6;
`can1` = bus 2, pins 3/4). Replay with the host simulator:
`TCU_SIM_CANLOG=bench_logs/<file> build/host/tcu_sim`.

| File | Content |
|---|---|
| `20261017_stock_first_power.log` | stock firmware, no vehicle; ignition switched on during the capture (first frames ≈ 15 s in), about 5 s of traffic |

These are traffic captures only. Before the repository goes public, check them for unit
identification data (the payload of `0x639` is not yet understood).
