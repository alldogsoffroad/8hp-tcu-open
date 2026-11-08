# Unit 12 (r08_app)

Region 0x000800AA–0x0008950E. This is the **UDS (ISO 14229) application service layer** of
spec 15: the service dispatch front-end, the per-service handlers named in spec 15 §2
(0x10 @0x00080A10, 0x11 @0x00081126, 0x14 @0x0008086C, 0x19 @0x00081054, 0x22 @0x00080C78,
0x23 @0x000812D4, 0x27 @0x00081490, 0x28 @0x00080808, 0x2E @0x00081804, 0x31 @0x00081D46,
0x3E @0x000817D0, 0x85 @0x000809C4), the DID read engine, and a family of gated wrappers
into the stream-channel module of unit 08 (`stream_chan_*` at 0x00074D28+). The handler
calling convention is `(svc_config*, req_ctx*)` where the request context is
{buf ptr @0, resp len u16 @8, req len u16 @+6, result type @+10 (1=pos, 2=neg, 3=suppress,
4/6/7=pending variants), flags @+0xE}. Negative response codes written into ctx buf are UDS
NRCs (0x12, 0x13, 0x22, 0x31, 0x33, 0x7E, 0x7F). [O]

#### 0x000800AA `diag_svc_dispatch_select` — logic
Service dispatch prologue: indexes the 0x30-stride service-config table at DAT_fff92480
(installed by 0x00073214), stores current-service id DAT_fff94078 and the handler pointer
pair from entry+0x24 into DAT_fff92f94/DAT_fff92f98, runs pre-hooks via PTR_FUN_00080150/
00080154/00080158, then matches the request code against the SID table DAT_fff924c0
(count DAT_fff93bc8, installed via 0x00073f42) recording the translation into
DAT_fff924d2/d3/d4; returns whether a match was found. Callers='-' (service/pointer table).
Single-member cluster. [O] spec 15 §2.

#### 0x00080108 `diag_svc_load_current` — logic
Abbreviated form of 0x000800AA without the SID match loop: loads the service-config entry
from (idx&0xff)*0x30 + DAT_fff92480 into DAT_fff94078/DAT_fff92f94/DAT_fff92f98 and runs the
setup hook via PTR_FUN_00080150(entry+0x10, entry[0x14]). Callers='-'. Single-member
cluster. [O]

#### 0x00080160 `diag_svc_load_timing` — data-accessor
Copies two 3-word timing blocks from the service-config entry (+0x1C and +0x20 pointers)
into the P2 timing globals DAT_fff92f54-0x92f60 and DAT_fff92f68-0x92f74, clearing
DAT_fff92f5c/70 and seeding both counters with 5000. Caller 0x0007FFB4 (r04_boot session
bring-up). Single-member cluster. [O] mechanics, [I] P2/P2* timing role (spec 15 §3).

#### 0x00080190 `diag_idle_state_reset` — logic
Idle/reset helper: latches the free-running timer DAT_fffff220 into DAT_fff9242c and
DAT_fff92430, sets DAT_fff92438=2, clears selector DAT_fff93ba9 and busy flag DAT_fff94079.
Callers='-'. Single-member cluster. [O]

#### 0x000805A8 `diag_resp_publish_stream` — logic
Sets busy flag DAT_fff93bac=1, latches the timer into DAT_fff92430, derives a stream channel
(data ptr DAT_fff92f90 or config-based DAT_fff800b0 + *(DAT_fff9247c+0x18), length
DAT_fff9387c or 8) and publishes the current service buffer via PTR_FUN_00080604 = 0x000806F4
(→ stream_chan_set_value). Callers='-'. Single-member cluster. [I] — response/notification
publication on a periodic stream channel.

#### 0x000806F4 `diag_stream_set_value` — stub
Gated wrapper: only when (chan & 0x80)==0 forwards 5 args through table slot 0x000807D4 to
stream_chan_set_value (0x00074D28, unit 08). Callers 0x000805A8 and 0x00073228
(diag_register_channels). Single-member cluster. [O]

#### 0x00080714 `diag_stream_clear_flags` — stub
Gated ((id & 0x80)==0) forward to stream_chan_clear_flags (0x00074D90) via slot 0x000807D8;
returns 0 when gated. Callers='-'. Single-member cluster. [O]

#### 0x0008072A `diag_stream_set_mode` — stub
Gated forward to stream_chan_set_mode (0x00074DC4) via slot 0x000807DC. Caller 0x0007FFB4.
Single-member cluster. [O]

#### 0x00080738 `diag_stream_take_flag` — stub
Gated forward to stream_chan_take_flag (0x00074DFE, consume bit 0x40) via slot 0x000807E0;
param is unused. Callers='-'. Single-member cluster. [O]

#### 0x0008074C `diag_stream_get_len` — stub
Gated forward to stream_chan_get_len (0x00074E32, u16 at channel state +0x14) via slot
0x000807E4. Callers='-'. Single-member cluster. [O]

#### 0x00080774 `diag_stream_get_tag_a` — stub
Ungated forward to stream_chan_get_tag_a (0x00074E5E) via slot 0x000807EC. Cluster of 2 —
member 0x00080782 (`diag_stream_get_tag_b`) forwards through slot 0x000807F0 to the
identical +0x1D accessor 0x00074E76. Callers 0x000800AA. [O]

#### 0x00080790 `diag_stream_stub_807f4` — stub
Two-argument forward through table slot 0x000807F4 (target unresolved in edges.csv; sibling
slot 0x000807F8 is stream_chan_commit 0x00074E90, so this is another stream-channel op).
Callers='-'. Single-member cluster. [I]

#### 0x000807A8 `diag_call_handler_b` — stub
Calls (*DAT_fff92f98)(a, b) — the second handler pointer the dispatcher loaded from
service-config entry+0x24. Note "could not recover jumptable" warning: the call is an
indirect dispatch, not a jumptable. Callers='-'. Single-member cluster. [O]

#### 0x000807B6 `diag_stream_get_state0` — stub
Ungated forward to stream_chan_get_state0 (0x00074F20) via slot 0x000807FC. Callers='-'.
Single-member cluster. [O]

#### 0x000807CC `diag_stream_stop` — stub
Ungated forward to stream_chan_stop (0x00074F5A) via slot 0x00080804. Callers='-'.
Single-member cluster. [O]

#### 0x00080808 `uds_28_comm_control` — logic
CommunicationControl (0x28) handler: requires req len 2 (else NRC 0x13); asks control
byte via PTR_FUN_00080868 — 8→NRC 0x12, 9→NRC 0x31, other nonzero→NRC 0x22; zero accepts
(positive echo, or type 3 when suppress bit 0x80 set). Callers='-' (service table).
Single-member cluster. [O] spec 15 §2.

#### 0x0008086C `uds_14_clear_dtc` — logic
ClearDiagnosticInformation (0x14): on len!=3 sets stored NRC DAT_fff9407b=0x13; for group
0xFFFFFF builds a 0x18-byte clear record (copies req bytes, marks type 4) and runs the
clear sequence via PTR_FUN_000809a0-a0b4, answering type 7 (0x78 pending) mid-flow; state
'D' path returns the stored NRC or completion byte; a P2* watchdog (PTR_FUN_000809c0,
timer*1000) can force NRC 0x22 + status 0x44. Callers='-' (service table). Single-member
cluster. [O] spec 15 §2.

#### 0x000809C4 `uds_85_ctrl_dtc_setting` — logic
ControlDTCSetting (0x85): passes (subFunction&0x7f, buf, len) to PTR_FUN_00080a0c — nonzero
return becomes the NRC (type 2); zero yields the positive echo (type 1, SID masked to 7
bits) or type 3 when the request suppress bit 0x80 was set. Callers='-'. Single-member
cluster. [O] spec 15 §2.

#### 0x00080A10 `uds_10_session_control` — logic
DiagnosticSessionControl (0x10): scans the session table (config+0, count config+4) for the
sub-function, validates session availability via PTR_FUN_00080c48 and the precondition
callback config[3] (NRC 0x33/0x22), commits the new session through 0x00080c4c into
DAT_fff9407e, then invokes the session handler table config[2][idx] mapping handler results
(1, 0x11, 0x12, 0x13, 0x17, 0x18, 8, 0x14, 0x15, 0x16) to response types 3/6/7/10 or NRCs
0x12/0x13/0x22/0x7E/0x7F. In-progress flag DAT_fff9407d, pending timer DAT_fff92f9c.
Callers='-'. Single-member cluster. [O] spec 15 §2/§3.

#### 0x00080C5C `diag_session_state_clear` — data-accessor
Clears the 0x10 in-progress flag DAT_fff9407d and reloads the active session index into
DAT_fff9407e via PTR_FUN_00080c74. Callers='-'. Single-member cluster. [O]

#### 0x00080C78 `uds_22_read_data_by_id` — logic
ReadDataByIdentifier (0x22): requires even len>1 (else NRC 0x13), copies the u16 DID list
into the DID buffer (config+0x10, 4-byte stride, count check vs config+0x16), then loops
per-DID through 0x00080E08 handling pending (0x78, max tries vs config+0x17), response
overflow vs config+4+0x12 (NRC 0x14), DID-done path (type 1 with total len) and NRCs
0x31/0x33/0x22/0x13; progress in DAT_fff9407f/80/81/83, cursor DAT_fff93880. Callers='-'.
Single-member cluster. [O] spec 15 §2.

#### 0x00080E08 `uds_22_read_one_did` — logic
Single-DID read engine for 0x22: fetches the DID u16 from the DID list (idx DAT_fff94080),
checks the dynamic-definition gate via PTR_FUN_00080ef0 (result 1 = dynamic DID: copies
data directly via PTR_FUN_00081038, 2 = reject), else binary-searches the 0xC-stride DID
table (config+0, count config+5) or uses the lookup callback config[2] for 0xF2xx/0xF3xx;
sets up the read record (config+1: handler ptr, buffer, sizes) and invokes the DID read
handler through PTR_PTR_00081034. Returns 0 ok / 1 pending / 3 overflow / 5 not-found.
Caller 0x00080C78. Single-member cluster. [O]

#### 0x0008103C `uds_22_state_clear` — stub
Clears the 0x22 in-progress flag DAT_fff94083. Callers='-'. Single-member cluster. [O]

#### 0x00081048 `uds_19_state_clear` — stub
Clears the 0x19 in-progress flag DAT_fff94085 and its suppress-echo bit DAT_fff94088.
Callers='-'. Single-member cluster. [O]

#### 0x00081054 `uds_19_read_dtc` — logic
ReadDTCInformation (0x19): requires len!=0 (NRC 0x13); matches the sub-function in the table
at config+4 (count config+0x13), latches index DAT_fff94087 and suppress bit
DAT_fff94088=req[0]&0x80; checks session mask (PTR_FUN_00081118 → NRC 0x7E when the
configured handler exists but the session bit is clear) then dispatches the sub-handler
from table config+8; suppress-positive conversion at the end. Callers='-'. Single-member
cluster. [O] spec 15 §2.

#### 0x0008111C `uds_11_state_clear` — stub
Clears the 0x11 in-progress flag DAT_fff94089. Callers='-'. Single-member cluster. [O]

