# 22 — The supervisor protocol (top-down artifact 2)

> The complete SSLB1 protocol: 14 job descriptors (10 bus B), reply grammar, the two interleaved conversations (5 ms mirror + timer-tick walk), the never-rules as stated by the stock's own code. Generated 2026-11-08.

# ARTIFACT 2 — The external supervisor protocol, unified (RSPI-B / SSLB1)

Target: Bosch 8HP TCU, P/N 1034420267, SH7254R. Scope: everything the stock firmware does
with, and knows about, the external supervisor chip on bus B chip-select SSLB1 — the complete
job list with wire format, the reply grammar field by field, the power-on state machine, and
the NEVER-RULES the stock's discipline implies (bench regressions E60m/E60n/E16-family were
violations of exactly these).

Markers: **[O]** observed in stock code/ELF/bench, **[I]** inference, **[M]** manual.
Citations: `0xADDR name` = decompile (`~/tcu_sweep/decomp/`, names from
`docs/spec/catalog/all_names.csv`); "spec 03 §x" = `docs/spec/03-spi-devices.md`.

---

## 0. VERDICT (read this first)

1. **The supervisor is a driver-family chip with switch outputs, selected SSLB1 on RSPI-B.**
   It speaks the same 16-bit `[command][data]` grammar as the four solenoid driver chips on
   RSPI-A; its reply class is `0xA8/0xAA` with **bit 11 required CLEAR** (class check
   `(rx & 0xF900) == 0xA800`, handlers 0x910EE/0x911D8/0x92D94/0x92ED4) [O].
2. **The stock's entire supervisor conversation is TEN bus-B job descriptors plus a 5 ms
   control word** (job table flash `0x409F4–0x40C74`, this document §2). Nothing else exists:
   no mode-set register job, no persist/store opcode, no hidden handshake job [O; exhaustive
   sweep, spec 20 §1.4].
3. **Commissioning is a WALK, re-run at every power-on**: level values `0x3F` (init) →
   `0x06` era (monitor state 0) → duty/token era → `0x1F` era (state 0x23), each level job
   `{84|lvl, 0A00, 1200, 0C00}` sent **cyclically** from a timer ISR with echo + commit
   verification, dwell `value*200 + 2400` time-base ticks [O; spec 20, reconciliation.md].
4. **The 5 ms control word's data byte is fully composed by code** (0x91C86): bit 2 toggle =
   the normal dialect's life sign; bits 7+4-toggle = the engaged dialect (mode `&0x11==0x11`,
   e.g. live mode 0x19); bit 0 = the enable request, sent from the FIRST monitor pass with
   the ladder running; bit 6 = power-down ONLY in mode 6 [O].
5. **The NEVER-RULES that our regressions broke** (§7): never bit-0 without the rolling
   ladder conversation (E60m = reset loop); never a level-class `84|xx` word outside the
   stock's timer-driven commissioning frame (E60n/E16 family = reset loop); never
   register-class data words on the supervisor's select outside its own descriptors
   (E42–E50 wall); never a stalled 5 ms stream under grant (E27/E28) [O bench, O code].

---

## 1. Physical layer and framing [O unless marked]

| Property | Value | Citation |
|---|---|---|
| Bus | RSPI-B, base `0xFFFFB800` | spec 03 §1 |
| Chip select | **SSLB1** (SPCMD `0xAF11`: mode 1, 16-bit, MSB first, per-frame select assert) | spec 03 §2, §4; descriptor cfg tables §2 below |
| Neighbours on bus B | SSLB0 (`0xAF01`, second system chip), SSLB2 (`0xAF20`, **mode 0**, rolling-counter chip) | spec 03 §4.3, §6.2aw |
| Not on bus B | the four driver chips + dead SSLA5 — they are RSPI-A (`0xFFFFB000`); "0x40CE4" is a bus-A five-select table; SPCMD values are peripheral-relative | spec 03 §6.2ai, §6.2aw |
| Frame | 16 bits, CPOL=0/CPHA=1, SPBR=5 → Pφ/12, select released each frame (SSLKP=0) | spec 03 §2 |
| Transport (app) | job descriptors: RAM staging buffers, A-DMAC-launched (`0x96468 spi_job_dmac_launch`, SPCR=0xF8-era cycle, vector 445 burst-end) — gapless frames, zero CPU per frame | spec 03 §6.2am–ap, 6.2aw |
| Stock live SPCR | `0x08` idle / armed-by-ISR per transaction; SPCMD registers read 0 (RAM shadow `0xFFF90630+0x7E` reloaded per transaction) | spec 03 §6.2ak, §6.2am |
| Boot-time fallback | CPU-mode polled access (SPCR=0x48) used by both boot programs every 5 ms and by the fatal path | spec 03 §4.4 |

The time base: the walk uses free-running counter `0xFFFFF220` (TCNTA, 1 MHz µs counter,
spec 02 §2) for era arithmetic, and an output-compare timer at peripheral `0xFFFFF580`
(compare `0xFFFFF5B0`, ISR ack bit0 of `0xFFFFF58C`, enable bit0 of `0xFFFFF585`, start bit0
of `0xFFFFF58E`) whose interrupt service routine is **`0x9106C`** [O disassembly; vector #224
[I], table base 0x40070 derived from vector 445 → 0x96590 at flash 0x40764].

**Correction (2026-11-11/12, sources: `~/tcu_sweep/walktmr_map_fix.md` §7; CLAUDE.md "bench mechanics" and r02g B2; applied
2026-11-18):** the walk timer is the **DCNTD00 down counter** at `0xFFFFF5B0` (not a compare register); vector
224 = its **underflow**. The register labels above are swapped: **`0xFFFFF585` = DSTRD0 (start strobe, reads
0)**, **`0xFFFFF58E` = TIERD0 bit 8 (UDEDn0, the underflow interrupt enable)**. Its rate is **1 µs per tick**
(measured against TCNTA on the bench, r02g: 1024 ticks in 1025 µs) [O]; the earlier ~9–10 MHz / 28 µs readings are
refuted.

---

## 2. The complete job table (flash 0x409F4–0x40C74) [O — dumped from the ELF]

