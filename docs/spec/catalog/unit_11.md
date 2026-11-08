# Unit 11 (r04_boot)

Cluster catalog for unit_11_r04_boot.csv. Module context discovered in this unit: a
**monitored-RAM data-block supervision service** centered on a linked list at `DAT_fff92408`
(nodes = guarded data blocks with complement-pair words, checksum seed at +0x0c, generation
byte at +0x11, age pair +0x12/+0x18 (complements), rolling seed pair +0x14..+0x1b, repair-class
id at +0x17, flags word at +0x10: bit24 = table-index checksum mode, bit25 = has repair handler).
Errors are reported by setting the fault-detail pointer `DAT_fff90998` to a flash code-table
entry (0x000975E4..0x000975EC) and calling the fault-entry thunk 0x0006865C (via
`PTR_FUN_0007f42c`) with subcode 0x220/0x221/0x226; that thunk captures SR/PR/stack context into
the fault record at 0xFFF80DB4..0xFFF80E0E and copies `fff90998` into 0xFFF80DC4 (spec 13 fault
reporting). All entries single-member clusters (count=1).

#### 0x0007F134 `rammon_list_recheck` — logic
Re-validation pass over the monitored-block list at DAT_fff92408: walks every node (next at
+0x08), counts them, and mirrors the last node's inverted checksum seed (~(node+0x0c)) to fixed
RAM 0xFFF7FFFC; always sets bit 0x100 in state word fff9384c, and if the walked count differs
from registered count fff9412a it sets fff94f84 bit 0x100 and clears fff94dfc bit 0x100.
Finishes with fff93320 = flash word at 0x0007F424 ^ 0xAAAA (signature refresh). Called only from
register/unregister (0x0007F44E, 0x0007F58A) inside their OS-lock window. [O]

#### 0x0007F1A6 `rammon_periodic_check` — logic
The periodic integrity check of the monitored-block service (called from r08_app supervision
task 0x000873F0, which also opens with the 0x0006865C service thunk). Per node: re-ages the
+0x13/+0x19 pair against tick counter fff94128; when due, verifies the check word node[1]
against ~node[3] (table mode xor's in an entry from PTR_LAB at 0x0007F428 indexed by rolling
seed fff93ba6 and restores the seed pair on success); on mismatch it optionally runs the repair
dispatch 0x0007EE48 (node flag 0x02000000), latches the worst node into fff9331c and its
repair-class id into fff800e9, and sets fff94f84/fff94dfc bit 0x1000. Verified values are
promoted word[1]→word[0], word[8]→word[7], patterns re-armed to 0xAAAA/0x5555 (word[1]/word[8]).
Any complement-pair violation (word0^word7, or age pair) raises fault 0x226 via PTR_FUN_0007f42c
with fff90998 = 0x975E4..0x975EA; post-loop, the aging triple fff94128/29/2b inconsistency
raises fault 0x220, then fff94128 resets and min-age globals are updated. [O]

#### 0x0007F3CE `rammon_check_registration` — logic
Small validation helper (caller 0x00072DB8, itself used by the 0x0007E5xx–0x0007E8xx accessor
family): sets bit 0x400 in state word fff9384c, then compares the node's generation byte
(+0x11) against the current list generation fff9412c; on mismatch sets fff94f84 bit 0x400 and
clears fff94dfc bit 0x400 (block stale/unregistered). [O]

#### 0x0007F404 `rammon_node_init_word` — logic
Node constructor for a plain (non-table) monitored block, called from r08_app 0x000870E0:
zeroes type byte +0x10, stores the check-age pair (+0x12 = param2, +0x18 = param3, expected
complements), repair-class id +0x17 = 4, seeds guarded word0 = param4 with complement ~param4 at
word7 (+0x1c), and clears generation +0x11 (not yet in list). [O]

#### 0x0007F430 `rammon_node_init_tblidx` — logic
Node constructor variant for table-mode blocks (byte +0x10 = 1 selects the flag-0x01000000
path used by 0x0007F44E/0x0007F1A6): sets type 1, age pair (+0x12/+0x18), repair-class id
+0x17 = 4, checksum-table index +0x16 = param4, generation +0x11 = 0. Caller 0x0007DD3C. [O]

#### 0x0007F44E `rammon_register_block` — logic
Inserts a prepared node at the head of the monitored list (DAT_fff92408) under the OS
lock/unlock pair (pointer table 0x0007F684/0x0007F694 → 0x0009A428/0x0009A3FC nesting-counter
enter/exit). Ages the node pair into the global min-age bookkeeping (fff94129/fff9412b),
checks both complement pairs (fault 0x226, fff90998 = 0x975EC[+1]), then seeds the expected
checksum: plain mode word[3] = ~verified word via the complement-read accessor 0x0006BE34
(itself faulting 0x221 with fff90998 = node address); table mode computes word[0]/word[7]/word[3]
from table[+0x16] ^ table[fff93ba6] (table pointer at 0x0007F68C). Arms pattern words
word[1] = 0xAAAA / word[8] = 0x5555, stamps generation fff9412c, increments block count
fff9412a, links the node, and re-validates the list via 0x0007F134. Callers 0x0007DD3C and
r08_app 0x00087060. [O]