#### 0x00081126 `uds_11_ecu_reset` — logic
ECUReset (0x11): sub-function (req[0]&0x7f) looked up in the reset-type table (config+0,
count config+0x11, miss → NRC 0x12), session availability via PTR_FUN_000812d0 (NRC 0x33)
and the session mask table param_1[6] (NRC 0x7E, latching the timer DAT_fff92fa0 on
pass); the type callback param_1[3] then decides: 0 → start the reset (flag DAT_fff94089,
response type 4, launch via (**(DAT_fff924a4+0x24))(type table value)), 1 → type 7
(pending), 0x1B → type 4, 4 → NRC 0x31, else NRC 0x22; pending state times out after
499999 timer ticks into a suppressed (type 3) response; positive path echoes the
sub-function, adding a second byte (config+4) for sub-function 4. Callers='-' (service
table). Single-member cluster. [O] spec 15 §2.

#### 0x000812D4 `uds_23_read_mem_by_addr` — logic
ReadMemoryByAddress (0x23): parses the address/length size nibbles from req[0] (both must
be 4, length nibble non-zero, total length consistent → NRC 0x31/0x13), builds the address
(DAT_fff92fa8) and length (DAT_fff92fa4) big-endian, checks the security/session gate via
PTR_FUN_00081478 (mask from PTR_FUN_00081474) and reads through 0x00081CFC into the
response buffer; pending result keeps DAT_fff9408b with a type-7 response; final positive
carries length DAT_fff92fa8; failures → NRC 0x31/0x22. Callers='-' (service table).
Single-member cluster. [O] spec 15 §2.

#### 0x00081484 `uds_23_state_clear` — stub
Clears the 0x23 in-progress flag DAT_fff9408b. Callers='-'. Single-member cluster. [O]

#### 0x00081490 `uds_27_security_access` — logic
SecurityAccess (0x27) state machine driven by DAT_fff93bcf: requestSeed (odd sub-function)
validates the 0xC-stride table pair (session mask via entry+8 → NRC 0x7E, delay/state via
PTR_FUN_00081720) and either returns the seed (positive, len config+0x13) or starts the
async key exchange (0x00081A98); sendKey (even) path checks sequence (NRC 0x24), 1 s
minimum delay window (0xF4241 ticks → NRC 0x35 with retry counter DAT_fff82e54++), invokes
the verify callback table param_1[1] and compares the returned key bytes; retry counter
DAT_fff9408d vs limit config+4 → NRC 0x26; mismatch NRC 0x22/0x12/0x13/0x37 as flagged.
Callers='-'. Single-member cluster. [O] spec 15 §2.

#### 0x000817D0 `uds_3e_tester_present` — logic
TesterPresent (0x3E): len must be 1 (else NRC 0x13); sub 0x00 → positive 1-byte echo,
0x80 → suppressed (type 3), anything else → NRC 0x12. Callers='-'. Single-member cluster.
[O] spec 15 §2/§4a.

#### 0x00081804 `uds_2e_write_data_by_id` — logic
WriteDataByIdentifier (0x2E): needs len>=3 (NRC 0x13); DID from req[0..1], precondition
via PTR_FUN_00081a04 (result 1 = echo-only, 2 = pending type 7, >0xF passes through as
NRC); binary-searches the 0xC-stride DID table (config+0, count config+2), builds the
write record (config+1: handler, buffer, cursor, sizes) and invokes the DID write handler
through PTR_PTR_00081a08 indexed by the DID's handler id DAT_fff9408f; maps handler
results 6/4/5/9/8/0xF/0x19 to NRCs 0x13/0x33/0x72/0x31/0x7F/0x24/0x10 (else 0x22).
In-progress flag DAT_fff94090. Callers='-'. Single-member cluster. [O] spec 15 §2.

#### 0x00081A0C `uds_2e_state_clear` — stub
Clears the 0x2E in-progress flag DAT_fff94090. Callers='-'. Single-member cluster. [O]

#### 0x00081A18 `uds_28_comm_enable` — stub
Applies communication-control(0, 1) through PTR_FUN_00081a8c — the same applier 0x00081A20
uses for enable — i.e. re-enables reception/transmission after a session hand-over.
Caller 0x00075F86 (diag_session_prep_outputs, unit 08). Single-member cluster. [O]

#### 0x00081A20 `uds_28_apply_control` — logic
CommunicationControl validator/applier used by handler 0x00080808 (via PTR_FUN_00080868):
control type (req[0]&0x7f) must be 0/1 (else 8→NRC 0x12), req[1] must be low-nibble 1 with
zero high nibble (else 9→NRC 0x31); checks the bus capability mask (PTR_FUN_00081a90 vs
PTR_PTR_00081a94[DAT_fff90ab8], else 5→NRC 0x22) then applies via PTR_FUN_00081a8c.
Caller 0x00080808. Single-member cluster. [O]

#### 0x00081A98 `uds_27_keycalc_poll` — logic
Async crypto/key-operation state machine on DAT_fff84a1c: idle → start via
PTR_FUN_00081aec(1) and poll status (PTR_FUN_00081af0) returning 1 (pending) until
running; running → collect result via PTR_FUN_00081af4(buf); result byte 'f' = verified
(returns 2), anything else resets the state and returns 0. Callers 0x00081490 (0x27) and
0x00074A04 (diag_timeout_poll). Single-member cluster. [I] role, [O] mechanics.

#### 0x00081C34 `eeprom_write_record_table` — logic
Walks an 8-byte-stride record table {len u16, type byte, data ptr}: checks the length sum
against *(u16*)(param_1+4) (mismatch → 6); for phase 0 entries gated by PTR_FUN_00081cec,
type 2 invokes PTR_FUN_00081cf0(offset) and type 4 PTR_FUN_00081cf4(offset, u16), and for
every entry calls the flash/EEPROM program thunk 0x0007D1A8(base+off, data ptr, len).
Caller 0x00082944. Single-member cluster. [O] mechanics, [I] EEPROM/dataflash target.

#### 0x00081CFC `uds_23_read_mem` — logic
Read-by-address accessor for UDS 0x23: forwards (address, length) to PTR_FUN_00081d34
(phase 0) or PTR_FUN_00081d38 (phase 1); returns 1 = data ready / 3 = failure (any other
phase → 3). Caller 0x000812D4. Single-member cluster. [O]

#### 0x00081D3C `uds_31_state_clear` — stub
Clears the 0x31 in-progress flag DAT_fff94091. Callers='-'. Single-member cluster. [O]

#### 0x00081D46 `uds_31_routine_control` — logic
RoutineControl (0x31): len>=3 (NRC 0x13); sub-function 1/2/3 (start/results/stop), routine
id u16 at req+1; routine lookup in the u16 table (config+0, count config+5, NRC 0x31),
availability via PTR_FUN_00081f74 (NRC 0x33), handler picked from config[2]/[3]/[4] by
sub-function (NULL → NRC 0x12); handler result codes map: 1 → type 7 pending, 0x19 →
type 10, 6 → 0x13, 4 → 0x33, 0xC/0xD/0xE → NRC 0x72, 9 → 0x31, 0xB → 0x24, 0xF → silent;
result-copy path length-checked against ctx (NRC 0x14); two-phase pending state
DAT_fff94091/92. Callers='-'. Single-member cluster. [O] spec 15 §2/§4 (routines FE10/FE20/FE30).

#### 0x00081F84 `diag_flag_94093_set` — stub
Sets status byte DAT_fff94093. Callers 0x0008086C, 0x0008843E, 0x00088426, 0x0008845A.
Single-member cluster. [O]

#### 0x00081F8C `diag_flag_94093_get` — stub
Returns status byte DAT_fff94093. Same caller set as the setter. Single-member cluster. [O]

#### 0x00081F94 `diag_cnt_9388a_add` — stub
Adds a signed u16 to counter DAT_fff9388a. Caller 0x0008845A. Single-member cluster. [O]

#### 0x00081FA0 `diag_flag_94094_latch` — stub
When DAT_fff94093 != 0, latches DAT_fff94094 = param_1 and sets flag DAT_fff94095 = 1.
Callers 0x00088420, 0x0008086C. Single-member cluster. [O]

#### 0x00081FB4 `diag_flag_94094_get` — stub
Returns byte DAT_fff94094. Caller 0x0008086C. Single-member cluster. [O]

#### 0x00082170 `diag_cur_service_get` — stub
Returns the current-service id DAT_fff94078 (set by the dispatcher 0x000800AA/0x00080108).
Caller 0x0008086C. Single-member cluster. [O]

#### 0x00082178 `diag_cleardtc_poll_done` — logic
Polls condition through PTR_FUN_00082198 twice (params passed through); returns 1 when the
result is 4 or 1. Used by 0x0008086C in the deferred ('D') phase of ClearDiagnosticInformation
to detect operation completion. Caller 0x0008086C. Single-member cluster. [I]

#### 0x0008219C `sol_forced_off_get` — stub
Returns flag DAT_fff84a2c ("all solenoids forced off"). Caller 0x000761B0
(cond_poll_fff94097, unit 08), which is polled from 0x0014FC5C/0x00133C4E. Single-member
cluster. [O]

#### 0x000821A4 `sol_all_off_set` — logic
Forces every solenoid off: calls sol_set_group_values (0x0006CFD4, unit 07) with mask
0xff/value 0 via PTR_FUN_000821d8 and sets DAT_fff84a2c = 1. Reached through thunk
0x000761C8 (r04_boot) — the safe-state switch of the flash hand-over. Single-member
cluster. [O] mechanics, [I] hand-over role (spec 12/16).

#### 0x000821BE `sol_all_off_clear` — logic
Releases the forced-off state: sol_set_group_values(0xff, 1) via PTR_FUN_000821d8 and
clears DAT_fff84a2c. Reached through thunk 0x000761C2. Single-member cluster. [O]

#### 0x000824BE `did_rd_3bus_bytes` — logic
DID read callback: in phase 0 appends three bytes to the response record (buffer +0xC,
cursor +0x12, max +0x14), each fetched from a per-bus byte table (slots 0x000826EC/826F0/
826F4 indexed by bus DAT_fff90ab8); returns 0 ok / 3 overflow / 5. Callers='-'. Single-member
cluster. [O]

#### 0x0008252A `did_rd_6bus_bytes` — logic
DID read callback appending six per-bus table bytes (slots 0x000826F8..0x0008270C indexed
by DAT_fff90ab8) to the response record; same overflow/status convention as 0x000824BE
(3 overflow, 6 for phase 2). Callers='-'. Single-member cluster. [O]

#### 0x000826BC `did_rd_svc_cfg_2bytes` — logic
DID read callback appending two bytes from the current service's 0x10-stride config entry
(table *PTR_PTR_00082904 indexed by DAT_fff94078, bytes +0x14/+0x15) to the response
record; phase 2 returns 2 (done), overflow 3, else 5. Callers='-'. Single-member cluster. [O]

#### 0x000828AE `did_rd_rom_ec0c_10bytes` — logic
DID read callback: phase 0 appends the 10 constant bytes at ROM 0x0000EC0C to the response
record (buffer +0xC, cursor +0x12, max +0x14); phase 2 → 10 (done), overflow 3, else 5.
Callers='-'. Single-member cluster. [O]

#### 0x00082944 `eeprom_write_async_poll` — logic
Async write state machine on DAT_fff84a1d: idle → start the record-table write
(0x00081C34 via PTR_FUN_000829b8) and return 1 until the op reports running; running →
collect the result via PTR_FUN_000829c0; result byte 'f' = success (0), else failure (5);
propagates 4/6. Callers='-'. Single-member cluster. [O] mechanics, [I] EEPROM target.