Descriptor = 32 bytes, integrity marker `0x45B9F72D` at +0x1C:
`{u16 class (0x0100 = bus-B 5 ms cyclic, 0x0101 = W1/W2 walk jobs — **exclusive**; 0x0000 = bus-A five-select; byte 1 selects the 5-slot register queue vs the 10-slot cyclic queue, decoded in 0x937F8) [V: spec22_validation], u8 frames,
u8 selects, u32 TX-staging-RAM, u32 RX-staging-RAM, u32 select-cfg-flash, u32 status-cell,
u32 completion-handler, u32 trailing-records, u32 0x45B9F72D}` (spec 03 §4.3, §6.2e; layout
cross-checked word-by-word in this artifact's ELF dump).

Select-config tables: `0x40CA0` = {AF11} (SSLB1 only); `0x40B74` = {AF11, AF01} (SSLB1/SSLB0
alternation); `0x40B76` = {AF01} (SSLB0 only); `0x40D8A` = {AF20} (SSLB2); `0x40CE4` =
{AF31, AF21, AF11, AF01, AF51} (the bus-A five-select, frame i → SPCMD[i mod 5]). A
descriptor's `selects` byte says how many SPCMD entries get loaded — jobs 0x40C0C/0x40C30/
0x40C54 load ONE (AF11) so **both frames of each go to SSLB1** (spec 03 §6.2az).

### 2.1 Bus-B supervisor jobs (the "7-job rotation" + walk jobs + fault twin)

| # | Descr | Frames→chip | Wire words (TX) | Cadence / caller | Completion handler |
|---|---|---|---|---|---|
| J1 | `0x40BE8` | 4 → B1,B0,B1,B0 | `8E00\|ctrl, C900\|c9, 2200, 0900` (live staging `0xFFF88424`) | every 5 ms pass: `0x8EE82` → `0x91DAA` → `0x92D38` (ctrl byte and C9 byte both composed by `0x91C86`; ~~C9 byte by `0x93126`~~ — `0x93126` is the handler-completeness summarizer. *Corrected 2026-11-19 [O bytes `0x91C96–0x91CB6`] (`~/tcu_sweep/c9_bit7_decode.md` §1.1; spec22_validation item 4)*) | `0x92D94 shifter_resp_echo` |
| J2 | `0x40C0C` | 2 → B1,B1 | `9000\|en, 2400` (template @0x40C08) | ~~on letter/output-state change~~ **every 5 ms pass, in every mode** (corrected 2026-11-11, CLAUDE.md "THE STOCK'S TASK RATES": the J2 stage `0x8EDF6` stays in the 5 ms list and `0x92E80` queues J2): `0x91804/0x91810` → `0x92E80` (word from `0x91758 switch_status_build`) | `0x92ED4` |
| J3 | `0x40C54` | 2 → B1,B1 | `2800, 2E00` (template @0x40C50) | cyclic [V]: queuer `0x9305A` (`bank_tmpl_init_88444`, called by `0x91ACA`; template `0x40C50`→staging `0xFFF88444`) | `0x93096` |
| J4 | `0x40C74` | 1 → SSLB2 (mode 0) | `00FF` (template @0x40D8C) | cyclic: `0x93198 bank_tmpl_init_885e4` | `0x931D4` |
| J5 | `0x40B80` | 4 → B1,B0,B1,B0 | `0600, 0300, 0000, 0000` (template @0x40B78) | first bus-B job at init (`0x91DD2`), then every 5 ms pass: `0x8EE82` → `0x91E12` → `0x92A84` | `0x92AC0` |
| J6 | `0x40BA0` | 5 → B1,B0,B1,B0,B1 | `1800, 0C00, 1E00, 0600, 1400` (template @0x40D78) | every 5 ms pass: `0x91DAA` → `0x92B82` | `0x92BBE` |
| J7 | `0x40BC0` | 1 → SSLB0 | `0000` (live staging `0xFFF885E0`) | ~~every rotation [I]: no queuer found ("built inline"); live staging present every pass (spec 03 §6.2m)~~ **never sent by the stock** (corrected 2026-11-11, below the table) | `0x92CC8` |
| W1 | `0x409F4` | 7 → B1×7 (cfg 0x40CA0) | `8800\|t0, 8800\|t1, 8800\|t2, 8800\|t3, 1200, 0C00, 0A00` (template @0x40CA6; 8th flash word `0C01` @0x40CB4 beyond the 7-frame copy) | the DUTY era: timer ISR `0x9106C` → `0x6C1BA park_pwm_service` (job-mode 0) → builder `0x91086` | `0x910EE serial_resp_handler7` |
| W2 | `0x40A14` | 4 → B1×4 (cfg 0x40CA0) | `8400\|lvl, 0A00, 1200, 0C00` (template @0x409EC) | the LEVEL eras: same ISR (job-mode 7) → `0x91190(level)`; **one in flight** (check-free `0x93B8E` inside both builders) | `0x911D8 serial_resp_handler4` |

**Corrections to the table (2026-11-11/12, sources: CLAUDE.md "ADDENDUM (2026-11-11 night)" and r02g block S4;
applied 2026-11-18):**
- **The stock never sends J7** [O, three lines]: descriptor `0x40BC0` has no literal anywhere in flash; the stock
  driving RAM dump holds J7's TX/RX staging `0xFFF885E0..E3 = 0000 0000` while J4's neighbour is live; the
  handler-completeness check `0x93126 → 0x931F8(&0x90973, …, mask 7)` expects bits 0–2 only (J7's handler
  `0x92CC8` owns bit 3; dump `0x90973` = 07). §4.3 below describes a handler that never runs.
- **Per-pass order and queues [O]:** the queue engine `0x937F8` keeps per-bus normal (10) and priority (5) rings,
  selected by descriptor byte 1; the dispatcher `0x93902` drains priority first. F (`0x40C30`) is priority 0;
  the only priority-1 bus-B descriptors are the walk W2 (`0x40A14`) and W1 (`0x409F4`). **J3 is queued before J4**
  (`0x91C12`/`0x91C16`). Bench r02g (stock order reproduced): J3, J4, J2, F, J6, J1, J5, F2 per pass, F2 = the F
  re-fire on 0x14 bits 5–7.
- **Handler bit map [O]:** A8 triple `0x907C8/C9/CA` (J5 1, J1 2, J6 4, J2 0x10, F 0x20, J3 0x40); AA triple
  `0x90973/74/75` (J5 1, J1+C9 2, J6 4); summarizer `0x93126`/`0x931F8` with per-round clear into
  `0x90971/72`. J1's C9 echo compares rx[3] with the C9 data byte `0xFFF88427` [O flash `0x92DD6/0x92DDE`].
| F  | `0x40C30` | 2 → B1,B1 | `9000\|en, 2400` (template @0x40C2C) | fault path only: `0x92F7C` (queued by `0x92BBE` on 0x14 fault bits) | `0x92FD0` |

### 2.2 Bus-A driver jobs (same table; the supervisor NEVER sees these) [O]

| Descr | Frames | Wire words (template) | Handler | Sender |
|---|---|---|---|---|
| `0x40A98` | 10 → 5 chips | `0600×5, 0000×5` (ID read; reply must start `0xA2`) | `0x921E0` | queued every 5 ms pass by `0x921A0` |
| `0x40AB8` | 15 | `18,1E,28` per chip (diagnostic read) | `0x92298` | 0x9225C-class senders |
| `0x40B28` | 40 | writes `8E,90,96,88`, reads `28,30,2E,36` per chip (config; live rows `8E←9F, 90←21, 96←55, 88←{1A,1A,1A,12,15}`) | `0x923BC` | runtime state-sync, not boot one-shot (spec 03 §6.2m/e) |
| `0x40B48` | 45 | cyclic `88,A0,E8,A6,F0,60,C0,66,D8` rows (setpoints/duty, 2×11-bit per chip in E8/F0) | `0x92530` | `0x924B0` from the current-control loop, 10 ms |

**The `0x8E9F/0x9021` rows in the driving dump are bus-A driver registers** (0x8E9F = the
driver 1 kHz PWM divider constant), NOT supervisor words — the root error of the E40–E53 arc
(spec 03 §6.2aw).

### 2.3 The engine discipline [O]

- Queue = `0x937F8 spi_job_queue` (10 cyclic + 5 register slots, integrity pairs; errors
  0x11–0x16 via event 0x253). Status cells `0xFFF908xx`; use counters at `0xFFF94218+2*id`.
- **Check-free before every queue** (`0x93B8E`): a busy job is skipped, never stacked —
  one job per descriptor in flight at any time. Both walk builders (`0x91086`, `0x91190`)
  call it first and abort silently if the previous job hasn't completed.
- Launch = `0x96468` (RSPI+DMAC program, SPCR=0xF8); burst end services at vector 445
  (`0x96590`-class ISR with clock-gate acknowledge at `0xFFFE6030`, resbank context —
  spec 03 §6.2am–ao).
- The walk's cadence driver: timer ISR `0x9106C` = `timing_enter_ff4(0x68F98)` → ack
  (bclr bit0 @0xFFFFF58C) → `0x6C1BA park_pwm_service` → `timing_accum_ff4(0x68FA2)` →
  resbank/rte. The service first feeds the internal watchdog (`0x920C6 wdt_service_magic`,
  WTCNT 0x5ADB) then sends the current walk job (duty or level by `0xFFF93B9B`).
- 5 ms task order (`0x8EE82`, pool 0x8EFA0-0x8EFEC): timing → `0x91C86` (compose word) →
  `0x91DAA` (= `0x93126` C9 summarize + `0x92B82` queue J6 + `0x92D38` arm J1) → `0x91E12`
  → `0x92A84` (queue J5) → `0x90ACC` → `0x921A0` (kick; also queues bus-A ID 0x40A98).

---

## 3. The 5 ms control word (job J1) — bit-by-bit law of the 0x8E data byte [O: 0x91C86]

Composed every 5 ms into `0xFFF907D4` (mirrors the boot programs' direct writes, spec 03
§4.4). The SSLB0 `0xC9` byte (`0xFFF9097B`) is composed in the same pass: base
`0xFFF90957 | 0xFFF90962&0xF0`, bit 4 set while shutdown self-test flag `0xFFF9094D==0`
(set), bit 7 = (`0xFFF9095A` ≠ 0). *Corrected 2026-11-19 [O bytes `0x91CA6–0x91CB6` `tst; nott; bst.b #7`; O bench r04g] (`~/tcu_sweep/c9_bit7_decode.md` §0–§1.4; CLAUDE.md r04g BENCH RESULT) — was: "bit 7 from variant cell `0xFFF9095A`". `0x9095A` is not a variant constant: its one writer `0x8CEBC` (inside the Park-signal law `0x8CE48`, called from the pin-cut entry `0x86B0C`) sets it 1 when the range state D712 = 0, i.e. lever/range R or D (or P held with the lever at R/D), so **the stock sends C980 in every R/D drive** and C900 in P/N/no lever; in mode 0 it is frozen at the init one-shot's value. C9 is an SSLB0 register; the J1 rx[3] echo must return the written byte (`AA80` under C980). Bench r04g: C980 in R/D accepted, echo passing, 0x14 C7 / gate 1 unchanged. [I] SSLB0 uses bit 7 as the MCU's veto on the Park-signal line.* **Note (2026-11-12, source: `~/tcu_sweep/c9_bit4_decode.md`, key claims re-checked
against the bytes):** in the stock, bit 4 is never set in normal running — the shutdown self-test phase machine
`0x86916` that drives `0x9094D` is dead (its enable `0x7C6AE` is `mov #0; rtv`; its start `0x868F0` is gated
by the constant-1 `0x132334`), and the only other `0x9094D = 0` writer, `0x93248`, is in the mode-5 (shutdown)
list. C9 stays `C900` *in P/N (bit 7 gives C980 in R/D — see the 2026-11-19 correction above)*; J1's C9 word is composed by `0x91C86` (spec22_validation).

| Bit | Meaning | Code (0x91C86) |
|---|---|---|
| 0 | **enable request** = `0xFFF90953`. The request is armed once per boot on the monitor's FIRST pass (`0x873F0`: one-shot `0x6C612(1)` while level<7), and if `0xFFF9094A != 1` the builder FORCES the request cell to 1 every pass | `bVar2 \|= 0xFFF90953` |
| 1 | from `0xFFF90964` bit 1 (the 0x90-register composer's cell) | `(0xFFF90964 & 2)` |
| 2 | **normal dialect life sign**: complement of the echo's bit 2 (`0xFFF907D5 & 4`) — toggles every pass because the echo mirrors the word. In the engaged dialect: held 0 (mode 0x19) or 0 (mode 0x31) | normal branch `&0x67\|4` / `&0x63` |
| 3 | always cleared in both dialects | `&0xF7` / `&0x67` |
| 4 | **engaged dialect life sign**: alternates every pass when `(mode 0xFFF90951 & 0x11) == 0x11`; cleared in normal | toggle in dialect branch |
| 5 | always cleared in the composed paths (the echo's bit 5 is the supervisor's OWN rail telemetry — never sent) | `&0x67/0x63/0x7B` |
| 6 | **power-down**: set only when mode byte `0xFFF90951 == 6` (also latches `0xFFF800EC=1`) | mode-6 branch |
| 7 | **engaged-dialect marker**: set when `(mode & 0x11)==0x11 && !(mode & 0x20)` — live mode 0x19 ⇒ bit 7 SET. Never set in the normal dialect | dialect branch `\|0x80` |

Consequences (live-stock verified): engaged steady word = `0x8E91/0x8E81` (bit7 + bit4
toggle + bit0) with echo `0xAAB1` — exactly the stock's driving-dump shadow pair
(spec 03 §6.2p, §6.2ak). Normal/boot word = `0x8E04/0x8E00` (+bit0 once requested). The
word's bit-0 **edge** routes through `0x920A4 state_9096a_update` → WDT pair (interval vs
reset mode) only when the echo confirms (the A-law: no echo bit 0 ⇒ no WDT switch).

The enable/disable setter `0x6C612 park_store_request_state`: param 0 → boot-handover
bookkeeping (`0x72EB6`, the 0xA5-magic cells); if check `0x68BE0` passes → `0xFFF9094A =
(param==0)`, `0xFFF90953 = param`.

---

## 4. Reply grammar — job by job, bit by bit [O: the completion handlers]

General frame grammar: reply word k answers command word k (full duplex). Header classes:
- **SSLB1 (supervisor)**: `(rx & 0xF900) == 0xA800` — high nibble 0xA, bit 12 = 0, **bit 11
  = 0 required** (`0xABxx` FAILS the stock's own class check), bit 9..8 free.
- **SSLB0**: `(rx & 0xFE00) == 0xAA00` — `AAxx`/`ABxx` both pass (bit 8 free).

Every handler: on state byte != '@' (job never ran) → error bits + per-job error counter
`0xFFF94218[2*id]`; on header failure → the job's error cell (`0xFFF907C9`/
`0xFFF90974`) plus `0xFFF907CA/0xFFF90975` echo-fail cells; on success → `0xFFF907C8`/
`0xFFF90973` OK cells. These triple cells are condensed each 5 ms by `0x931F8` via
`0x93126`.

### 4.1 J5 ID job `[0600, 0300, 0000, 0000]` — handler `0x92AC0 shifter_resp_4w`

| rx word | chip | check | harvest |
|---|---|---|---|
| rx[0] (06→B1) | SSLB1 | A800-class | low byte → `0xFFF907D0` |
| rx[1] (03→B0) | SSLB0 | AA00-class | low byte → `0xFFF90978` |
| rx[2] (00→B1) | SSLB1 | A800-class | low byte → `0xFFF907CF` |
| rx[3] (00→B0) | SSLB0 | AA00-class | low byte → `0xFFF90977` |

Bench live values: SSLB1 `0x06`→`0x14`, `0x00`→`0xBC`; SSLB0 `0x06`→`0xD0`, `0x03`→`0x42`
(spec 03 §4.4 register map).

### 4.2 J6 status job `[1800, 0C00, 1E00, 0600, 1400]` — handler `0x92BBE shifter_resp_5w`

| rx word | chip | harvest / consumption |
|---|---|---|
| rx[0] (18→B1) | SSLB1 | low byte → `0xFFF907D2` (the rolling challenge-selector area, spec 03 §6.2p) |
| rx[1] (0C→B0) | SSLB0 | low byte → `0xFFF90979`; **`0xFFF90958 = (rx[1] & 0xF) ^ 0xF`** — the inverted 4-bit status nibble |
| rx[2] (1E→B1) | SSLB1 | low byte → `0xFFF907D3`; bits 4/5 consumed by `0x91ACA` as `0xFFF9095E` ~~direction~~ = (bit 5 << 1) \| bit 4, a whole-byte store — **the supervisor's two over-temperature comparator flags** (read by the die-temperature producer `0x8DB5A`). *Corrected 2026-11-19 [O store; I meaning] (`~/tcu_sweep/diag_temp_inputs_decode.md` §1.1, §6 item 4; `~/tcu_sweep/temp_stage2a_port.md` §5 item 3)* |
| rx[3] (06→B0) | SSLB0 | low byte → `0xFFF9097A` |
| rx[4] (14→B1) | SSLB1 | low byte → **`0xFFF907D1`**; `0xFFF90959 = rx[4] & 2`, `0xFFF90956 = rx[4] & 0x10`; **`if (0xFFF907D1 & 0xE0) → 0x92F7C`** — bits 5–7 of the 0x14 register are per-switch-output FAULT bits; reading any queues the fault-path enables job F (`0x40C30`) |

The 0x14 fault bits are the same bits `0x9181C switch_states_copy` expands into the
per-output booleans `0xFFF90947[]`, and they feed the fault mask `~(status & 0xE0)` applied
to the 0x90 word (§4.4). Bench: our SSLB1 reads `0x14 → 0xD5` = bits 6/7 faulted =
**outputs 1 and 2** (unpopulated on the bench); **bit 5 clear = output 0 (the rail)
unfaulted** (spec 03 §6.2az).

### 4.3 J7 SSLB0 nibble poll `[0000]` — handler `0x92CC8 shifter_resp_1w`

*Corrected 2026-11-11: the stock never queues J7 (§2.1 correction); this handler is dead code in the stock.
`0xFFF90958` is nevertheless live, written from J6: `0x90958 = (J6 rx[1] & 0x0F) ^ 0x0F` (CLAUDE.md park-sensor
blocks); the park-sensor contacts appear there (bit 2 = AN5 active, bit 3 = AN6 active; bench 2026-11-14,
CLAUDE.md "PARK HALL SENSOR — MAGNET TEST").*

rx[0]: AA00-class check → low byte → `0xFFF90979`; `0xFFF90958 = (rx & 0xF) ^ 0xF`. The
stock tolerates this job's header check failing (driving-dump reply was `0x0000`; our bench
SSLB0 answers `0xBB`-data) — error bits only, no fault [O spec 03 §6.2m].