#### 0x0007F58A `rammon_unregister_block` — logic
Removes a node from the monitored list under the OS lock pair: bumps list generation fff9412c
(1 after wrap at -1), unlinks the node (fixing head fff92408 or predecessor +0x08), marks the
node unregistered (+0x11 = 0), clears the tracked-worst pointer fff9331c and id fff800e9 if it
pointed at this node, decrements count fff9412a, re-stamps every remaining node's generation,
then re-validates via 0x0007F134. Called by the six generated wrappers 0x0007E8F8/0x0007E918/
0x0007E938/0x0007E958/0x0007E978/0x0007E9B8 and r08_app 0x00087060. [O]

#### 0x0007F60C `rammon_publish_word` — logic
Publishes a new expected value pair into a registered block (caller 0x00072CCE ← 0x0007E500):
under the OS lock, sets state bit 0x400; if the node's generation is current it stores
word[1] (+4) = param2 and word[8] (+0x20) = ~param2 (the pair verified by the next
0x0007F1A6 pass), resets the node age pair to 1/0xFE, and rebases the global min-age triple
(fff94129 = 1, fff9412b adjusted); a stale generation instead sets the 0x400 fault flags
(fff94f84 |= / fff94dfc &= ~0x400). [O] mechanics, [I] "publish expected value" reading.

#### 0x0007F70A `flash_mode_get` — data-accessor
Classifies the flash-session state byte DAT_fff84a1a into 0..3: 0 → 1, bit0 set → 2, 0xFE → 0,
anything else → 3. Consumers pick behavior from it: the flash write scheduler 0x0007D3CC uses
result 2 (with fff93b44 ≠ 0) to select an alternate programming-routine pair, and 0x0007AADA
maps its result to diagnostic codes 0x00/0x43/0x55/0xFF. Exact state meanings not resolvable
from this unit. [I]

#### 0x0007FFB4 `boot_tbl_dispatch_entry` — dispatch
Runs one entry (param_1 = index) of the 0x30-byte-stride table at DAT_fff92480: records the
"exclusive" flag from entry+0x24, copies hand-over bytes from fff924d2/fff924d3 and the entry
word at +0x0c into the 0xFFF80080–0xFFF800C7 state block (task id → 0xFFF800B0), reads config
word (fff9247c)+0x16 → fff911c0, then calls 0x00080160 (loads two 12-byte parameter blocks
from entry+0x1c/+0x20 into fff92f54..0x78, both timers reloaded with 5000) and mirrors
fff92f54..64 into 0xFFF80088–0xFFF80098. If entry+0x2c ≠ 0 makes a pre-call (ptr at
0x0008014C), it then invokes the entry's function pointer at +0x18 and, when the exclusive
flag is clear, activates every other entry via 0x0008072A(id, 1) (ids with bit 0x80 are
skipped). No static callers — reached through a pointer table (kernel/scheduler API).
Writes the hand-over RAM readable via the measurement protocol (spec 10 §; spec 06/15
hand-over context). [O] mechanics, [I] purpose.

## Unit summary
- Clusters: 10 (all single-member). Functions named: 10.
- Kinds: logic 8 (0x0007F134, 0x0007F1A6, 0x0007F3CE, 0x0007F404, 0x0007F430, 0x0007F44E,
  0x0007F58A, 0x0007F60C), data-accessor 1 (0x0007F70A), dispatch 1 (0x0007FFB4).
- unknown: 0.
- Cross-region leads:
  - r08_app 0x000873F0 is the periodic supervision task driving 0x0007F1A6 (plus many
    0x0006Bxxx complement-read accessors like 0x0006BE34); its unit should treat it as the
    data-integrity monitor heartbeat.
  - r08_app 0x00087060/0x000870E0 form the register/unregister API used by application data
    blocks; the six 0x0007E8F8–0x0007E9B8 wrappers are generated per-block unregister stubs.
  - Fault-entry chain: 0x0006865C → 0x00066F82 captures context + fff90998 into the fault
    record at 0xFFF80DB4.. (subcodes 0x220/0x221/0x226 seen here) — belongs with the spec 13
    fault-reporting unit; other units seeing `PTR_FUN_000xxxxxxx(0x2xx)` calls with
    `DAT_fff90998` set are using the same reporting API.
  - 0x0007FFB4's table entries (function ptr at +0x18, sibling activation via 0x0008072A)
    and the 0xFFF80080–C7 hand-over block connect to spec 10 (RAM read) and spec 06/15
    (boot hand-over); the r08_app scheduler unit owning 0x0008072A/0x000801DC should
    cross-check.
  - 0x0007F70A's state byte DAT_fff84a1a is written elsewhere (no writer in this unit) —
    flash/boot-mode unit should locate the writer to name states 1/2/3.