#### 0x000829C4 `flash_ctrl_enable_sel` — stub
Maps selector 1 → PTR_FUN_00082bec(1, 0xFE), 0 → (0, 0xFF); other values return 0
without action. Callee 0x0007C736. Callers='-'. Single-member cluster. [O]

#### 0x000829F4 `flash_status_map` — stub
Polls the shared flash-op status (PTR_FUN_00082bf0): 0 → 0 (idle/ok), 10 → 1, anything
else → 2 (error). Callers='-'. Single-member cluster. [I]

#### 0x00082A16 `reflash_mat_copy_blocks` — logic
Chunked copy of the 0x180000 mirror into flash/dataflash at 0x600000: chunk length =
min(PTR_FUN_00082bf4(2), PTR_FUN_00082bf4(4)); each round erases both ranges
(PTR_FUN_00082bf8(addr, len, 2/4, 0x10)) then copies via PTR_FUN_00082bfc, advancing
DAT_fff92fbc/DAT_fff92fc0 until the 0x67FFFE boundary; result bytes 0x24/0x22/0x23 are
reported in param_2[1], 0xFF marks completion; 3 = still busy. Callers='-'. Single-member
cluster. [O] mechanics, [I] MAT-copy routine FE20 role (spec 15 §4).

#### 0x00082B42 `reflash_mat_copy_4k` — logic
Simpler copy variant: moves fixed 0x4000 chunks from 0x180000 to 0x600000..0x67FFFF via
PTR_FUN_00082bfc only (no erase), finishing with 0xFF in param_1[0]; busy flag
DAT_fff940a0, 3 = in progress. Callers='-'. Single-member cluster. [O] mechanics, [I]
same MAT-copy family as 0x00082A16.

#### 0x00082C00 `flash_erase_prog_phased` — logic
Two-phase flash op: phase 0 issues op type 2 and phase 1 op type 4 via
PTR_FUN_00082d38(addr, len, type, 0x10); maps the driver result {0,1,2,4} through, other
→ 3; phase >1 → 3. Callers='-'. Single-member cluster. [O]

#### 0x00082C52 `lib_memdup_alloc` — library
Allocates 0x50 bytes via PTR_FUN_00082d3c, copies param_3 bytes src→dst, then frees the
scratch block via DAT_00082d40; returns the advanced destination pointer. Caller
0x000792AC (can_tx_build_frames). Single-member cluster. [O]

#### 0x00082C7C `flash_write_mirror_480000` — logic
Program helper: when type-1 and the address lies in 0x180000-0x1FFFFF it remaps the
target by +0x480000 (to the 0x600000-range mirror); then op 4 via PTR_FUN_00082d38 and
copies param_4 data through PTR_FUN_00082d44; zero length is a no-op. Callers='-'.
Single-member cluster. [O] mechanics, [I] flash-write role.

#### 0x00082CD0 `flash_write_remap_120000` — logic
Variant of 0x00082C7C with +0x120000 remap for 0x180000-0x1FFFFF addresses and op type 2;
skips the op-2 step when the range is inside the protected 8-byte window
PTR_LAB_00046918_1_00082d48 (0x46918..0x46920); copies param_4 data via PTR_FUN_00082d44.
Callers='-'. Single-member cluster. [O] mechanics.

#### 0x00082D4C `tbl_mask_lookup_u16` — library
Scans a u16 pair table for the first entry whose mask value satisfies
(entry & param_1) == entry (2x-unrolled, odd-length tolerant) and returns the entry's
second u16; 0 when no entry matches. Callers 0x0007B358/0x0007AC44/0x0007B222/0x0007B4EC/
0x0007C618 (r04_boot) and 0x00082F84. Single-member cluster. [O]

#### 0x00082DD4 `meas_pub_sig29_graded` — logic
Publishes signal 0x29: primes via PTR_FUN_00082ecc(0xBC2, 0), mirrors quality counters
(DAT_fff940a3/a2), packs two 2-bit grades from DAT_fff940fd/fff94100 (<7 → 2, 6..5 → 1,
else 0) into DAT_fff940a1, clamps value ((DAT_fff939d8+8)>>4)+0x28 into DAT_fff940a4, then
emits via PTR_FUN_00082ed0/ed4 with the two low param bits as flags. Callers='-'. Single-
member cluster. [O] mechanics, [I] graded measurement publication (spec 10).

#### 0x00082EE0 `dtc_evt_186a_queue` — stub
If any bit of param except bit 0 is set, queues fault event (0x186A, 0x10, 0) through
DAT_00083040 → dtc_queue_event (0x0006F5C0, unit 07); zero/no-bit-0 input returns
silently. Callers='-'. Single-member cluster. [O]

#### 0x00082EF6 `dtc_bits_186a_accum` — logic
Records the bitmask through the fault-occurrence tracker (PTR_FUN_00083044 → 0x0007CA24)
with id 0x186A, then ORs the mask bytes into accumulators DAT_fff940ab (param_1) and
DAT_fff940a9 (param_1 & param_2); zero masks return early. Callers='-'. Single-member
cluster. [O]

#### 0x00082F32 `dtc_evt_177a_queue` — stub
Sibling of 0x00082EE0 for event id 0x177A with mask 0xFFE0 (bits 5+). Caller 0x0007DD3C.
Single-member cluster. [O]

#### 0x00082F48 `dtc_bits_177a_accum` — logic
Sibling of 0x00082EF6 for id 0x177A: accumulates into DAT_fff940aa (param_1) and
DAT_fff940a8 (param_1 & param_2). Callers='-'. Single-member cluster. [O]

#### 0x00082F84 `meas_pub_sig2a_32` — logic
Publishes the two aggregated check results: for (DAT_fff940a9 & ab) runs
tbl_mask_lookup_u16 (0x00082D4C) over table PTR_PTR_00083048 and emits signal 0x2A
(PTR_FUN_00083050); resets both accumulators, refreshes DAT_fff940a7/DAT_fff92fd0; then
for (DAT_fff940a8 & aa) looks up PTR_PTR_00083054 and emits signal 0x32 via
PTR_FUN_00083058. Caller 0x0007AB48 (task_poll_group_a). Single-member cluster. [O]
mechanics, [I] plausibility-status publication (spec 10).

#### 0x0008305C `meas_bin_classify_16tbl` — logic
One-shot init (DAT_fff84a1e), then reads a value via PTR_FUN_0008311c(*PTR_DAT_00083118)
and walks the 16-entry u16 threshold table (PTR_DAT_00083120) to pick bin DAT_fff94136;
on bin change reloads enable byte DAT_fff9481e; when enabled loads the bin's flag word
from *PTR_PTR_00083128 into DAT_fff9413a and derives booleans DAT_fff9413b/c/9413c from
bits 0x200/0x100. Caller 0x0007AB48. Single-member cluster. [O] mechanics, [I] thermal/
operating-point bin classifier.

#### 0x0008312C `meas_seq_step_u16` — logic
Two-byte state sequencer: disabled (param_4 != 1) resets the state and returns 0;
enabled returns successive u16s from the table at param_2 (index clamped to param_3-1,
post-incremented) — a staged ramp/sequence source. Caller 0x0007B6D8
(meas_acquire_words). Single-member cluster. [O]

#### 0x0008316C `meas_frame_build_chan` — logic
Frame assembler for channel idx (param_1 & 0xff): descriptor at *PTR_PTR_00083270 +
idx*0x10 = {script ptr, dst buffer, length, capacity}; gated on script[0]==1 and
DAT_fff93b42==3; copies the script's mixed-size source list (8-byte, 4-byte, 2-byte and
1-byte items, counts in script bytes 4-7) into the destination until capacity is
exhausted, stores the length into RAM register DAT_fffc0d04 and triggers the commit hook
PTR_FUN_0008327c. Callers='-'. Single-member cluster. [O] mechanics, [I] measurement/
stream frame build (spec 10).

#### 0x00083280 `lib_seed_xor_timer` — library
Builds a 32-bit word from bit 4 of each of 32 u16 table entries (via PTR_FUN_000832a8)
and XORs it with the free-running timer DAT_fffff220 — a rolling seed/checksum. Caller
0x0007A97C (r04_boot). Single-member cluster. [O] mechanics, [I] purpose.

#### 0x000832AC `sig_hub_a` — dispatch
Runs ten no-arg hooks (publishers, reloads, trace releases) through the pointer slots 0x00083344-0x00083364 into the
0x00088A50-0x00089508 DID/registry area (incl. 0x000894B6, 0x00088BAC, 0x00088EB0).
Callers='-'. Single-member cluster. [O]

#### 0x000832D4 `sig_hub_b` — dispatch
Runs six hooks (slots 0x00083368-0x0008337c) into 0x00088ABE-0x00088EE6. Callers='-'.
Single-member cluster. [O]

#### 0x00083300 `sig_hub_c` — dispatch
Runs ten hooks (slots 0x0008338c-0x000833b0) into 0x000887D8-0x0008941C plus 0x0009A3FC/
0x0009A428. Callers='-'. Single-member cluster. [O]

#### 0x0008332C `sig_hub_d` — dispatch
Runs five hooks (slots 0x0008338c/b4/b8/bc/b0 — shares first and last slot with
sig_hub_c) into 0x00089114-0x00089518. Callers='-'. Single-member cluster. [O]

#### 0x000833C0 `mon_target_set` — logic
Sets the monitor target value DAT_fff93a06: first call converts (param, 10000) via
DAT_00083674 into filter state DAT_fff930b8; afterwards filters through PTR_FUN_00083670
with the per-bus parameter (PTR_PTR_0008366c + DAT_fff90ab8). Caller 0x0007B896 (r04_boot).
Single-member cluster. [O] mechanics, [I] role — feeds the monitor channel models below.

#### 0x0008341C `mon10_raw_set` — data-accessor
Stores a u16 into channel array DAT_fff93360[param_1] (index < 10). Caller 0x0007B896.
Single-member cluster. [O]

#### 0x00083430 `mon_stage_gate_set` — stub
Sets gate boolean DAT_fff9413f = (param != 0). Caller 0x0007B896. Single-member cluster. [O]

#### 0x0008343E `mon10_channels_reset` — logic
Resets the 10 monitor channels: raw array DAT_fff93360[i] = 0xFFFF, enable DAT_fff939f6[i]
= 1, and copies two u16s from ROM DAT_00083678 into DAT_fff93a0a/0a0c. Caller 0x0007CB90.
Single-member cluster. [O]

#### 0x0008347C `mon10_channel_check` — logic
Per-channel plausibility: when enabled and raw > 39999, computes two per-bus model results
via PTR_FUN_00083680 (tables PTR_PTR_0008367c/83684, inputs DAT_fff93a06 and clamped
param_2/DAT_fff94ed0), takes the minimum, rate-limits through 0x000835C2 and compares the
delta and the filtered value against per-bus thresholds (PTR_PTR_0008368c/83690), setting
fault bits in DAT_fff93374/93378; stores filtered value DAT_fff9334c[i]. Caller 0x00089648
(mon_input_acquire, unit 13). Single-member cluster. [O] mechanics, [I] meaning.

#### 0x000835C2 `mon10_rate_limit` — logic
Slew-rate/validity filter for one channel: on first pass initialises flags; compares the
raw step against the per-bus limit (PTR_PTR_00083698); marks validity DAT_fff93950/947c2
and either zeroes the output or applies the per-bille increment (PTR_PTR_000837a4); state
in DAT_fff930d0/930bc. Caller 0x0008347C. Single-member cluster. [O] mechanics, [I].