### 4.4 J1 control job `[8E00|ctrl, C900|c9, 2200, 0900]` — handler `0x92D94 shifter_resp_echo`

| rx word | chip | check | consumption |
|---|---|---|---|
| rx[0] (8E write→B1) | SSLB1 | A800-class | fail → `0xFFF907C9 \|= 2` |
| rx[1] (C9 write→B0) | SSLB0 | AA00-class (reply to the write) | class only |
| rx[2] (22 read→B1) | SSLB1 | A800-class | **`0xFFF907D5 = rx[2].lo` — THE ECHO CELL**; echo test `(rx[2] ^ 0xFFF88424) & 0xDF == 0` vs the staged 0x8E data byte → `0xFFF907C8\|=2` OK / `0xFFF907CA\|=2` fail |
| rx[3] (09 read→B0) | SSLB0 | AA00-class; **C9 echo test: `rx[3].lo == 0xFFF88427`** (the staged C9 data byte, compare at byte offset +7 of the rx buffer; *corrected 2026-11-19 [O `movu.b @(3,r13)`, r13 = 0x88424, bytes `0x92DDA/0x92DDE`] (`~/tcu_sweep/c9_bit7_decode.md` §1.4, §5) — was `0xFFF88426`, the word base*) → `0xFFF90973\|=2` OK / `0xFFF90975\|=2` fail | SSLB0's 09 register mirrors the C9 word back on the FOLLOWING read |

