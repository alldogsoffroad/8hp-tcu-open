# ZF 8HP45/70: ATRA seminar slides, notes

Source: `zf8hp45_70_rukovodstvo.pdf`, 57 slides, from https://akpphelp.ru/images/contents/files/zf8hp45_70_rukovodstvo.pdf.
It is an ATRA seminar deck (Toledo Trans-Kit / Seal Aftermarket) using **VW/Audi part codes**
(G182, G195, G747, N486, J217). Slides 36–45 were reviewed on 2026-11-16; the user pointed to
41–45. Paraphrased; nothing is reproduced. Grade [M], with the caveat that this is a different
variant: VW/Audi labelling, the same ZF mechatronic family.

## Sensors (slides 41, 43, 44)
- **G182 input (turbine) speed sensor:** a Hall sensor that reads an **encoder wheel with a
  magnetic ring**, a band of alternating magnetic poles on the drum next to the P2 planet. It is
  **not a steel-tooth target**.
- **G195 output speed sensor:** a Hall sensor that reads the **extended teeth of the parking
  gear** (steel), on the P4 planet / output side.
- Both are called "intelligent sensors" by the deck.
- **G747 Parking Lock Sender:** the park position sensor, a separate part on the mechatronic
  next to the park mechanism. N486 is the park hold solenoid (its connector is on the same
  harness).
- G93 fluid temperature sensor; J217 = the mechatronic (TCM).

## What this means for our bench (2026-11-16)
- **The output sensor fits:** steel counted on TIF12, the parking-gear teeth.
- **The input sensor is now explained:** the evenly spaced spots where the user's magnet grabs
  the turbine-side drum are the poles of the magnetic ring. A steel piece cannot trigger G182,
  which is why the two steel tests showed nothing. The earlier hand-magnet test on that sensor
  (15:27) watched only the analog inputs, not the Timer F counters. **Test still to run:**
  pass a magnet with alternating poles (flip it between passes) across G182 while watching
  F13.
- **AN5/AN6 = G747**, not a speed sensor. The deck does not give its polarity (which contact is
  active in Park).