#### 0x0008370C `mon_stage_select3` — logic
For channels 0-2 (when gate DAT_fff9413f is clear) selects a staged u16 from ROM tables
(PTR_PTR_000837a8/000837b0/PTR_DAT_000837ac/000837b4) by thresholding DAT_fff93a06,
stores it via param_2 and DAT_fff93a08[i], and flags the change bit in DAT_fff93a04.
Caller 0x00089648 (unit 13). Single-member cluster. [O] mechanics, [I].

#### 0x0008379C `mon_result_one` — stub
Sets *param_2 = 1 and returns 0. Caller 0x00089648. Single-member cluster. [O]

#### 0x000837B8 `mon_case_latch` — logic
Latches monitor-case setup: DAT_fff9410b = 1, gate DAT_fff9410a = DAT_fff94057, and the
channel-enable mask DAT_fff93390 from dataset bytes DAT_fff84640/41 (dataset-transfer
buffer area, unit 10). Caller 0x0007CB90. Single-member cluster. [O]

#### 0x000837E0 `mon_curve_eval_chan` — logic
Per-channel curve evaluation: when the case is latched (DAT_fff9410b), the channel's mask
bit is in DAT_fff93390, DAT_fff940fc > 5 and param_2 != 0, evaluates
PTR_FUN_000838ac(offset into the dataset-13 buffer at DAT_fff84642.., 3, 4,
DAT_fff940ff, scaled param_2) and stores (result & 0xff)*2 - 0x100 into DAT_fff9337c[ch].
Caller 0x00089540 (unit 13). Single-member cluster. [O] mechanics, [I].

#### 0x000838B0 `mon10_commit_loop` — logic
Ten-channel commit pass under the lock PTR_FUN_000839f8(&LAB_00045ec0): one-shot flag
DAT_fff9410c, then per channel reads DAT_fff94ed4[i], calls hook PTR_FUN_000839fc(i) and
FUN_0008390C(i, 0, value, DAT_fff9340c[i]). Caller 0x0007CBA8. Single-member cluster. [O]

#### 0x0008390C `mon_chan_calc_filtered` — logic
Per-channel float computation: interpolates two per-bus factors via PTR_FUN_00083a04
(tables PTR_PTR_00083a00/00083a08 by DAT_fff93a10 / DAT_fff94fd6), computes
((const - param_4)*-A + param_4*DAT_fff94ed0[param_2]) * B / (limit DAT_fff933a8[i] *
const), clamps to [0, DAT_00083a14], stores DAT_fff930e4[i] and the average with the old
value into DAT_fff93394[i]. Caller 0x000838B0. Single-member cluster. [O] mechanics, [I]
— current derating against the thermal limit of 0x00083ABC.

#### 0x00083A1C `mon10_thermal_step` — logic
Staged periodic driver of the thermal model 0x00083ABC under lock PTR_FUN_00083cdc
(0x45E5C): state DAT_fff9410e; after start it advances one channel every 400 ticks
(DAT_fff9476e) with ramp flag DAT_fff84a21, fast mode runs all ten per call. Caller
0x0007CBA8. Single-member cluster. [O]

#### 0x00083ABC `mon_chan_thermal_model` — logic
Per-channel thermal/power model: input power term f = interp(curve by DAT_fff94fd6) *
value² / limit² (value = DAT_fff94ed4[ch]); two exponential-lag state components
(DAT_fff933d0/933e4) blended with per-bus factors (PTR_PTR_00083d04/00083d08), sum clamped
to DAT_00083d10 into DAT_fff933bc; recomputes the channel temperature limit DAT_fff933a8
from a linear model clamped by DAT_00083d1c; calls 0x00083CA8 to blend the shared
filter pair. Caller 0x00083A1C. Single-member cluster. [O] mechanics, [I] thermal role
(spec 12 solenoid loading).

#### 0x00083CA8 `mon_filter_pair_blend` — logic
Blends the complement pair DAT_fff94e4e/94e4c: new value = per-bus factor * ~old state
(DAT_00083da0 scale) or the base term; writes both via PTR_FUN_00083da8 and refreshes
DAT_fff94fd6/DAT_fff94fd4. Caller 0x00083ABC. Single-member cluster. [O] mechanics.

#### 0x00083D94 `mon_unreg_45e5c` — stub
Single call PTR_FUN_00083dac(0x45E5C, 2) → 0x00086FCA (lock/registry release of the same
id used by 0x00083A1C). Caller 0x0007CB9C. Single-member cluster. [O]

#### 0x00083DB0 `mon10_chan_driver` — logic
The Level-1 ten-channel monitor driver (unit 13 calls it "per-channel driver"): gated by
per-bus enable masks, evaluates per-channel state (codes 0x55/0x11/0x16/0x33), value caps
(0x2FFA, complement sentinel 0xD005) and state codes 0-4, maintains value/state/complement
triples (DAT_fff94ed4/94fd8/94f58 with 0xFE..0xFB complement state bytes), latches window
bits DAT_fff9397c/93978, runs the per-channel hooks PTR_FUN_00084324/84328/8432c/84334/
84338/8433c, and reports flow-tag integrity violations as event 0x226 via
PTR_FUN_00084330 with the marker address in DAT_fff90998. Caller 0x0007CC1E. Single-member
cluster. [O] structure, [I] meaning.

#### 0x0008422E `mon_speed2_filter` — logic
Filters the global value DAT_fff94ed0 twice (channels 0/1) with per-bus rates
(PTR_PTR_00084344): first pass converts (x,10000) via PTR_FUN_00084348 into
DAT_fff930f8[i], later passes run the PT1 filter 0x000842BA; results in DAT_fff93100[i].
Caller 0x00083DB0. Single-member cluster. [O]

#### 0x000842AA `mon_hook_89a8c_fwd` — stub
Forwards its argument through DAT_0008434c to 0x00089A8C (mon_ratio_eval, unit 13).
Caller 0x000838B0. Single-member cluster. [O]

#### 0x000842B0 `dtc_cond_9e3_report` — stub
Calls dtc_report_cond_9e3 (0x0007BA06, unit 10) with mask 0x3FFF and mode 9. Caller
0x0007CBDC. Single-member cluster. [O]

#### 0x000842BA `lib_pt1_filter_i` — library
Integer PT1/lag filter: keeps a state int, computes the difference to param_2 against the
scaled old state (consts DAT_00084354/84358/8435c), applies rate param_3 symmetrically,
and returns the scaled new state. Caller 0x0008422E. Single-member cluster. [O]

#### 0x00084392 `mon10_chan_driver_b` — logic
Monitor (shadow) twin of 0x00083DB0: same state machine but writing the shadow variables
DAT_fff94d4c/94e50/94dd0 from shadow input DAT_fff94d60, with per-bus gates
(0x00084584..0x000845a8, 0x0008479c/847a0/847a4) and window bits DAT_fff9397e; ends in
hook PTR_FUN_000847a8 (counter step, unit 13 0x00089718). Caller 0x00083DB0. Single-member
cluster. [O] structure, [I] — independent computation for comparison in 0x00084644.

#### 0x00084644 `mon10_chan_compare` — logic
Level-2 comparison for one channel: reads main state DAT_fff94e50[ch], value
DAT_fff90244[ch], shadow complement uVar15 = ~DAT_fff94d4c[2i]; re-derives the conditions
(state codes 'D','4','8','?','\"','3',0x11,0x16,'U' with the same per-bus gates) and
decides fault flag DAT_fff93980[ch]; on disagreement with the running flag reports event
0x226 via PTR_FUN_000849c4 with marker &LAB_0003dcc0. Caller 0x00083DB0. Single-member
cluster. [O] structure, [I].

#### 0x0008493A `mon10_shadow_to_main` — logic
Copies five value/state pairs from the shadow set to the main set per channel via the
hooks PTR_FUN_000849c8/000849cc (DAT_fff94d60→94ee8, 94d4c→94ed4, 94e18→94fa0,
94dd0→94f58, 94e50→94fd8), then the release hook PTR_FUN_000849d0(&LAB_0003dcb6 + ch).
Caller 0x00083DB0. Single-member cluster. [O]

#### 0x000849D4 `mon5_chan_driver` — logic
Five-channel variant of the complement-verified driver architecture: per-channel state
(codes 0x55/0x11/0x16/0x33/0x44), input classes bVar11 (0xFF/0xFE special windows vs
per-bus thresholds DAT_fff94f9e/94f86), value source DAT_fff94faa[ch] or
DAT_fff939aa[2i] (capped 0x2FFA), result state 0/1/2 into DAT_fff95000/94ffb, window
masks DAT_fff94111/94112/94113, complement loop counter verified against ~cStack_3c with
event 0x226 via PTR_FUN_00084d94. Caller 0x0007CC32. Single-member cluster. [O] structure,
[I] meaning.

#### 0x00084D7A `dtc_report_83c_4` — stub
Calls 0x0007BA96 (r04_boot report wrapper feeding dtc_queue_event) with (0x83C, 4).
Caller 0x0007CBDC. Single-member cluster. [O]

#### 0x00084DA0 `mon5_chan_driver_b` — logic
Shadow twin of the 5-channel driver 0x000849D4: same gate structure (per-bus tables
0x0008503C..0x00085064), input classes 0xFF/0xFE with threshold windows (DAT_fff94e16
complement, DAT_fff94f86), shadow states -1/-2/-3 written to DAT_fff94e38/94e78/94e6e/94e73,
window mask DAT_fff84a24, then commits via 0x000850A4 and hook PTR_FUN_00085068, ending
with the flow-tag call PTR_FUN_00085144(ch + 0x3DA5E). Caller 0x000849D4. Single-member
cluster. [O] structure, [I].

#### 0x000850A4 `mon5_shadow_to_main` — logic
For channels whose per-bus type byte != 3, copies the shadow trio (DAT_fff94e22 →
DAT_fff94faa, 94e6e → 94ff6, 94e73 → 94ffb) through hooks PTR_FUN_0008514c/00085150 and
runs the commit hook DAT_00085154. Caller 0x00084DA0. Single-member cluster. [O]

#### 0x00085160 `seq_state_machine` — logic
Complement-checked sequencer over the pair DAT_fff9500c/94e84 (and 9500d/94e85): reads the
flow state ('L', 0, '.', '=', 0x1F); each branch writes value/complement codes (0x2E/0xD1,
0x1F/0xE0, 0x3D/0xC2, 0x4C, 0xAA/0x55) via PTR_FUN_000853ec/000853f4/00085654, gates on the
speed threshold DAT_fff907fc (per-bus) and flags DAT_fff90959/95010/94151, publishes group
codes (0x33/0x22/0x16/0x44/0x23/0x12) through 0x00085600, runs the 0xAA/0x55 handshake via
0x00085788, and calls the guarded sequencer 0x0008B7A8 (unit 13). Epilogue 0x00085740
under lock PTR_FUN_000853d4(&LAB_0003d860). Callers 0x0008570E, 0x0007CC66. Single-member
cluster. [O] structure, [I] meaning.

#### 0x0008556E `seq_chan_masks_build` — logic
Classifies the two channels by their per-bus type byte (PTR_PTR_00085670) into three masks
(type 2 / type 1 / other) and writes each with its complement via PTR_FUN_00085674 into
the pairs DAT_fff94f70/94de8, 94f6e/94de6, 94f6c/94de4. Caller 0x00085678. Single-member
cluster. [O]