**The 0x22 echo law [O]**: the reply's low byte mirrors the sent 0x8E control byte, PLUS
the supervisor's own bits. The stock's own match mask is `0xDF` — **bit 5 (the rail/VS
telemetry) is excluded from the match**, bit 0 (the grant) is INCLUDED. Downstream:
`0x91ACA serial_rx_process` (each 5 ms) computes **`0xFFF90954 = (0xFFF907D5 & 1) != 0`**
— the confirmation cell the output gate checks; it also latches `0xFFF84A46` (=
`0xFFF907D1` bit 0), the SSLB0 status bits, and the 4-sample average of the 10-bit
telemetry `0xFFF907CC` into `0xFFF908CA`.

*Addition (2026-11-19, sources: `~/tcu_sweep/temp_stage2a_port.md` §5 items 1–2 [O bytes, gate re-checked]; `~/tcu_sweep/c9_bit7_decode.md` §5 [O]; CLAUDE.md r04j block):* this condensing block of `0x91ACA` (grant `0x90954`, handover `0x84A46`/`0x84A44/45`, the J3 average, `0x9095D/E`, the SSLB2 copy) runs only while the gate **`0xFFF9095B`** ≠ 0. The batch `0x91DAA`'s tail sets it 0 → 1 (after J6/J1 are queued); a pass that reads 1 reseeds the 4-slot average and stores 0x0F. So the first condensed average is a real J3 sample, and the grant/handover first update one reply-processor pass after the first batch. A refused C9 echo sets summary `0x90971` bit 2 → `0x9094E` = 0x2E → `0x7E41A` (3-pass debounce) → `0x94FD2` bit 2 → `0x7DE7E` (an error event and a DSM report, **no reset** — the fatal path needs bits 5–15); cal `0x1919E2` holds reaction-**disable** bits (0 on this unit = all reactions on).

### 4.5 J2/J-F enables jobs `[9000|en, 2400]` — handlers `0x92ED4` / `0x92FD0`

Both frames to SSLB1 (single SPCMD AF11). A800-class on both words; **`0xFFF907D7 =
rx[1].lo` — the 0x24 switch-state echo**; certification cross-check
`(rx[1] ^ staged_90_word) & 0x15 == 0` — i.e. bits 0, 2, 4 of the reply data must match the
written 0x90 data (the letter bits 5–7 are NOT in the mask). `0x24 = 0xAAFF` is the chip's
REFUSAL byte; the engaged stock reads `0xAA20` = slot-0 letter `'c'` acknowledged (spec 03
§6.2n, §6.2ax). OK/echo-fail bits 0x10 (J2) / 0x20 (F) in `0xFFF907C8/C9/CA`.

The 0x90 data byte itself is composed by `0x91758 switch_status_build`:
`0xFFF90963 & 0x1D` (flash const `0x1918EF` = 0x00) | letter bits (slots 0/1/2 → bits
5/6/7 via table `0x1918E8`) | calib `0x1918EB<<5` | bit 3 = (`0xFFF90952`==1); the starter
`0x92E80` merges `0xFFF907D6 & ~(0xFFF907D1 & 0xE0)` — **outputs whose 0x14 fault bits are
set get their letter bits cleared before sending**.

