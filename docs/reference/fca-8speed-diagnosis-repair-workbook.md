# FCA "8-Speed Automatic Transmission Diagnosis and Repair", Student Workbook (2015)

Source: FCA US LLC technical training, © 2015, 122 PDF pages, the 8HP45 family on shift-by-wire
vehicles. The specs already cite it as "user's copy" (spec 01 §2.8, spec 09, spec 11, spec 12 §1).
A public copy is at https://akpphelp.ru/assets/template/redesign/files/guidelines/zf8hp45_70_rukovodstvo_manual.pdf
(reviewed 2026-11-16). Paraphrased; nothing is reproduced. Grade [M].

**Extraction note:** the body text uses an encoded font, so `pdftotext` returns garbage except the
figure labels. Read the pages as images. Printed page = PDF page − 8.

## Pages most relevant to the bench work (printed page / PDF page)
- **p. 74 / 82, Fig. 43, park lock hydraulics:**
  - the default state is pawl engaged;
  - out of Park, the park release solenoid feeds the park release valve, which puts line pressure
    into the park release cylinder and releases the pawl;
  - the park hold solenoid mechanically holds the piston out of Park, including with the engine
    off and no pressure.

  This matches the stock law ported in r03j/r03k.
- **p. 85 / 93, Fig. 47, TCM I/O:**
  - eight solenoids on the actuator supply (A, D, B, E, C, TCC, line pressure, park release);
  - the park hold solenoid on a separate actuator-supply branch;
  - the temperature sensor;
  - ISS/OSS on a shared supply;
  - **the park lock position sensor with two signals labelled "Park" and "Not Park"** plus sensor
    ground.
- **p. 87 / 95, Fig. 48:** ISS and OSS are Hall-effect sensors. The ISS is at the front and reads
  the **magnetic ring on the P2 carrier**; the OSS is at the back and reads the P4 carrier
  (output). Bench: OSS = TIF12 (steel), ISS = TIF13 (magnet), 2026-11-16.
- **p. 88 / 96, Fig. 49:** the fluid temperature thermistor sits under the solenoids. The TCM also
  has an internal temperature sensor; both are compared for hot mode.
- **p. 89 / 97, Fig. 50, park position sensor:**
  - a digital sensor watching the **park pawl linkage**;
  - its **target is on the park selector shaft**;
  - **two digital Hall elements** in the valve-body assembly watch that one target.
- **p. 91–92 / 99–100, Fig. 52–53:**
  - shift-by-wire: the electronic shifter talks to the TCM on CAN-C and on a dedicated CAN, with
    redundant messages;
  - TCM connector pins: 13 battery, 9 ignition, 3/4 dedicated CAN, 5/6 CAN-C, 14 ground,
    **15 "TRS Park Signal"** (a hard-wired output to the RF hub).

  Spec 01 §2.7 already carries these.

## Bearing on the open park-contact question (2026-11-16)
- The sensor has one target and two elements, one named "Park" and one "Not Park". So in each
  valid state exactly one element should see the target; the stock maps A alone → 1 and B alone
  → 0.
- The stock code expects **position 1 (A = AN5 alone) in Park** (park_position_speed_decode.md).
  That makes **AN5 the "Park" element and AN6 the "Not Park" element** [I, from the code; the
  workbook does not say which signal pin is which].
- The bench's hand-magnet placement, at the spot the user identified as the lever's out-of-Park
  position, activated AN5. Under the stock's reading, that spot is the **Park** position. With a
  target that sits by the sensor in both states, the earlier "magnet at the sensor = out of Park"
  may describe the target's travel rather than which element it covers.
- Still decided only by moving the real mechanism (valve body fitted).