#### 0x00085600 `seq_group_code_send` — logic
For group param_1 (0/1/2): reads the channel mask pair DAT_fff94f6c+2i (complement-checked
via PTR_FUN_0008565c) and delivers the code param_2 to each enabled channel through
PTR_FUN_00085660. Callers 0x00085160, 0x00085678, 0x0008B974, 0x0008BB8A (unit 13).
Single-member cluster. [O]

#### 0x00085678 `seq_init_11` — logic
Sequencer init: clears the state pair 9500c/94e84, rebuilds the channel masks (0x0008556E),
sets DAT_fff94153 = 0x11, sends code 0x11 to all three groups, arms DAT_fff9414d from a
per-bus byte, resets counter pair 94fe2/94e5a (1/0xFFFE), state pair 9500d/94e85 (0/0xFF),
runs DAT_00085774 and re-enables DAT_fff95005 = 1 with DAT_fff94e7d = 0xFE, clearing
DAT_fff93a84/93a86. Caller 0x0007CBDC. Single-member cluster. [O]

#### 0x0008570E `seq_poll` — stub
Dispatches the sequencer tick: when the idle flag DAT_fff93b85 is clear calls the idle
hook PTR_FUN_00085778 (twice, second with &LAB_0003d860), otherwise runs the state machine
0x00085160. Caller 0x0007CC76. Single-member cluster. [O]

#### 0x00085740 `seq_flags_publish` — stub
When the poll hook PTR_FUN_00085780 reports pending, publishes the flag words
(DAT_fff93a86, DAT_fff93a84) through PTR_FUN_00085784. Caller 0x00085160. Single-member
cluster. [O]

#### 0x00085788 `seq_aa55_handshake` — logic
0xAA/0x55 start/stop handshake: with DAT_fff94153 == 0x33 sets bit 0x20 once (or calls
PTR_FUN_00085818); DAT_fff94152 == 0xAA → start via PTR_FUN_0008581c (set bit 4), == 0x55 →
stop via PTR_FUN_00085820 (set bit 0x10, clear 4, reset the byte); 0x11 group performs the
same dance on DAT_fff94154. Caller 0x0007CC32. Single-member cluster. [O]

#### 0x00085810 `seq_hook7_call` — stub
Calls DAT_00085824(7). Caller 0x0007CBDC. Single-member cluster. [O]

#### 0x00085828 `seq2_chan_fsm` — logic
Two-channel guarded FSM: per channel reads the previous state DAT_fff94fca[ch] and state
DAT_fff94fd0[ch], runs the hooks PTR_FUN_00085aa4/85aac/85ab0/85ab4, transitions over the
state alphabet {0, 0x11, 0x16, 0x22, 0x33, '4', '8', '?', 'D', 'U', 'f'} with the
speed-window gates (DAT_fff94f96 vs per-bus tables), hold/timeout counters DAT_fff947ac/
947b0/947b2, classifies the result into DAT_fff939b8 (0-4) and sets window bits
DAT_fff94116/94118; the loop counter is complement-verified with event 0x226 (marker
&LAB_0003d98c via PTR_FUN_00085ea8); channels >= 2 are forced to state 0x22 via
PTR_FUN_00085e9c. Caller 0x0007CC56. Single-member cluster. [O] structure, [I] meaning.

#### 0x00085D50 `seq2_thresh_reload` — logic
Reloads the per-channel hold thresholds DAT_fff947ac/947ae from the per-bus table pair
(PTR_PTR_00085eac/DAT_00085eb8) and reports (0x1EF, 1) via PTR_FUN_00085eb4. Caller
0x0007CBDC. Single-member cluster. [O]

#### 0x00085DAC `seq2_type_get` — stub
Runs hook PTR_FUN_00085ec0(param) then returns the channel type DAT_fff93a92[param].
Caller 0x0008C250 (unit 13). Single-member cluster. [O]

#### 0x00085DC4 `seq2_state_get` — data-accessor
Complement-checked read of the channel state pair DAT_fff94fd0/DAT_fff94e48 (via
lib_diverse_read_1b_chk 0x0006BDCA) for channels 0/1; 0 otherwise. Twelve callers across
the monitor families (0x00083DB0, 0x00084392, 0x000849D4, 0x00084DA0, unit 13 hooks).
Single-member cluster. [O]

#### 0x00085DE8 `seq2_state_raw_get` — data-accessor
Raw (unchecked) read of the complement byte DAT_fff94e48[ch] (0xFF for ch >= 2). Caller
0x0007B4EC (r04_boot). Single-member cluster. [O]

#### 0x00085E00 `seq2_state_set` — logic
Writes the state/complement pair for channel 0/1 with codes restricted to
{0x11, 0x16, 0x33, 0x22} via PTR_FUN_00085ec8; code 0x22 additionally loads the per-bus
hold byte into DAT_fff947b0[ch]. Callers 0x00085160, 0x00085600. Single-member cluster. [O]

#### 0x00085E64 `seq2_hold_set` — stub
Stores param_2 into DAT_fff939b6[ch] when the channel state is 0x11 or 0x16. Callers
0x0008B974, 0x0008BC54, 0x0008BEC0, 0x0008BFB8 (unit 13). Single-member cluster. [O]

#### 0x00085E82 `seq2_flag_reload` — stub
Reloads DAT_fff947b2[ch] from the per-bus table PTR_PTR_00085ed0. Caller 0x00086628.
Single-member cluster. [O]

#### 0x00085ED4 `seq2_result_compute` — logic
Per-channel result computation: when the hook state (PTR_FUN_00086114) is >= 0x22/0x33
with hold clear and DAT_fff94f96 inside the per-bus window, derives the result mask from
0x00086628; ANDs current and candidate with the per-bus mask (PTR_PTR_00086120), stores
DAT_fff9495c and merges set/clear bits into DAT_fff94958, then hooks PTR_FUN_00086124,
0x000865E0 and 0x00086586. Caller 0x00085828. Single-member cluster. [O] mechanics, [I].

#### 0x00085FC4 `seq2_phase_fsm` — logic
Per-channel phase machine on DAT_fff939be (phases 0, 1, 2, 3, 4, 5, 8, 9, -1, -2): phase 1
starts via 0x00086838(ch, 0x22, tbl…) and manages the output masks DAT_fff84a25-28 plus
result words DAT_fff93118/9311a (bits 0x20/0x40/0x80/0x100); phase 2 issues 0x11; phase 4/5
gate on mask overlaps; phase 8 ANDs the words with a per-bus mask and recomputes 9495c/
94958; phase 9 checks completion; caller-supplied per-bus tables throughout, progress bytes
DAT_fff9411b/9411c, bit updates in DAT_fff94115. Caller 0x00085828. Single-member cluster.
[O] structure, [I].

#### 0x000863D8 `seq2_counters_reset` — stub
Zeroes the two-byte arrays DAT_fff939b4[] and DAT_fff93120[]. Caller 0x00085678. Single-
member cluster. [O]

#### 0x00086414 `seq2_cnt_incr` — stub
Increments DAT_fff939ba[ch] unless it already holds 0xFF. Callers 0x00089F5C, 0x0008B284
(unit 13). Single-member cluster. [O]

#### 0x0008642C `seq2_cnt_clear` — logic
For each of the two channels: when DAT_fff939ba[ch] is below the per-bus limit clears
DAT_fff939bc[ch], then resets both to zero. Caller 0x0007CC1E. Single-member cluster. [O]

#### 0x0008646C `seq2_timeout_eval` — logic
Per channel: when the state is 'U' with hold clear, DAT_fff94f96 inside the per-bus window,
result bits (9495c & 94958 & 0x48) != 0x48 and per-bus bit 0x80 set, latches the bit into
DAT_fff9411a and debounces counters DAT_fff939ba/bc against per-bus limits into
DAT_fff94119. Caller 0x0007CC32. Single-member cluster. [O] mechanics, [I].

#### 0x00086586 `seq2_class_report` — logic
Classifies the per-channel result (param_2 & param_3) against the per-bus mask
(PTR_PTR_000866ec): none → 0, match → 2, match with bits 0x1E0 → 0x82, stored into
DAT_fff939c4[ch]; reports (2, ch, ch, class, value) through PTR_FUN_000866f0. Callers
0x00085FC4, 0x00085ED4. Single-member cluster. [O]

#### 0x000865E0 `seq2_mask_latch` — logic
Latches the 0x1E0 bit group: new = old DAT_fff93120[ch] & 0x1E0 | (*p & p[1] & per-bus mask
& 0x1E0), writes it back and ORs it into both param_2 words. Callers 0x00085FC4, 0x00085ED4.
Single-member cluster. [O]

#### 0x00086628 `seq2_bitmap_eval` — logic
Evaluates the state bitmap for one channel by hook state param_2: '"'-states build
0xE|classes, '4'/'8'/'?'/'D' build bits 4/0xC with per-bus OR 939c0, 'U' checks the
speed/slip window (DAT_fff94f96, DAT_fff94fed, DAT_fff940fa vs per-bus tables
0x000866F8/0x000868E0/0x000866FC) and sets bits 8/0x40, with result classes 0/2/4/8 in
param_3[1]; finally ORs 0x80 flags from the debounce words DAT_fff9411a/94119 into both
bytes. Caller 0x00085ED4. Single-member cluster. [O] mechanics, [I].

#### 0x00086838 `seq2_arb_step` — logic
Two-byte arbitration FSM per channel (state DAT_fff939c2, countdown DAT_fff947b4, phases
0-4): phase 0/1 latch the two caller bytes, phase 2 compares param_2 with the channel type
DAT_fff93a92 returning -1 on match or the type byte, always returns -0x10 while not
decided; runs the hook PTR_FUN_000868ec first. Caller 0x00085FC4. Single-member cluster. [O]

#### 0x000868F0 `proc0_start_once` — logic
Returns 1 exactly once (when busy flags DAT_fff94816/94817 and phase DAT_fff9411e are all
clear), then latches DAT_fff9411e = 1. Caller 0x0007ACF2 (r04_boot). Single-member
cluster. [O]

#### 0x00086916 `proc0_phase_fsm` — logic
Phase machine on DAT_fff9411e (1,2,3,4,5,6,7,8,9,10): phases start/stop two operations via
PTR_FUN_00086a70(0/1) + PTR_FUN_00086a74 with the busy flags DAT_fff94815/94816/94817
loaded from per-bus config bytes (PTR_PTR_00086a6c/78/7c); 1→2→3→5→4→0 (off path) and
6→7→10 / 9→8 (alternate paths). Caller 0x00086A80. Single-member cluster. [O]

#### 0x00086A80 `proc0_driver` — logic
Driver for the phase machine: under the guard PTR_FUN_00086ae0(&PTR_LAB_0003d478, ...) with
pre/post hooks PTR_FUN_00086ae4/86aec, checks the enable condition PTR_FUN_00086ae8 and
switches DAT_fff9411e between paths 9/6 (running) and 10/7 (idle), then runs 0x00086916.
Caller 0x0007CC32. Single-member cluster. [O]

#### 0x00086ACE `hook_call_1c_7c662` — stub
Calls DAT_00086af0(0x1C) → 0x0007C662 (r04_boot). Caller 0x0007CBDC. Single-member
cluster. [O]