### 4.6 J3 telemetry job `[2800, 2E00]` — handler `0x93096 shifter_resp_pos_2w`

Both frames to SSLB1. A800-class; on success: `0xFFF907CE = (rx[0] & 0x40) != 0` (valid
flag; = 2 on header fail), **10-bit value = `rx[1] & 0x3F | (rx[0] & 0xF) << 6` →
`0xFFF907CC`** (live stock: 373–380, closed-loop-walking). OK bit 0x40.

*Correction (2026-11-19, source: `~/tcu_sweep/diag_temp_inputs_decode.md` §1.2, §6 item 3) [O conversion, I physical meaning]:* the 10-bit value is **the supervisor's die temperature**: `0x91ACA` averages it into `0x908CA`, and `0x8DB5A` converts T = −738 + 3.347·N (1/16 °C, variant `0x90966` = 0) → `0x93ABA` (status `0x9415E`), one input of the ECU-temperature selection `0x88BD0`. Dump: N ≈ 381 → 33.6 °C; bench r04f: N 433–434 → 44.5 °C. The "closed-loop-walking" reading is superseded.

### 4.7 J4 SSLB2 job `[00FF]` — handler `0x931D4 serial_resp_store_908ee`

No header check at all: rx[0] → `0xFFF908EE`; `0x91ACA` copies it to `0xFFF908CC` and
re-arms the cell to `0x8001`. ~~SSLB2 answers everything with a rolling counter
(`0x0ECx..0x0F6x`, +4/job)~~ — the "fourth reply class `0x0Cxx`" of spec 03 §4.4 [O bench].

*Correction (2026-11-19, source: `~/tcu_sweep/diag_temp_inputs_decode.md` §1.3, §6 items 1–2) [O conversion, I strong device class]:* SSLB2 is **a board temperature sensor** (16-bit SPI digital sensor class): raw in 1/128 °C, low 3 bits read as ones (checked with mask 7), **`0xFF03` = its not-ready word** (cal `0x19171E`). The producer `0x8DD7C` stores raw >> 3 → `0x9393E` (status `0x94100`, byte `0x94101`; 600 ms hold). The "rolling counter" was a slowly rising temperature (0x0ECx ≈ 29.5 °C). The `0D8B/0E37/0D57` J4 replies on warm boots (r02k–r03q) were the board temperature (26.7–28.4 °C), not a disturbed chip. It is the first-priority source of the selected ECU temperature `0x9393A`.

### 4.8 W2 level job `[8400|lvl, 0A00, 1200, 0C00]` — handler `0x911D8 serial_resp_handler4`

A800-class on all 4 words. Harvest:
- **`0xFFF9054F = rx[1].lo`** — the 0x0A **commanded-level echo** (bench: levels echo
  exactly, 6→`AA06`, 7→`AA07`, 0x1F→`AA1F`);
- **`0xFFF90551 = rx[2].lo`** — the **0x1200 commit status**;
- **`0xFFF90550 = rx[3].lo`** — the **0x0C feedback byte**;
- freshness flag `0xFFF90540 |= 2`; exchange counter `0xFFF9054C`++;

Consumption (`0x6C1EA`, §6): `selector = 0x90550 & 0xF → 0xFFF93BA6`;
`achieved level = (0x90550 >> 4) & 7 → 0xFFF93BA1`; commit check `(0x90551 & 5) ==
0xFFF93BA5` (phase mask: 0 = settling, 1 = duty-climb re-target, 4 = re-target — set by
`0x6C0BA`=0 / `0x6BF9C`=1 / the 0x1E-context path); echo check `staged_value == 0x9054F`.

Bench commit semantics: bit 0 = challenge rotation held (1) vs advanced (0); a hand-fed
commit answers `0xAAC2` — **bit 1 set is the bit the stock's live 0x14 carries and ours
lacked** (spec 03 §6.2ba); the 0x0C01 trailing read answers in the same family.

### 4.9 W1 duty/token job `[8800|t0..t3, 1200, 0C00, 0A00]` — handler `0x910EE serial_resp_handler7`

A800-class on all 7 words. Harvest:
- `0xFFF90550 = rx[5].lo` (0x0C feedback: challenge selector + achieved level),
- `0xFFF90551 = rx[4].lo` (0x1200 commit status),
- `0xFFF9054F = rx[6].lo` (0x0A level echo),
- `0xFFF90548` holds the last token sent (builder), `0xFFF9054D` exchange counter,
- clamps `0xFFF800ED & 0x3F` (when > 0x3F and `0xFFF9095B > 0xE`), freshness `0xFFF90540|=1`.

Note: the stock's class check requires bit 11 CLEAR — our bench `0xABFF` 0x2E-era replies
would register as header errors in the stock's own grammar (§4 preamble).

---

## 5. The challenge/token system (the duty job's payload) [O: spec 03 §6.2d-era decode]

The "duty value" is a **rolling challenge-response token**, not a physical duty:

- The challenge = the 0x0C feedback's low nibble (`0xFFF93BA6`), rotating under the chip's
  control (observed residues marching 0–3 with +4 inside each; 16-entry table).
- Token = `~(xor_table[sel] ^ 0xAAAA ^ 0xB32F8A6A)`, table at flash `0x4526C` (16 entries,
  pointer cell `0x7F12C`); constant's two flash initializers agree (`0x7F424^0xAAAA` =
  `0x8D20C`). Wire form: the four token bytes OR'd onto the four `0x88` template words.
- The stock computes it in `0x7EE8A monitor_integrity_sweep` (the same pass that checksums
  the dual-rail RAM — the "calibration walker"), stages it at `0xFFF92414`, and the duty
  era's timer service sends it every tick (`0x6C1BA` job-mode 0 → `0x91086`).
- **Duty-era context cycling** (`0x6BF9C` + `0x8CEDC`): while the achieved level is 0 the
  duty arms cycle contexts 0 → 7 → 0x19, and **context 0x19 sends the COMPLEMENT of the
  token** (`~token`); once level ≠ 0 the steady context is 0x1E (plain token) [O spec 03
  §6.2d-era; contexts staged at `0xFFF94FE7/0xFFF94E5F`].
- Repetition is the norm: 249–587 consecutive correct token rounds observed live with zero
  bad replies; the challenge rotating is the chip's MISS/retry behaviour, not a requirement
  (spec 03 token-exchange section).

---

## 6. The state machine the stock drives from power-on [O]

### 6.1 Init (before any exchange)

- `0x87732 fault_manager_init` (every boot) → `0x6C12C park_pwm_init`: commanded value
  `0xFFF94FE8 = 0x3F` (diverse pair `0xFFF94E60 = 0xC0`), period `0xFFF93848 = 0x3138`
  (= 0x3F×200), expected AND reported level cells (`0xFFF93BA2`, `0xFFF93BA1`) = **7**,
  era counter 1, time base = now; guard windows derived by `0x6C594`. Boot flag
  `0xFFF9500E = 1`; walk state `0xFFF93BA3 = 0` (RAM-cleared).
- `0x91DD2 serial_rx_init`: enable `0xFFF90953 = 0`, disable flag `0xFFF9094A = 1`
  (outputs disabled at hand-in, spec 03 §4.4), SSLB2 sentinel `0x8000`, and **the first
  bus-B job: J5 ID `[0600,0300,0000,0000]`** staged and queued.
