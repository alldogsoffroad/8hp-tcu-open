# Spec 05 — Clocks, watchdog and power-up

Status: **draft.** Peripheral clock established; CPU clock open (measurable on first bring-up).
Markers: **[M]** manual, **[O]** observed in stock firmware, **[I]** inference, **[D]** derived by calculation.

## 1. Clock system

The SH7254R has no software-programmable PLL. Both clocks are fixed by the crystal and strap pins
[M §4]:

- CPU/internal clock Iφ = EXTAL × {4, 6, 8, 10} (pins MD_CLK1, MD_CLK0)
- Peripheral clock Pφ = EXTAL × {1, 2} (pin MD_CLKP); the CK pin outputs Pφ
- EXTAL input range 16–20 MHz

### 1.1 Peripheral clock: **Pφ = 20 MHz** [D]

The stock CAN driver selects bit timing from a table of five standard bit rates [O]. Applying the
RCAN-TL1 formula `bit rate = Pφ / (2 × (BRP+1) × (TSEG1+TSEG2+1))` [M §18] to every entry gives
the same Pφ:

| Bit rate | BCR0 | BCR1 | BRP | TSEG1 | TSEG2 | SJW | Tq/bit | Pφ |
|---|---|---|---|---|---|---|---|---|
| 1 000 000 | 0x0000 | 0x6110 | 0 | 7 | 2 | 2 | 10 | 20 MHz |
| 666 667 | 0x0000 | 0xA220 | 0 | 11 | 3 | 3 | 15 | 20 MHz |
| 500 000 | 0x0000 | 0xE330 | 0 | 15 | 4 | 4 | 20 | 20 MHz |
| 250 000 | 0x0001 | 0xE330 | 1 | 15 | 4 | 4 | 20 | 20 MHz |
| 125 000 | 0x0003 | 0xE330 | 3 | 15 | 4 | 4 | 20 | 20 MHz |

All three CAN channels are configured for **500 kbit/s** [O] (sample point 80 %, SJW 4).

Because Pφ ∈ {1, 2} × EXTAL and EXTAL ∈ [16, 20] MHz, the only consistent combination is
**EXTAL = 20 MHz, MD_CLKP = 0 (Pφ = EXTAL)** [D].

### 1.2 CPU clock: **open**

Iφ ∈ {80, 120, 160, 200} MHz. The boot code reads bits 1–0 of an undocumented register at
`0xFFFF1000` and chooses the on-chip RAM access-cycle setting from it (`RAMACYC` = `0x7821` when
the bits are 00, otherwise `0x7810`) [O]. [I] Those bits reflect the PLL strap setting, so the stock
software adapts to the clock mode instead of assuming one.

Recommended resolution: the first bring-up firmware should time a fixed CPU loop against a CMT
channel running from Pφ (20 MHz) and report Iφ over CAN or SCI.

### 1.3 Derived peripheral rates

| Item | Setting | Rate |
|---|---|---|
| SPI (RSPI-A, RSPI-B) | SPBR = 5, BRDV = 0 | Pφ / 12 = **1.667 MHz** [D] |
| CAN (all channels) | see §1.1 | **500 kbit/s** [O] |

## 2. Internal watchdog (WDT)

| Item | Stock setting | Meaning |
|---|---|---|
| WRCR | `0x5A80` | RSTE = 1: watchdog overflow **resets the chip** [M][O] |
| WTCR (normal) | `0xA567` | watchdog-timer mode, counter enabled, CKS = 7 → Pφ/16384 (819.2 µs per count) [M][O] |
| WTCNT start | `0xB7` | first overflow after 73 counts ≈ **59.8 ms** [D] |
| WTCNT service value | `0xDB` | each service leaves 37 counts ≈ **30.3 ms** until overflow [D] |
| WTCR (alternate) | `0xA563` | CKS = 3 → Pφ/256 (12.8 µs per count), with a caller-supplied count [O]; [I] used to force a quick reset |

Writes use the manual's key protocol: `0xA5` upper byte for WTCR, `0x5A` for WTCNT and WRCR [M].

The software also checks at run time that the watchdog is running with the expected clock
select [O].

## 3. External supervision

See spec 03 §5: the stock software relies on an external device on RSPI-B (chip select SSLB1) as
part of its shutdown path. Its service protocol and timing are **open**.

## 4. Power-up order observed in the stock software

1. RAM access cycles and RAM write enable configured; ROM cache enabled (`RCCR` = 1,
   `RCCR2` = `0xF5`) [O].
2. Interrupt controller cleared to a known state [O].
3. All ports set to safe directions and levels, then peripheral pin functions selected
   (spec 01 §4) [O].
4. DMA controllers reset, then enabled [O].
5. RSPI units disabled, then the SPI job layer started (spec 03) [O].
6. CAN controllers configured per channel (bit timing, all mailboxes inactive, then mailbox
   setup) [O].
7. Watchdog started (≈60 ms first timeout), then serviced cyclically [O].

## 5. Open items

1. CPU clock Iφ (measure on bring-up, §1.2).
2. External supervisor protocol and timing (spec 03).
3. ~~Operating-system tick~~ — resolved in spec 02: timer H at 4 MHz with compare 4000 gives a
   **1 ms tick**; timer A provides a 1 µs timestamp. The scheduling of tasks on that tick is still
   open.