#### 0x00086AD4 `unreg_3d478_2` — stub
Calls PTR_FUN_00086af4(&PTR_LAB_0003d478, 2) → 0x00086FCA (trace fill). Caller 0x0007CC06.
Single-member cluster. [O]

#### 0x00086AF8 `thunk_FUN_0008cab4` — stub
Indirect thunk through DAT_00086b04 → 0x0008CAB4 (unit 13). Caller 0x0007CC32. The
original symbol name is retained. Single-member cluster. [O]

#### 0x00086AFE `hook_call_0c_8877c` — stub
Calls DAT_00086b08(0xC) → 0x0008877C. Caller 0x0007CBDC. Single-member cluster. [O]

#### 0x00086B0C `proc_post_run_hooks` — logic
One-shot after gate DAT_fff9095b > 14: runs hooks PTR_FUN_00086b48 (when DAT_fff9411f
already set), PTR_FUN_00086b4c and PTR_FUN_00086b50, then latches DAT_fff9411f = 1.
Callees 0x0008CE48, 0x0008CB7C, 0x0008CCD4 (unit 13). Caller 0x0007CC32. Single-member
cluster. [O]

#### 0x00086B6C `tbl_pattern_fill` — logic
Fills buffers from an 8-byte-stride descriptor table {count, fill byte, u16 max, ...}: for
each entry writes param_1[1] into the current buffer (obtained via PTR_FUN_00086d64) until
the entry count, tracks totals in DAT_fff94123/939c6/939c8 with a global cap 0x2FF entries
and overflow flag DAT_fff84a29. Callers 0x0007CD30/3E/4C/5A and 0x00086C68. Single-member
cluster. [O] mechanics, [I] test/measurement pattern fill.

#### 0x00086C68 `dset_blk_fsm_a` — logic
Descriptor-driven block processor with state DAT_fff84a2a (1 setup / 2 copy / 3 pattern /
4 done): setup stores the RAM block DAT_fff84818-0x84828 (mode byte, u16 params, cursors,
pointers), starts via PTR_FUN_00086d6c and re-reads the descriptor; state 2 copies
*(u16*)(param_1+6) bytes src→dst; state 3 runs the pattern fill 0x00086B6C; returns 1
while busy. Callers 0x0007CDA0-0x0007D104 (r04_boot dataset handlers). Single-member
cluster. [O] mechanics, [I] dataset-transfer block handler (unit 10 family).

#### 0x00086D70 `dset_blk_fsm_b` — logic
Variant gated on !(descriptor[0] & 0x40): sets up the block DAT_fff8482c-0x84838, starts it
via PTR_FUN_00086de8 into DAT_fff91b86, sets status DAT_fff91b84 = 2 until the cursors
(DAT_fff84830/32) meet; the 0x40 path copies *(u16*)(param_1+2) bytes and reports done.
Callers 0x0007CE48-0x0007D140 (r04_boot). Single-member cluster. [O] mechanics, [I].

#### 0x00086DEC `lib_fill_words` — library
Fills param_2/4 words at param_1 with param_3. Callers='-'. Single-member cluster. [O]

#### 0x00086DFC `lib_bitpair_write` — logic
Complement-pair bit update on DAT_fff94fd2/DAT_fff94e4a under guard PTR_FUN_00086e7c/8c:
mode 0 clears bits in the value and sets them in the complement (mode 1 the reverse),
writes both via PTR_FUN_00086e84 and reports integrity event 0x226 (code 0x92BA9) via
PTR_FUN_00086e88 on mismatch. Callers 0x0007E3F4, 0x0007E06C, 0x0007DCE8. Single-member
cluster. [O]

#### 0x00086E90 `trace_rec_begin` — stub
Begins a trace record: stores the write pointer DAT_fff92e3c, advances via
PTR_FUN_00087024 (0x0007EA48), writes the 0xFFFFFFFF terminator and returns the cursor.
Caller 0x00086FFE. Single-member cluster. [O]

#### 0x00086EAC `trace_groups_alloc` — logic
Allocates up to five trace groups by the selector bits of param_1: for each selected group
(base DAT_fff92e70/92ea4/92ed8/92f0c/92f40) computes the max word count from its table
(PTR_PTR_00087028-0x00087038) and reserves count*4+4 bytes from the buffer. Callers
0x0007A526 (r04_boot), 0x00086F5C. Single-member cluster. [O]

#### 0x00086F26 `trace_group_max_words` — library
For each of param_2 table pointers: measures the reserved size (via PTR_FUN_00087024 with
cursor reset DAT_fff92e3c) and returns the maximum in words. Caller 0x00086EAC. Single-
member cluster. [O]

#### 0x00086F5C `trace_usage_check` — logic
Computes the total trace/RAM usage: allocates groups 0 and 1 at DAT_fff93138 via
0x00086EAC, derives (end + 0x6CEC8)>>2 into the watermark DAT_fff93a96 and reports event
0x227 (with the overage encoded in DAT_fff90998) when it exceeds 0x78; closes the
allocation (PTR_FUN_00087040/44) and enables the recorder DAT_fff9406d = 1. Callers='-'.
Single-member cluster. [O] mechanics, [I] RAM/stack watermark check.

#### 0x00086FAC `trace_word_put` — stub
Writes one word to the trace cursor DAT_fff92e3c when the recorder is enabled
(DAT_fff9406d); always advances the cursor. Callers='-'. Single-member cluster. [O]

#### 0x00086FCA `trace_words_fill` — logic
Skips or fills param_2 words in the trace buffer: when the recorder is disabled just
advances the cursor, otherwise fills every word with param_1 (fill/skip idiom used as
guard-region release, e.g. by 0x00086AD4). Twelve callers across r08_app and r04_boot.
Single-member cluster. [O]

#### 0x00086FFE `trace_rec_log_wrap` — stub
Begins a record via 0x00086E90(param_1, *(param_2+0x30)) then hands the record to the
consumer hook PTR_FUN_00087048(ctx, ptr, param_3, param_4). Callers: fourteen r04_boot
sites (0x0007E510-0x0007E884). Single-member cluster. [O]

#### 0x0008701A `trace_enable_swap` — stub
Swaps the recorder enable flag DAT_fff9406d and returns the previous value. Caller
0x0007A526 (r04_boot). Single-member cluster. [O]

#### 0x0008704C `hook_228_report` — stub
Runs PTR_FUN_0008710c(*(ctx+0x28)) then reports event 0x228 via PTR_FUN_00087110.
Caller 0x0007EE48 (r04_boot). Single-member cluster. [O]

#### 0x00087060 `dtc_env_ctx_switch` — logic
Fault-environment context switch: when the ctx state byte (+0x11) equals the active slot
DAT_fff9412c, closes it (PTR_FUN_00087114) and decrements the nesting counters DAT_fff9387a/
9406a/94069 per the ctx flags (+0x2a/+0x2b); submits the record through 0x000870E0, reopens
via PTR_FUN_00087118 and increments the counters. Caller 0x00086FFE (the r04_boot trace
loggers route through this). Single-member cluster. [O] mechanics, [I] fault-env capture.

#### 0x000870E0 `dtc_env_submit` — stub
Submits the record via PTR_FUN_0008711c(ctx, param_3, param_4, param_2 ^ param_1) and marks
the ctx state byte (+0x17) = 2 plus bit 2 in +0x10. Caller 0x00087060. Single-member
cluster. [O]

#### 0x00087120 `fault_entry_set_state` — logic
Fault-memory entry writer over five 0x20-stride groups (count PTR_DAT_000873b4, table
PTR_PTR_000873b8, index param_2): validates the class (-2..7) else event 0x224; checks the
per-entry value/complement word pair (event 0x226, code idx+0x98968 in DAT_fff90998);
arbitrates old vs new class (1-4) and writes the new class/status word pair plus the entry
byte and u16, clamped by the per-bus cap (PTR_PTR_000873c0); re-verifies the complement.
Callers 0x000873C8, 0x00086586, 0x00089F5C, 0x0008B284 (unit 13). Single-member cluster.
[O] structure, [I] meaning (spec 13).

#### 0x000873C8 `fault_entry_set_simple` — stub
Maps the caller's mode byte to class nibble (param_3 & 0xF8 | 4) and forwards to
0x00087120(p1, p2, 0, class, p4). Caller 0x000877C0. Single-member cluster. [O]

#### 0x000873F0 `fault_manager_poll` — logic
Central fault-manager tick: advances the complement cycle counter pair DAT_fff9500f/94e87
(event 0x223 when the cycle delta DAT_fff934c0 exceeds *PTR_PTR_000876b8), drives the state
byte DAT_fff95010/94e88 (states 0/'E'/8/0x12/0x23/0x34) with hooks PTR_FUN_000876c4-876e0,
timeout re-arms via PTR_FUN_000876dc(&LAB_00001838, &DAT_0000e7c7, 2), verifies the counter
pair complement (event 0x220 via PTR_FUN_0008795c), reads the confirmed mask pair
DAT_fff94f84/94dfc and hands both to 0x000877C0. Callers='-'. Single-member cluster. [O]
structure, [I] (spec 13).

#### 0x00087732 `fault_manager_init` — init
Init of the fault manager: hooks PTR_FUN_00087964-00087978, sets the sub-state pair
DAT_fff9500e/94e86 = 1/0xFE, arms the boot slot via 0x00087B08 and registers
&LAB_00001dce_1. Callers='-' (init table). Single-member cluster. [O]

#### 0x00087766 `fault_state_write` — logic
Writes a new state byte into the pair DAT_fff95010/94e88 (latching the previous into
DAT_fff93b93); codes 0x23/0x12/0 also reset the sub-state pair DAT_fff9500e/94e86 to
1/0xFE. Guarded by PTR_FUN_00087980/00087988. Caller 0x00085160. Single-member cluster. [O]

#### 0x000877C0 `fault_debounce_eval` — logic
Twelve-entry fault debounce engine (8-byte table via PTR_DAT_0008798c, counters at
DAT_fff93826[2i], hold bytes +1): counts up on new conditions (saturating 0xFF), raises
event 0x223 on threshold+flag 0x200, latches condition/confirmed masks DAT_fff93a98/93a9a
(0 = released when cleared and count 0), calls 0x00087A56 on new faults, and reports the
masks via PTR_FUN_00087994 when the manager allows; finally publishes (3, 2, 2,
&DAT_0000a5a5) when flag 0x100 entries changed. Caller 0x000873F0. Single-member cluster.
[O] structure, [I].

#### 0x0008799C `fault_state_advance` — logic
State advance helper: on state 0x23 with sub-state 0 reads the pair via PTR_FUN_00087a94,
forces 0x45/0xBA (complement) with flag DAT_fff93b99 = 1 and re-arms the timeout
(&LAB_00001838); otherwise runs PTR_FUN_00087a98/00087a9c/00087aa0. Caller 0x000873F0.
Single-member cluster. [O]

#### 0x00087A56 `fault_mask_expand` — logic
Expands the raw condition bits (param_1) through a local 13-entry u16 table into the
latch word DAT_fff939ce under guard PTR_FUN_00087aa8. Caller 0x000877C0. Single-member
cluster. [O]

#### 0x00087AAC `fault_boot_record_read` — logic
One-shot read of the boot fault record: if the RAM magic DAT_fff800e4 == 0x5555 latches
DAT_fff9384a/b from DAT_fff800e8/e9 (returns -1); on later polls returns the latched value
only while DAT_fff90955 is set. Caller 0x00087732. Single-member cluster. [O] mechanics,
[I] boot-passed reset/fault info (spec 06 §5a area).