- Boot-software era (before the app): both boot programs service the supervisor directly
  every 5 ms — `0x8E00|data` with **bit 2 toggled** (normal dialect), one-shot
  `0x8E01 + 11×0x8401` in the MAT-flash state, bit 6 in state 6 (bootloader `0x1D0B4`,
  boot manager `0x30E40`, spec 03 §4.4). The rail is held from T+0 by this gapless service
  (spec 03 §6.2aw timeline).

### 6.2 The walk machinery (per monitor pass and per timer tick)

Monitor task `0x873F0 fault_manager_poll` (period [I], cooperative OS `0x67A68`/
`0x67B16`, registrar pool 0x7EE38): per pass → RAM checks (`0x7F1A6`, `0x7EE8A`) →
**walk task `0x6C63E park_pwm_task`** → the outer FSM `0xFFF95010`.

Walk task `0x6C63E` (state cell `0xFFF93BA3`):
- **state 7** ("sampling"): waits until the timer deadline (`(FRT − 0xFFF9240C) ≥
  0xFFF92410`), then runs `0x6C1EA`; if the reply counter is stale → stall counter
  `0xFFF93B9E`++, stop the timer (`0x91058`), → **state 0x19**.
- **state 0x19** (era walk): era stepper `0x6C542` + reply processor `0x6C1EA`.
- **state 0**: era stepper only (waiting for a new staging ping-pong to complete).
- Tail: if the staging ping-pong (`0x6C742`) completes → state 7; else state 0.
- Output gate (every pass): `level ≥ 6 AND 0xFFF90954` (echo bit 0) → master gate
  `0xFFF800E8 = 1` (+ event bits); at level 7 twice → the certification event
  (`0xFFF94F84 |= 0x2000`).

Era stepper `0x6C542`: when `(FRT − 0xFFF9240C) ≥ 0xFFF93848 + 0x960` → advance the era
base, bump the **expected** level toward 7 (reported := expected at `0x6C0BA`-class bumps).

Reply processor `0x6C1EA park_monitor_feedback` (the ONLY harvester of the level nibble in
the image): watches the reply-freshness counter (duty counter `0xFFF9054D` in job-mode 0 /
level counter `0xFFF9054C` in mode 7 vs last-seen `0xFFF93B48/49`); on a fresh valid reply
→ harvest selector+level (§4.8), max-latch `0xFFF94997`, then THREE checks: expected vs
reported (mismatch → flag `0xFFF93BA7 |= 8/0x10`, expected := reported, attention cell
`0xFFF93BA4 = 7`), commit `(0x90551 & 5) == 0xFFF93BA5` (mismatch → retry counter
`0xFFF93BA0`++, flags), echo `staged == 0x9054F` (mismatch → flags `0x20/0x40`,
attention = 7). Diversity-pair corruption anywhere → fault 0x226 via `0x6865C` (with
`0xFFF90998` detail codes 0x975EE/F0/E1/E2/D2). Attention 7 → `0x8CEDC` sub-state 0x2A →
`0x6C720 park_pwm_req_restart` (resend level at bumped value) — the bump-and-resend loop
that syncs on the **0x0A echo**, not the nibble (spec 03 level-exchange note).

Staging ping-pong `0x6C742 park_pwm_confirm_echo` + `0x6C48C park_pwm_learn_value(value,
complement, retries)`: value is the period (level×200; range-checked < 0x3139, else
return 2 = invalid); level = value/200. Cold (walk idle) → stage the diverse pair,
sub-state 0x19, program the timer, walk state 7. Warm → stage the pending slot
`0xFFF94FE9` (sub-state 7); the next pass promotes it (`0x6BD48 lib_diverse_write_2b_locked`)
and the pass after confirms: echo must equal the staged byte (retry counter
`0xFFF93B9D`, re-program `0x6C0BA` per retry, give-up flag `0x93BA7 |= 4`), then
**commits the period `0xFFF93848 = level × 200`**.

Timer programmer `0x6C0BA park_pwm_program_timer` (level mode): job-mode `0xFFF93B9B = 7`;
guard-window logic bumps expected +1/+2 (caps 7); deadline `0xFFF92410 = 0xFFF93846 +
elapsed`; **commit-phase mask `0xFFF93BA5 = 0`**; output-compare `0xFFFFF5B0 =
(max(period−1−elapsed, 5)) << 8`; TIER bit0, TSR ack, start bit0. Duty twin `0x6BF9C`:
job-mode 0, context-aware expected bumps, **phase mask 1** (context ≠ 0x1E/0x19) or
**0** (context 0x1E), drag-down `expected−−` when level > 6.

### 6.3 The outer FSM (0xFFF95010) — the era sequence

| State | Entry | Action | Exit |
|---|---|---|---|
| **0** | boot flag `0xFFF9500E` set at init / re-armed on entering {0, 0x12, 0x23} (`0x87766`) | `0x8D136 mode_req_set(5)` (level target ≥ 5) + **`0x6C48C(0x4B0, 0xFB4F, 2)` = the 0x06 era** (0x4B0 = 6×200) | → **8** |
| **8** | from 0 | `0x8D020 mode_transition_supervise` every pass: budget `0xFFF9417D` countdown until target reached; not-reached → walk restart (`0x6C720`), reached → `0x8CEDC mode_seq_step` advances | → 0x12 [I: via the system-mode chain 0x85160 → `0x87766 fault_state_write`] — **corrected 2026-11-12: 0x85160 calls `0x87766(0x23)` on the ladder's 0xAA, so 8 → 0x23 directly** (see below) |
| **0x12** | from 8 | boot flag → `mode_req_set(5)` again | → 0x23 — **corrected 2026-11-12: no exit of its own; reached only on a budget-failed ladder (sub 0xF1)** |
| **0x23** | from 0x12 | boot flag → **`mode_req_set(0)`** (drag the level to 0); `0x8D020` now arms the **DUTY era** (`0x7EE8A` token → `0x6C802(token, 0x1E)` — context cycling §5); `0x8D1BA` timeout → 0x34; **`0x8D108(0)` = "reported level == 0" AND `0x6C48C(0x1838, 0xE7C7, 2)` = the 0x1F era** (0x1838 = 0x1F×200) | → 0x45 |
| **0x45** | from 0x23 | ~~**proceed gate: level ≥ 5** (warm chip passes instantly; cold chip WAITS — `0x8CEDC` while < 5)~~ **'E' (corrected 2026-11-12): level ≥ 5 → the 0x34 path (a fault path: `0x94F84 \|= 0x800`); else the 'E' walk rule `0x8CEDC`** | → 0x34 only on level ≥ 5 |
| **0x34** | from 0x45 / timeout | `0x8799C fault_state_advance`: if prev = 0x23 and level == 0 → re-request the 0x1F era; else steady supervision | → steady |
| other | — | event bit, return to previous state `0xFFF93B93` | — |

While the walk is active (`0xFFF93BA3 != 0`) the outer FSM is PARKED (0x873F0 goto) [O].

**Correction (2026-11-12, source: `~/tcu_sweep/ladder_blockers_decode.md` P3, bytes `0x874F2–0x876B0`; applied 2026-11-18):** the FSM
runs in `0x873F0`, which executes **every 2.5 ms in every OS mode** from the first OS tick (CLAUDE.md "THE STOCK'S
TASK RATES"). `0x87766(t)` is called only by the system-mode sequencer `0x85160`: **0xAA (ladder complete) →
0x23**; a ladder failure with no retries left → **0x12 if sub == 0xF1 (budget), else 0x23**. State 0x23: entry →
`0x8D136(0)`; **budget `0x8D1BA()` == 1 → the 0x34 path**; else if `0x8D108(0)` (reported == 0) and
`0x6C48C(0x1838, 0xE7C7, 2)` == 0 → **'E'** (0x93B93 = 0x23, no `0x8D020` that pass); otherwise `0x8D020`. The
'E' rule `0x8CEDC` walks token contexts 0 → 7 → 0x19 at level 0 (the caller inverts the token for 0x19), resends
(0x2A) on attention 7, and uses context 0x1E above level 0. **Stock opening: 8 → 0x23 → 'E'** (dump 0x95010 = 'E',
0x93B93 = 0x23); 0x34 only on level faults. The 'E' state was reproduced on the bench under the ported law
(r02t–r02x, CLAUDE.md).

Dwell arithmetic: era length = `value×200 + 2400` time-base units (0x06 era: 3600; 0x1F
era: 8600). Spec 03 §6.2bd reads these as ~3.6 s / ~8.6 s (stock commissioning ≈ 5.6–15 s
end to end, spec 03 §6.2p/§6.2i); the time base unit (TCNTA µs vs timer-H ms vs the <<8
compare scaling) is [I] — the formula and the sequencing are [O].

**Correction (2026-11-11, source: CLAUDE.md r02f block "C2's 3.6 s dwell was a units error"; applied 2026-11-18):**
the era arithmetic runs on TCNTA, a **1 MHz µs counter** (spec 02 §2), so `0x4B0 + 0x960` = **3600 µs** and
`0x1838 + 0x960` = **8600 µs** — the era's *cycle*, not a multi-second dwell [O units]. The "~3.6 s / ~8.6 s" reading above is refuted.

### 6.4 Certification and steady state

- Output gate `0xFFF800E8`: **level ≥ 6 AND 0x22-echo bit 0** (`0x6C63E` tail + `0x91ACA`).
- Certification event: level 7 on two consecutive passes (`0xFFF93B9A == 2`).
- Steady engaged stream (live-stock, byte-exact, spec 03 §6.2p): J1 `8E91 C900 2200 0900`
  with echo `AAB1` (grant + rail), enables `9020 2400`, letters `63 00 00` (slot-0 'c'),
  staging frozen except the rolling selector and the 10-bit telemetry.
- The one-shot enable (first monitor pass, level < 7) means **bit 0 rides the word from
  the first frames of the ramp** — accepted DURING the walk (spec 03 §6.2 structural
  decode). The 0x91/0x81 dialect arrives with mode 0x19/0x31 (§3).
- Shutdown: mode 6 → bit 6 in the word (power-down, bench: 0.169→0.018 A latched); the
  hand-over disables outputs (`0x6C612(0)`), the fatal path re-inits RSPI-B, sends one
  CPU-mode frame and halts (spec 03 §4.4–§5).

---

## 7. THE NEVER-RULES — what the stock's discipline never does

Each rule: (a) the code evidence the stock avoids it, (b) the bench regression that
violated it. "Reset loop" = supervisor-initiated MCU reset every boot (uptime 0,
AAB0-primed fresh windows in the echo ring, spec 03 §6.2be).

| # | NEVER-rule | (a) Code evidence the stock avoids it | (b) Regression that broke it |
|---|---|---|---|
| **NR-1** | **Never a bit-0 (enable) word without the rolling register conversation running underneath.** The ask and the ladder are owned by the same monitor pass; the stock never asks into a silent bus | `0x873F0`: the one-shot `0x6C612(1)` fires in the SAME pass sequence that arms the walk (`0x6C48C(0x4B0)` at state 0); the ladder jobs (W1/W2) are queued every timer tick for the whole walk; live-stock captures show the conversation from the first exchanges | **E60m** (spec 03 §6.2bc): ladder wire-jobs silenced, bit-0 self-ask kept → **reset loop**; one-variable proof (E60l/i with jobs = first-pass grant) |
| **NR-2** | **Never a level-class `84|xx` word outside the stock's commissioning frame** — only values {0x3F init, 0x06, 0x1F} (validated: `0x6C48C` rejects value ≥ 0x3139), only through `0x91190`'s one-in-flight cyclic job, at the timer cadence with era dwells `value×200+2400`, with echo + commit-mask verification every reply and bump-and-resend resync | `0x91190` is the only 84-writer; templates force the `{84\|v, 0A, 12, 0C}` shape; `0x6C1EA` checks echo/commit each reply; `0x6C742` retries; `0x6C720/0x6C0BA` resync on mismatch | **E60n** (spec 03 §6.2be): the walk entering at `84|06` (bench 10 ms cadence, era floors ~1 s/~2 s, down-command to a warm chip) → **reset loop**; the **E16 family** (certain 84-values outside full commissioning reset instantly); our October bare `0x841F` (no commit/reads/repetition) latched the level but never certified |
| **NR-3** | **Never register-class data words (0x8E/0x90/0x96/0x88-with-data) on the supervisor's select outside its own descriptors.** Every driver-register write rides RSPI-A; the supervisor's select appears ONLY in read contexts and its own 8E/C9/90 jobs | Bus topology [O]: driver jobs 0x40A98–0x40B48 ride the bus-A five-select `0x40CE4` (spec 03 §6.2ai/6.2aw); bus-B descriptors contain no 8E/90/96/88 data rows except J1/J2/F (their own registers); the stock's write job `8E9F/9021/9655` staging matched the bus-A signature (§6.2e) | **E42/E42c/d** (five-select register bursts → MCU reset), **E45–E48** (bus-B companion, every content/cadence/mode variation → reset with bit0 in play; grant-poison bit0-free, spec 03 §6.2au/av), the **0x8E55** one-shot reset (2026-11-01: bits 0,2,4,6 = ungranted enable + POWER-DOWN) |
| **NR-4** | **Never the power-down bit (bit 6) outside mode 6; never bit 7 outside the engaged dialect; never bit 0 in the normal dialect's shape outside the stock's first-pass ask.** Dialects are mode-gated, not style choices | `0x91C86`: bit 6 only when `0xFFF90951 == 6`; bit 7 + bit-4-toggle only when `(mode & 0x11) == 0x11`; normal branch hard-clears bits 3/4/5 and toggles bit 2 | **E23** (100 passes of constant `0x8E40` → latched power-down fault, unrecoverable boot loop, spec 03 §6.2o); **E32r** (`0x8E91/0x81` from word one → reset loop, §6.2y/z); Oct-31 bit-7 reset (`0x91` shape outside mode); 2026-10-30 bit-0 instant-reset class |
| **NR-5** | **Never let the 5 ms mirror stream stall under grant.** Both buses run interrupt-driven at equal priority; neither ever waits on the mainline; burst work is interleaved by the scheduler, not stacked into one tick | The ISR/DMAC engine (vectors 445/449, clock-gate ack, spec 03 §6.2am–ao); IPR27 nibbles 4/4 (§6.2aw); check-free discipline prevents stacking; the stock's stall threshold sits between ~11 and ~20 ms (E28/E29) | **E27/E28** (40-word duty job landing in the same 10 ms tick → MCU reset AT the duty-fire second, our own WDT disarmed — the supervisor did it); **E40b** (100 ms burst → grant withdrawal class); E59's 20 ms guard loop alone could trip the threshold (§6.2aw) |
| **NR-6** | **Never two jobs on one descriptor in flight; never a mid-conversation re-frame.** One-in-flight check-free before every queue; a busy job is skipped, not stacked | `0x93B8E spi_job_check_free` called by every starter/builder; `0x937F8` slot discipline with integrity pairs; the wrong-chip lesson: half-routed jobs (our plat_sup_xfer alternation) put words on SSLB0 that belonged to SSLB1 (spec 03 §6.2az/ba) | **E42c/E42d** (stacked bursts), the E60j/k wrong-chip bug (commits to SSLB0 → certification could never latch — refused-but-no-reset), E60m's starved conversation |
| **NR-7** | **Never a register write without its commit and read-back.** Every write template ends `1200` + `0C00/0A00` reads; the commit status is CHECKED (& 5 vs phase mask) every reply; the 0x90/0x22/0x24 writes are echo-verified with the stock's exact masks (0xDF / 0x15) | Templates @0x40CA6/0x409EC/0x40C08; `0x6C1EA` commit check; `0x92D94` mask 0xDF; `0x92ED4/0x92FD0` mask 0x15 | Our October-era bare `0x841F`/`0x8820`/`0x9020` one-shots (no commit, no reads, no repetition) — level happened to latch, nothing certified; E60l proved even stock-exact words ack-but-don't-latch without the full frame (§6.2bc) |
| **NR-8** | **Never an ungranted rail assumption.** The rail (echo bit 5) is the supervisor's OWN output — the stock's echo match deliberately excludes bit 5; outputs gate on level ≥ 6 AND the granted echo bit, and the 0x90 letter bits are fault-masked from the 0x14 reply BEFORE sending | `0x92D94` mask 0xDF; `0x6C63E` output gate; `0x92E80` merge `~(0xFFF907D1 & 0xE0)`; `0x92BBE` → fault-path twin job F on any 0x14 fault bit | The whole E13–E26 ask/rail hunt (bit-0 in every timing/shape tried; the "primed ≠ conducting" law, §6.2ae/ay); attempts to write 0x90 switch bits while faulted/ungranted (accepted, no observable change — spec 03 §4.4 bench note) |

Umbrella rule: **the stock never improvises a word the job table doesn't contain** — every
bus-B frame that ever leaves the SH7254R is one of the ten descriptors' template words
(plus the builders' OR'd live bytes). Both bench regressions (E60m, E60n) were, at bottom,
violations of the frame: silencing stock frames (m) or re-sequencing stock values at
non-stock cadence (n).

---

## 8. Open questions — what only the chip knows

1. **The certification latch**: what internal condition flips `0x24`'s data byte from
   `0x00` (ack-but-unlatched, E60l) to the stock's `0x20`? Candidates: the 84-write's
   runtime era sequencing (our floors vs the stock's dwells), the classifier letters'
   internal conditions (`0x85828/0x8C7A8/0x8C6D4`), the E60m-exposed engage-path
   dependency. One byte, undecoded (spec 03 §6.2bc).
2. **Which sub-delta of the 0x06 era is fatal for a replica**: cadence (10 ms vs timer
   tick), dwell (1 s floor vs 3.6 s *[the era value is a 3600 µs cycle — §6.3 correction]*), the down-command to a boot-warm chip, or the missing
   8th template word `0x0C01` / the 0x1E-context repetition. Only the planned stock
   observation run (restore stock, read `0xFFF9054E/0xFFF90551/0xFFF93BA1/0xFFF90944` live
   at ≤1 s cadence through the first 15 s, spec 03 §6.2be) separates them.
3. **The READY law**: after each MCU restart the supervisor ignores bit-0 asks for a
   variable ~100–200+ pass period, then grants on the first qualifying word; what gates the
   ready moment (spec 03 §6.2r). Related: the grant lease length varies (122–417 s) and its
   end conditions (accumulated stream hiccups?) are chip-side.
4. **The rail-hold mechanism**: the stock's rail never drops from T+0 (boot-manager words
   + gapless handover); our window closes at ~320 ms regardless of content. Whether the
   renewal is protocol (the boot's exact 5 ms dialect at handover) or electrical
   (WDTOVF/supply sequencing) is unobserved on the sealed TCU (spec 03 §6.2p/§6.2u).
5. **The telemetry words**: the 10-bit value's physical meaning (`0xFFF907CC`, 373–380
   live), the 0x28 status bit that differs between us and stock (AA45 vs AA46), the 0x2E
   drifting counter, and why our 0x2E-era replies carry bit 11 (0xAB class) that the
   stock's own grammar would reject. *(2026-11-19: the 10-bit value is the die temperature — §4.6 correction.)*
6. **Self-persistence**: the bench chip powers up at level 0x1F engaged-ready and keeps
   latched state across application reboots while powered (spec 03 §6.2j); whether
   commissioning survives full power-off (and whether the stock's walk-per-boot is
   therefore mandatory or merely conservative) is chip-internal.
7. **SSLB0 and SSLB2's roles**: SSLB0 (0xBB-ID system chip, the inverted status nibble,
   the C9 mirror) and SSLB2 (~~answers everything with a rolling counter~~ a board temperature sensor, §4.7
   correction 2026-11-19) — neither datasheet is known; their part in the grant/rail story is open.