#### 0x00087B08 `fault_boot_slot_arm` — stub
Clears DAT_fff800e8/e9 and writes the magic 0x5555 into DAT_fff800e4 — arms the RAM slot
consumed by 0x00087AAC. Caller 0x00087732. Single-member cluster. [O]

#### 0x00087B1C `mon_env_min_eval` — logic
Evaluates the graded minimum: gates the two interpolation inputs (DAT_fff9393c/9393e) on
the quality counters DAT_fff940fd/94100 vs per-bus limits, interpolates both via
PTR_FUN_00087c44 with per-bus factors, takes the min into DAT_fff939d6 and grades
DAT_fff9412d (1 valid / 2 invalid / 0 none). Callers='-'. Single-member cluster. [O]
mechanics, [I] — the value source behind signal 0x29 (0x00082DD4).

#### 0x00087BBC `mon_env_value_read` — logic
Reads the monitored input DAT_fff939d8 through per-bus accessor PTR_FUN_00087c54 — one
variant when the selector DAT_fff9412f is clear, otherwise the minimum of two; also mirrors
DAT_fff94f86 → DAT_fff939da and DAT_fff939f4 → DAT_fff939dc. Callers='-'. Single-member
cluster. [O]

#### 0x00087CB8 `uds_19_read_dtc_bytes` — logic
ReadDTCInformation sub-handler (shares the 0x19 in-progress flag DAT_fff94085 with
0x00081054): len must be 2 (else NRC 0x13); phase 0 echoes the mask, starts the read via
PTR_FUN_00087f14 and latches the record count DAT_fff939e6; phase 2 collects 4-byte records
via PTR_FUN_00087f1c into the response, switching pages (grow via thunk 0x0006C8E6, NRC
0x14 on overflow, suppress bit handling at the end); phase 1 zero-fills the remaining
slots. Callers='-'. Single-member cluster. [O]

#### 0x00087F6C `uds_2a_read_periodic` — logic
Periodic-identifier read handler (request len 5): parses the DID/mode via PTR_FUN_00088058,
starts via PTR_FUN_0008805c, computes the length byte (0xFF when max >= 0x104) and
transfers the data via PTR_FUN_00088060(did, 1, 0, tag, req+5, &len), mapping failures
{1,4}→NRC 0x31, 5→0x14, 6→0x13, else 0x22; success copies the 3-byte DID and len. Callers
='-'. Single-member cluster. [O] mechanics, [I] 0x2A identification.

#### 0x00088068 `uds_19_read_dtc_frames` — logic
Twin of 0x00087CB8 with its own state set (DAT_fff939e8/939ea/939ec, phase bytes
DAT_fff94134/94135) — the second ReadDTCInformation sub-function: same phase 0/2/1
record-collection engine (4-byte records via PTR_FUN_000882f4, paging via
PTR_FUN_000882f8, NRC 0x13/0x22/0x14) and the shared in-progress flag DAT_fff94085.
Callers='-'. Single-member cluster. [O]

#### 0x0008839C `dtcblk_ptr_93324` — data-accessor
Returns &DAT_fff93324 — the 0x18-byte clear-DTC record buffer written by handler 0x0008086C.
Caller 0x0008086C. Single-member cluster. [O]

#### 0x000883A2 `dtcblk_93324_reset` — logic
Clears the record fields DAT_fff93324/28/2c/30/38/3a/3b and masks DAT_fff93334 to 0x3F.
Callers='-'. Single-member cluster. [O]

#### 0x000883C4 `dtc_tmr_93340_latch` — stub
Latches the task timer DAT_fff90628 into DAT_fff93340. Callers 0x0008843E, 0x0008845A.
Single-member cluster. [O]

#### 0x000883D2 `dtc_tmr_9333c_latch` — stub
Latches DAT_fff90628 into DAT_fff9333c. Caller 0x0008086C. Single-member cluster. [O]

#### 0x000883E0 `dtc_tmr_93340_expired` — logic
Returns 1 unless param_1 is 0xFFFFFFFF or the elapsed time (DAT_fff90628 - DAT_fff93340)
is below it — timeout check against the 0x93340 latch. Callers='-'. Single-member cluster.
[O]

#### 0x00088400 `dtc_tmr_9333c_expired` — logic
Timeout check against the 0x9333c latch (same shape as 0x000883E0). Caller 0x0008086C.
Single-member cluster. [O]

#### 0x00088420 `dtc_latch_94094_fwd` — stub
Forwards param_2 through PTR_FUN_0008847c → 0x00081FA0 (diag_flag_94094_latch). Callers
0x000A9D66, 0x000A97CA (other regions). Single-member cluster. [O]

#### 0x00088426 `dtc_status_notify_44` — stub
When the status byte (via PTR_FUN_00088480 → 0x00081F8C, i.e. DAT_fff94093) is 0x11, sets
the status to 0x44 via PTR_FUN_00088484 (0x00081F84). Callers 0x000A9D66, 0x000A97CA.
Single-member cluster. [O]

#### 0x0008843E `dtc_status_notify_22` — stub
Status 0x11 → latches the task timer (0x000883C4 → DAT_fff93340) and sets the status to
0x22. Caller 0x000A97CA. Single-member cluster. [O]

#### 0x0008845A `dtc_status_notify_cnt` — stub
Status 0x33 → adds param_1 to the counter (0x00081F94 → DAT_fff9388a), latches the timer
via 0x000883C4 and sets the status 0x22. Caller 0x000A9D66. Single-member cluster. [O]

#### 0x0008848C `uds_19_sub_handler_sm` — logic
ReadDTCInformation sub-handler (len 2, else NRC 0x13): reads the mask byte via
PTR_FUN_0008852c, starts the query via PTR_FUN_00088530(mask, 0, 0, 1, 0) (failure → NRC
0x22) and reads the count u16 via PTR_FUN_00088534; positive response is 5 bytes
{SID (DAT_fff94084), mask, config+0x15, count hi, count lo} with the usual suppress
handling. Callers='-'. Single-member cluster. [O]

#### 0x00088538 `lib_curve2d_tbl_eval` — library
Unpacks a nested curve-table header (count at param_1[0], x-array, y-count at the tail)
and evaluates the 2-D curve through PTR_FUN_00088568 (bilinear interpolation of the
0x0008856C shape; resolved target 0x0008D308). Caller 0x0008347C (mon10_channel_check).
Single-member cluster. [O]

#### 0x0008856C `lib_curve2d_interp` — library
Bilinear interpolation over a row-major u8 matrix with independent x/y breakpoint arrays
(param_2/len param_4 and param_3/len param_5), edge-clamped, float interpolation plus the
offset constant DAT_000886a8. Caller 0x000837E0. Single-member cluster. [O]

#### 0x000886AC `lib_curve1d_eval` — library
1-D curve lookup: table header {count}, x-array at +2, forwards (xs, ys, count, x) to the
interpolator PTR_FUN_000886c4 (→ 0x0008D42C). Callers 0x0008390C, 0x00083ABC, 0x00087BBC
and unit-13 hooks. Single-member cluster. [O]

#### 0x000886C8 `lib_satur_add_u16` — library
u16 add saturated at 0xFFFF. Caller 0x00089A8C (unit 13 mon_ratio_eval). Single-member
cluster. [O]

#### 0x000886DA `lib_clamp_add_s16` — library
s16 add clamped to [+0x7FFF, -0x8000]. Caller 0x00087B1C. Single-member cluster. [O]

#### 0x000886F4 `lib_pt1_filter_scale` — library
Integer PT1 filter with rounding: state = state + rate*(target - state/scale), returns the
scaled state with half-step rounding — the filter behind PTR_FUN_00083670 (0x000833C0).
Callers 0x000833C0. Single-member cluster. [O]

#### 0x00088748 `lib_mul` — library
Returns param_1 * param_2. Callers 0x000833C0, 0x0008422E, 0x00089A8C. Single-member
cluster. [O]

#### 0x00088750 `lib_satur_sub_u16` — library
u16 subtract saturated at 0. Callers 0x00084644, 0x0008A788, 0x0008C6D4. Single-member
cluster. [O]

#### 0x00088760 `lib_clamp_sub_s16` — library
s16 subtract clamped to [+0x7FFF, -0x8000]. Caller 0x0008347C. Single-member cluster. [O]

#### 0x00088792 `sig_occ_track_9cf` — stub
Records (0x9CF, 0, param_1, param_2) into the fault-occurrence tracker 0x0007CA24 via
PTR_FUN_000887a4. Cluster of 3 — identical pattern with different event ids:
0x000887C0 (`sig_occ_track_9f6`, caller 0x0008CCD4) and 0x0008D566 (`sig_occ_track_96a`,
caller 0x00088AE0). [O]

#### 0x000887D8 `sig_pub_1fc_status` — logic
Publishes the status signal code 0x1FC: reads the complement pairs DAT_fff94f9a/94e12,
94fef/94e67, 94f98/94e10, 94ff0/94e68, writes back the working values (DAT_fff93aa0,
DAT_fff94159) with the grade logic (6 < counter → special case, per-bus bit 4 overrides
with value+DAT_fff9393a), computes the two clamped value bytes DAT_fff94102/94103
(((x+8)>>4)+0x28, capped 0xFF), decodes the condition flags from DAT_fff93a9e (masks
0x809/0x828/0x87f → 4/8/0x10/0x20/0x40/0x80, plus 0x100 from DAT_fff94155) into
DAT_fff93ae8 and publishes via PTR_FUN_00088a4c under the lock &DAT_0003b920. Caller
0x00083300 (periodic hub). Single-member cluster. [O] mechanics, [I] signal identity.

#### 0x00088A50 `sig_1fc_state_reset` — logic
Resets the signal state used by 0x000887D8: clears the pairs DAT_fff94f9a/94e12 and
94f98/94e10 to (0, 0xFFFF), 94fef/94e67 and 94ff0/94e68 to (0, 0xFF) via hooks
PTR_FUN_00088ac8-00088ad8. Caller 0x000832AC (hub a). Single-member cluster. [O]

#### 0x00088ABE `trace_rel_3b920` — stub
Trace fill/skip (&DAT_0003b920, 2) via PTR_FUN_00088adc → 0x00086FCA. Caller 0x000832D4
(hub b). Single-member cluster. [O]

#### 0x00088AE0 `sig_pub_mode_pair` — logic
Under the lock &DAT_0003ad68: computes DAT_fff940fb = clamp(DAT_fff94f86 * const
DAT_00088b94, 0xFF), selects the two status values DAT_fff93aee/93aec (2 or 0) from the
mode bits DAT_fff93aac (& 0x18 / & 2) and the override flag DAT_fff9415b, and publishes
the pair via PTR_FUN_00088b9c. Caller 0x0008332C (hub d). Single-member cluster. [O]

#### 0x00088B74 `sig_hook_88b74` — stub
Runs hooks PTR_FUN_00088ba0() and PTR_FUN_00088ba4(2). Caller 0x000832AC (hub a).
Single-member cluster. [O]

#### 0x00088B82 `trace_rel_3ad68` — stub
Trace fill/skip (&DAT_0003ad68, 2) via PTR_FUN_00088ba8. Caller 0x000832F0. Single-member
cluster. [O]

#### 0x00088BAC `sig_reload_9393a` — stub
Runs hooks PTR_FUN_00088d6c() and PTR_FUN_00088d70(0x62), then reloads DAT_fff9393a from
the per-bus table PTR_PTR_00088d74. Caller 0x000832AC (hub a). Single-member cluster. [O]

#### 0x00088BD0 `sig_pub_60_status` — logic
Publishes the status signal 0x60/0x62 (DAT_fff93af2): selects the value source grade by
the per-bus selector byte (PTR_PTR_00088d80 bits 1/2/4 with counters DAT_fff94fee/9415e/
94100 → source flags DAT_fff94170, grade DAT_fff940fc 3/6), computes the clamped value
bytes DAT_fff940ff/940fe from DAT_fff9393a/DAT_fff93aba, decodes flags from DAT_fff93ab6
(masks 0x20/0xC4/0xC2/0xD0) into DAT_fff93af0 and publishes via PTR_FUN_00088d90 under the
lock &DAT_0003b9e8. Caller 0x00083300 (hub c). Single-member cluster. [O] mechanics, [I].

#### 0x00088D62 `trace_rel_3b9e8` — stub
Trace fill/skip (&DAT_0003b9e8, 2) via PTR_FUN_00088d94. Caller 0x000832D4 (hub b).
Single-member cluster. [O]

#### 0x00088D98 `sig_hook_160` — stub
Runs hooks PTR_FUN_00088e6c() and DAT_00088e70(0x160). Caller 0x000832AC (hub a).
Single-member cluster. [O]

#### 0x00088DA8 `sig_pub_160_status` — logic
Publishes the status signal 0x160 (DAT_fff93af6): computes the clamped value byte
DAT_fff94101 from DAT_fff9393e, decodes the flags DAT_fff93af4 from the condition word
DAT_fff93ac0 (masks 0x1041/0x1021/2/0x1101/0x1011 → bits 0x40/0x20/0x100) and publishes
via PTR_FUN_00088e80 under the lock &DAT_0003ba4c. Caller 0x00083300 (hub c). Single-member
cluster. [O] mechanics, [I].

#### 0x00088E62 `trace_rel_3ba4c` — stub
Trace fill/skip (&DAT_0003ba4c, 2) via PTR_FUN_00088e84 → 0x00086FCA. Caller 0x000832D4
(hub b). Single-member cluster. [O]

#### 0x00088E88 `sig_src_latch_907ea` — logic
Latches the source values DAT_fff939f2 = DAT_fff907ea and DAT_fff939f4 = DAT_fff907ec and
sets the two limits DAT_fff9413d/9413e = 8. Caller 0x00083300 (hub c). Single-member
cluster. [O]

#### 0x00088EB0 `sig_6ch_state_reset` — logic
Resets the six complement pairs DAT_fff94f8e/94e06 to (0, 0xFF) via PTR_FUN_000890e0,
then runs hooks PTR_FUN_000890e4 and DAT_000890e8(0x3E, 5). Caller 0x000832AC (hub a).
Single-member cluster. [O]

#### 0x00088EE6 `trace_rel_3bd08` — stub
Trace fill/skip (&DAT_0003bd08, 2). Caller 0x000832D4 (hub b). Single-member cluster. [O]

#### 0x00088EF0 `sig_pub_6ch_status` — logic
Six-channel status publisher under the lock &DAT_0003bd08: snapshots the six source bytes
DAT_fff93ad6 → DAT_fff93afe, computes the enable gate DAT_fff94171 (mode 8 + compare
clean), evaluates each channel against the selection tables PTR_DAT_000890f8/00089100 and
per-bus masks, class codes {0, 0x3E} into DAT_fff934dc and value codes {0, 8, 0x20, 0x10,
4, 2} into DAT_fff934d0, latches DAT_fff93af8 and rewrites the complement pairs via
PTR_FUN_00089110/000890e0. Caller 0x00083300 (hub c). Single-member cluster. [O] mechanics,
[I].

#### 0x00089114 `sig_pub_2ch_scaled` — logic
Two-channel scaled publisher under the lock &DAT_0003adcc: reads the pairs
DAT_fff94ed0/94d48, DAT_fff94f94/94e0c and DAT_fff94f96/94e0e, computes
DAT_fff93940[i] = clamp(DAT_fff94ed0[i] * const, 0xFF) and per-bus status codes
(1/0xFE normal, 8/0xF7 low, 7/0xF8 high per threshold tables), plus the variant selector
via PTR_PTR_00089318. Caller 0x0008332C (hub d). Single-member cluster. [O] mechanics, [I].

#### 0x00089288 `sig_2ch_state_reset` — logic
Resets the two channel pairs and the globals used by 0x00089114 (DAT_fff94ed0/94d48,
94f94/94e0c, 94f96/94e0e, 94fed/94e65) to (0, complement). Caller 0x000832AC (hub a).
Single-member cluster. [O]

#### 0x000892EE `trace_rel_3adcc` — stub
Trace fill/skip (&DAT_0003adcc, 2). Caller 0x000832F0. Single-member cluster. [O]

#### 0x00089320 `sig_src_latch_907e8` — stub
Latches DAT_fff93942 = DAT_fff907e8. Caller 0x00083300 (hub c). Single-member cluster. [O]

#### 0x00089330 `sig_pub_pair_b59c` — logic
Status publisher under the lock &DAT_0003b59c: clears DAT_fff93b04, reloads
DAT_fff94824 from per-bus config when mode bits DAT_fff93ade & 6 are set (else 0x0C when
the counter is clear), classifies DAT_fff93b04 = 4|8 from DAT_fff9416b when
DAT_fff93adc is below the per-bus threshold, and publishes (DAT_fff93b06, DAT_fff93b04)
via PTR_FUN_0008940c. Caller 0x00083300 (hub c). Single-member cluster. [O] mechanics, [I].

#### 0x000893CA `sig_hook_pair_b59c` — stub
Runs hooks PTR_FUN_00089410() and PTR_FUN_00089414(0x2C), then reloads DAT_fff94824 from
the per-bus table. Caller 0x000832AC (hub a). Single-member cluster. [O]

#### 0x000893EE `trace_rel_3b59c` — stub
Trace fill/skip (&DAT_0003b59c, 2). Caller 0x000832D4 (hub b). Single-member cluster. [O]

#### 0x0008941C `sig_pub_pair_b538` — logic
Twin of 0x00089330 under the lock &DAT_0003b538 with its own state (DAT_fff93ae4,
DAT_fff93ae2, DAT_fff94825, DAT_fff9416d → DAT_fff93b08/93b0a); publishes via
PTR_FUN_000894f8. Caller 0x00083300 (hub c). Single-member cluster. [O] mechanics, [I].

#### 0x000894B6 `sig_hook_pair_b538` — stub
Runs hooks PTR_FUN_000894fc() and PTR_FUN_00089500(0x6C), then reloads DAT_fff94825 from
the per-bus table. Caller 0x000832AC (hub a). Single-member cluster. [O]

#### 0x000894DA `trace_rel_3b538` — stub
Trace fill/skip (&DAT_0003b538, 2). Caller 0x000832D4 (hub b). Single-member cluster. [O]

#### 0x00089508 `thunk_FUN_0008e58c` — stub
Indirect thunk through DAT_00089530 → 0x0008E58C (unit 13). Caller 0x000832AC (hub a);
original symbol name retained. Single-member cluster. [O]

#### 0x0008950E `trace_rel_3b150` — stub
Trace fill/skip (&DAT_0003b150, 2). Caller 0x000832F0. Single-member cluster. [O]

## Unit summary

Region 0x000800AA–0x0008950E, 220 clusters, 223 named functions (two multi-member
clusters: 0x00080774/0x00080782 tag-a/tag-b stream wrappers, and
0x00088792/0x000887C0/0x0008D566 occurrence-tracker stubs differing only in event id
0x9CF/0x9F6/0x96A).

Counts by kind: logic 117, stub 80, library 15, data-accessor 6, dispatch 4, init 1.
`unknown` entries: none — every function's mechanics were observable; meaning-level
inference is flagged [I] (53 entries carry at least one [I], 217 at least one [O]).

Subsystems documented:
1. UDS application layer (spec 15): dispatcher front-end (0x000800AA/0x00080108), all
   service handlers named by spec 15 §2 (0x10, 0x11, 0x14, 0x19 ×3 sub-handlers, 0x22 +
   DID engine, 0x23, 0x27, 0x28, 0x2A [I], 0x2E, 0x31, 0x3E, 0x85), per-service state
   clears, and the gated stream-channel wrappers (0x000806F4–0x000807CC).
2. Fault reporting (spec 13): fault manager (0x000873F0 family), complement-verified
   fault-memory entry writer (0x00087120), 12-entry debounce engine (0x000877C0),
   boot-record slot (DAT_fff800e4 magic 0x5555), clear-DTC record/timers.
3. Monitor/sequencer families with E-GAS-style complement verification: 10-channel
   (0x00083DB0 + shadow 0x00084392 + comparator 0x00084644), 5-channel (0x000849D4 +
   0x00084DA0), 2-channel FSM (0x00085828) with phase FSM 0x00085FC4, plus the
   thermal/power model (0x00083ABC) and the top sequencer 0x00085160 — all reporting
   integrity events 0x220/0x223/0x226/0x227 with ROM flow tags (&LAB_0003dxxx).
4. Signal publication hubs 0x000832AC/0x000832D4/0x00083300/0x0008332C and their
   publish/reset/hook functions (signal codes 0x1FC, 0x60/0x62, 0x160, 0x29, 0x2A, 0x32;
   occurrence ids 0x9CF/0x9F6/0x96A; per-bus calibration via DAT_fff90ab8 bank index).
5. Trace recorder (DAT_fff92e3c cursor): group allocator, usage watermark (event 0x227),
   fill/skip API used as guard release.
6. Flash/dataflash helpers: MAT-copy chunked routines 0x00082A16/0x00082B42 ([I] routine
   FE20, spec 15 §4), record-table writer 0x00081C34 + async poll 0x00082944, mirror
   remap writers 0x00082C7C/0x00082CD0.

Cross-region leads:
- Unit 08 (r04_boot): stream-channel engine `stream_chan_*` 0x00074D28–0x00074F5A backs
  all `diag_stream_*` wrappers here; 0x0007CA24 (fault-occurrence tracker) is still
  unnamed in unit 08/09/10 catalogs — named here by behavior as the occurrence tracker.
- Unit 13 (r08_app 0x00089518+): `mon_*` family consumes this unit's drivers
  (0x00083DB0, 0x00084392, 0x000849D4, 0x00084DA0 call 0x00089718/0x00089F5C/0x0008A788/
  0x0008ADF4/0x0008B284); 0x0008B7A8 guarded sequencer is driven by 0x00085160; the
  0x0008332C hub is the same chain unit 13 documents as mon_subsys_init (0x00089518).
- 0x0009A3BC/0x0009A3D6 (callees of 0x00081C34, other unit) belong to the flash-write
  engine and should be cross-checked against `eeprom_write_record_table`.
- r04_boot 0x0007FFB4 (session bring-up) calls 0x00080160/0x0008072A — unit covering it
  should link here for the UDS session timing globals DAT_fff92f54-0x92f78.
- Note: 0x00080760/0x00080798 (in unit 11's slice) sit inside the stream slot table
  0x000807D4–0x00080808 documented at the top of this file.