8. **The stall/withdrawal thresholds**: exact supervisor reset threshold for 5 ms-stream
   gaps (bracketed 11–20 ms) and the withdrawal class mechanism (one observation,
   spec 03 §6.2ad) — internal timers of the chip.
9. **Vector 224's timer unit identity** (peripheral 0xFFFFF580 family) and the exact
   time-base scaling of the era arithmetic (µs vs ms vs the <<8 compare) — manual-level
   detail, unresolved in spec 02.

---

## Appendix — quick wire reference (all words big-endian, hex)

```
5 ms word job J1      : 8E{ctrl} C9{c9} 2200 0900          -> AAxx AAxx AAxx AAxx (echo at rx[2])
enables J2 / fault F  : 90{en} 2400                         -> AAxx AAxx (cert echo at rx[1], mask 0x15)
telemetry J3          : 2800 2E00                           -> AAxx AAxx (10-bit: (rx0&F)<<6 | rx1&3F)
SSLB2 J4              : 00FF                                -> board temperature, 1/128 °C (FF03 = not ready) [corrected 2026-11-19, §4.7]
ID J5                 : 0600 0300 0000 0000                 -> A8/A2-class IDs
status J6             : 1800 0C00 1E00 0600 1400            -> status + 0x14 fault bits (E0)
SSLB0 poll J7         : 0000                                -> nibble (0xF ^ (rx&F))
duty/token W1         : 88{t0} 88{t1} 88{t2} 88{t3} 1200 0C00 0A00   (flash template +0C01)
level W2              : 84{lvl} 0A00 1200 0C00              -> echo/commit/feedback
0x8E ctrl byte        : bit0 enable, bit1 90comp, bit2 toggle(normal), bit4 toggle(engaged),
                         bit6 power-down, bit7 engaged-marker; bits 3,5 always 0
echo 0x22             : mirror of ctrl | bit5 rail (excluded from stock's match, mask 0xDF)
level ladder          : 0x3F (init) -> 0x06 (state 0, ~3.6 s) -> duty/token era -> 0x1F (0x23, ~8.6 s)
                        [corrected 2026-11-11: 3600 / 8600 µs cycles, not seconds — §6.3]
```
