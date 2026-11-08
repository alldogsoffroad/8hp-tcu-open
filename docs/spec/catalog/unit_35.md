# Unit 35 (r10_app)

Region r10_app: 220 clusters, 222 functions (0x00106ED0-0x00108260). Cluster-order entries below.
This unit is two things: (1) the boot parameter-group pointer-table init bank (entries 0xFFF84C8C-0xFFF84DE8,
rom/ram default pairs + complement shadows 0xFFF8D01C-0xFFF8D080 + integrity getters), all driven by init
dispatcher 0x000DA418; (2) the solenoid actuator classes (spec 12): pressure/small/on-off constructors,
current calculators, fast-fill SM, and the solcur channel-set cycle wrappers.

#### 0x00106ED0 `init_pgrp_4c8c_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053694 into parameter-group table entry 0xFFF84C8C. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106ED0.

#### 0x00106ED8 `init_pgrp_4c8c_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182718 into parameter-group table entry 0xFFF84C8C. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106ED8.

#### 0x00106EEC `init_pgrp_4c90_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x0005370C into parameter-group table entry 0xFFF84C90. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106EEC.

#### 0x00106EF4 `init_pgrp_4c90_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182744 into parameter-group table entry 0xFFF84C90. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106EF4.

#### 0x00106F08 `init_pgrp_4c94_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x000537F8 into parameter-group table entry 0xFFF84C94. spec 12: solenoid i_min/i_max group (500/8500). The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106F08.

#### 0x00106F10 `init_pgrp_4c94_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x001827E4 into parameter-group table entry 0xFFF84C94. spec 12: solenoid i_min/i_max group (500/8500). The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106F10.

#### 0x00106F24 `init_pgrp_4c98_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053808 into parameter-group table entry 0xFFF84C98. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106F24.

#### 0x00106F2C `init_pgrp_4c98_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x001827F4 into parameter-group table entry 0xFFF84C98. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106F2C.

#### 0x00106F40 `init_pgrp_4c9c_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053818 into parameter-group table entry 0xFFF84C9C. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106F40.

#### 0x00106F48 `init_pgrp_4c9c_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182804 into parameter-group table entry 0xFFF84C9C. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106F48.

#### 0x00106F5C `init_pgrp_4ca0_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x0005381C into parameter-group table entry 0xFFF84CA0. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106F5C.

#### 0x00106F64 `init_pgrp_4ca0_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182808 into parameter-group table entry 0xFFF84CA0. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106F64.

#### 0x00106F78 `init_pgrp_4ca4_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053830 into parameter-group table entry 0xFFF84CA4. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106F78.

#### 0x00106F80 `init_pgrp_4ca4_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x0018281C into parameter-group table entry 0xFFF84CA4. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106F80.

#### 0x00106F94 `init_pgrp_4ca8_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053840 into parameter-group table entry 0xFFF84CA8. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106F94.

#### 0x00106F9C `init_pgrp_4ca8_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x0018282C into parameter-group table entry 0xFFF84CA8. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106F9C.

#### 0x00106FB0 `init_pgrp_4cac_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x0005384C into parameter-group table entry 0xFFF84CAC. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106FB0.

#### 0x00106FB8 `init_pgrp_4cac_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182838 into parameter-group table entry 0xFFF84CAC. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106FB8.

#### 0x00106FCC `init_pgrp_4cb0_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x0005385C into parameter-group table entry 0xFFF84CB0. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106FCC.

#### 0x00106FD4 `init_pgrp_4cb0_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182848 into parameter-group table entry 0xFFF84CB0. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106FD4.

#### 0x00106FE8 `init_pgrp_4cb4_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053A10 into parameter-group table entry 0xFFF84CB4. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106FE8.

#### 0x00106FF0 `init_pgrp_4cb4_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x001829B0 into parameter-group table entry 0xFFF84CB4. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00106FF0.

#### 0x00107004 `init_pgrp_4cb8_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053A98 into parameter-group table entry 0xFFF84CB8. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107004.

#### 0x0010700C `init_pgrp_4cb8_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x001829EC into parameter-group table entry 0xFFF84CB8. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010700C.

#### 0x00107020 `init_pgrp_4cbc_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053B9C into parameter-group table entry 0xFFF84CBC. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107020.

#### 0x00107028 `init_pgrp_4cbc_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182A5C into parameter-group table entry 0xFFF84CBC. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107028.

#### 0x0010703C `init_pgrp_4cc0_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053BD4 into parameter-group table entry 0xFFF84CC0. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010703C.

#### 0x00107044 `init_pgrp_4cc0_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182A94 into parameter-group table entry 0xFFF84CC0. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107044.

#### 0x00107058 `init_pgrp_4cc4_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053BDC into parameter-group table entry 0xFFF84CC4. spec 11: shift threshold group. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107058.

#### 0x00107060 `init_pgrp_4cc4_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182A9C into parameter-group table entry 0xFFF84CC4. spec 11: shift threshold group. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107060.

#### 0x00107074 `init_pgrp_4cc8_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053CC0 into parameter-group table entry 0xFFF84CC8. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107074.

#### 0x0010707C `init_pgrp_4cc8_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182B80 into parameter-group table entry 0xFFF84CC8. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010707C.

#### 0x00107090 `init_pgrp_4ccc_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053CE0 into parameter-group table entry 0xFFF84CCC. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107090.

#### 0x00107098 `init_pgrp_4ccc_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182BA0 into parameter-group table entry 0xFFF84CCC. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107098.

#### 0x001070AC `init_pgrp_4cd0_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053D54 into parameter-group table entry 0xFFF84CD0. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001070AC.

#### 0x001070B4 `init_pgrp_4cd0_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182C08 into parameter-group table entry 0xFFF84CD0. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001070B4.

#### 0x001070C8 `init_pgrp_4cd4_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053DD0 into parameter-group table entry 0xFFF84CD4. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001070C8.

#### 0x001070D0 `init_pgrp_4cd4_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182C6C into parameter-group table entry 0xFFF84CD4. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001070D0.

#### 0x001070E4 `init_pgrp_4cd8_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053E00 into parameter-group table entry 0xFFF84CD8. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001070E4.

#### 0x001070EC `init_pgrp_4cd8_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182C9C into parameter-group table entry 0xFFF84CD8. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001070EC.

#### 0x00107100 `init_pgrp_4cdc_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053E3C into parameter-group table entry 0xFFF84CDC. spec 11: shift group. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107100.

#### 0x00107108 `init_pgrp_4cdc_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182CD8 into parameter-group table entry 0xFFF84CDC. spec 11: shift group. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107108.

#### 0x0010711C `init_pgrp_4ce0_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053E6C into parameter-group table entry 0xFFF84CE0. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010711C.

#### 0x00107124 `init_pgrp_4ce0_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182D08 into parameter-group table entry 0xFFF84CE0. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107124.

#### 0x00107138 `init_pgrp_4ce4_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053E80 into parameter-group table entry 0xFFF84CE4. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107138.

#### 0x00107140 `init_pgrp_4ce4_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182D1C into parameter-group table entry 0xFFF84CE4. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107140.

#### 0x00107154 `init_pgrp_4ce8_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053E84 into parameter-group table entry 0xFFF84CE8. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107154.

#### 0x0010715C `init_pgrp_4ce8_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182D20 into parameter-group table entry 0xFFF84CE8. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010715C.

#### 0x00107164 `init_pgrp_4cec_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053E8C into parameter-group table entry 0xFFF84CEC. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107164.

#### 0x0010716C `init_pgrp_4cec_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182D28 into parameter-group table entry 0xFFF84CEC. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010716C.

#### 0x00107174 `init_pgrp_4cf0_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053E94 into parameter-group table entry 0xFFF84CF0. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107174.

#### 0x0010717C `init_pgrp_4cf0_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182D30 into parameter-group table entry 0xFFF84CF0. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010717C.

#### 0x001071A8 `init_pgrp_4cf4_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053E9C into parameter-group table entry 0xFFF84CF4. table read by rseq mode qualifier 0x00107B2A. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001071A8.

#### 0x001071B0 `init_pgrp_4cf4_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182D38 into parameter-group table entry 0xFFF84CF4. table read by rseq mode qualifier 0x00107B2A. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001071B0.

#### 0x001071C4 `init_pgrp_4cf8_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053EA4 into parameter-group table entry 0xFFF84CF8. spec 11: shift threshold group. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001071C4.

#### 0x001071CC `init_pgrp_4cf8_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182D40 into parameter-group table entry 0xFFF84CF8. spec 11: shift threshold group. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001071CC.

#### 0x001071E0 `init_pgrp_4cfc_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053F38 into parameter-group table entry 0xFFF84CFC. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001071E0.

#### 0x001071E8 `init_pgrp_4cfc_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182DD4 into parameter-group table entry 0xFFF84CFC. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001071E8.

#### 0x001071FC `init_pgrp_4d00_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053F3C into parameter-group table entry 0xFFF84D00. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001071FC.

#### 0x00107204 `init_pgrp_4d00_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182DD8 into parameter-group table entry 0xFFF84D00. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107204.

#### 0x00107218 `init_pgrp_4d04_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053F4C into parameter-group table entry 0xFFF84D04. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107218.

#### 0x00107220 `init_pgrp_4d04_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182DE8 into parameter-group table entry 0xFFF84D04. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107220.

#### 0x00107234 `init_pgrp_4d08_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00053FAC into parameter-group table entry 0xFFF84D08. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107234.

#### 0x0010723C `init_pgrp_4d08_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00182E48 into parameter-group table entry 0xFFF84D08. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010723C.

#### 0x00107250 `init_pgrp_4d0c_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00054198 into parameter-group table entry 0xFFF84D0C. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107250.

#### 0x00107258 `init_pgrp_4d0c_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00183020 into parameter-group table entry 0xFFF84D0C. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107258.

#### 0x0010726C `init_pgrp_4d10_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00054680 into parameter-group table entry 0xFFF84D10. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010726C.

#### 0x00107274 `init_pgrp_4d10_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x001832A0 into parameter-group table entry 0xFFF84D10. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107274.

#### 0x00107288 `init_pgrp_4d14_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00054C4C into parameter-group table entry 0xFFF84D14. spec 12: solenoid temperature-correction maps. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107288.

#### 0x00107290 `init_pgrp_4d14_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x0018371C into parameter-group table entry 0xFFF84D14. spec 12: solenoid temperature-correction maps. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107290.

#### 0x001072A4 `init_pgrp_4d18_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00054C54 into parameter-group table entry 0xFFF84D18. spec 12: per-channel pressure->current curve group. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001072A4.

#### 0x001072AC `init_pgrp_4d18_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00183724 into parameter-group table entry 0xFFF84D18. spec 12: per-channel pressure->current curve group. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001072AC.

#### 0x001072C0 `init_pgrp_4d1c_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00054C80 into parameter-group table entry 0xFFF84D1C. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001072C0.

#### 0x001072C8 `init_pgrp_4d1c_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00183750 into parameter-group table entry 0xFFF84D1C. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001072C8.

#### 0x001072DC `init_pgrp_4d20_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00054CC0 into parameter-group table entry 0xFFF84D20. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001072DC.

#### 0x001072E4 `init_pgrp_4d20_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00183790 into parameter-group table entry 0xFFF84D20. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001072E4.

#### 0x001072EC `init_pgrp_4d24_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00054D60 into parameter-group table entry 0xFFF84D24. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001072EC.

#### 0x001072F4 `init_pgrp_4d24_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00183830 into parameter-group table entry 0xFFF84D24. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001072F4.

#### 0x001072FC `init_pgrp_4d28_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00054E00 into parameter-group table entry 0xFFF84D28. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001072FC.

#### 0x00107304 `init_pgrp_4d28_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x001838D0 into parameter-group table entry 0xFFF84D28. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107304.

#### 0x0010730C `init_pgrp_4d2c_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00054EA0 into parameter-group table entry 0xFFF84D2C. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010730C.

#### 0x00107314 `init_pgrp_4d2c_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00183970 into parameter-group table entry 0xFFF84D2C. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107314.

#### 0x0010731C `init_pgrp_4d30_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00054F40 into parameter-group table entry 0xFFF84D30. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010731C.

#### 0x00107324 `init_pgrp_4d30_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00183A10 into parameter-group table entry 0xFFF84D30. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107324.

#### 0x00107368 `init_pgrp_4d34_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00055524 into parameter-group table entry 0xFFF84D34. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107368.

#### 0x00107370 `init_pgrp_4d34_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00183F10 into parameter-group table entry 0xFFF84D34. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107370.

#### 0x00107384 `init_pgrp_4d38_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00056EA0 into parameter-group table entry 0xFFF84D38. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107384.

#### 0x0010738C `init_pgrp_4d38_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x0018542C into parameter-group table entry 0xFFF84D38. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010738C.

#### 0x001073A0 `init_pgrp_4d3c_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00056EC4 into parameter-group table entry 0xFFF84D3C. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001073A0.

#### 0x001073A8 `init_pgrp_4d3c_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00185450 into parameter-group table entry 0xFFF84D3C. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001073A8.

#### 0x001073BC `init_pgrp_4d40_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00056EDC into parameter-group table entry 0xFFF84D40. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001073BC.

#### 0x001073C4 `init_pgrp_4d40_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00185468 into parameter-group table entry 0xFFF84D40. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001073C4.

#### 0x001073D8 `init_pgrp_4d44_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00056F98 into parameter-group table entry 0xFFF84D44. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001073D8.

#### 0x001073E0 `init_pgrp_4d44_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x001854E8 into parameter-group table entry 0xFFF84D44. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001073E0.

#### 0x001073F4 `init_pgrp_4d48_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x0005713C into parameter-group table entry 0xFFF84D48. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001073F4.

#### 0x001073FC `init_pgrp_4d48_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00185670 into parameter-group table entry 0xFFF84D48. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001073FC.

#### 0x00107410 `init_pgrp_4d4c_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00057474 into parameter-group table entry 0xFFF84D4C. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107410.

#### 0x00107418 `init_pgrp_4d4c_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00185914 into parameter-group table entry 0xFFF84D4C. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107418.

#### 0x00107420 `init_pgrp_4d50_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x000574B4 into parameter-group table entry 0xFFF84D50. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107420.

#### 0x00107428 `init_pgrp_4d50_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00185954 into parameter-group table entry 0xFFF84D50. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107428.

#### 0x00107430 `init_pgrp_4d54_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x000574F4 into parameter-group table entry 0xFFF84D54. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107430.

#### 0x00107438 `init_pgrp_4d54_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00185994 into parameter-group table entry 0xFFF84D54. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107438.

#### 0x00107440 `init_pgrp_4d58_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00057534 into parameter-group table entry 0xFFF84D58. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107440.

#### 0x00107448 `init_pgrp_4d58_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x001859D4 into parameter-group table entry 0xFFF84D58. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107448.

#### 0x00107450 `init_pgrp_4d5c_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00057574 into parameter-group table entry 0xFFF84D5C. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107450.

#### 0x00107458 `init_pgrp_4d5c_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00185A14 into parameter-group table entry 0xFFF84D5C. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107458.

#### 0x0010749C `init_pgrp_4d60_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00057C64 into parameter-group table entry 0xFFF84D60. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010749C.

#### 0x001074A4 `init_pgrp_4d60_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00185F0C into parameter-group table entry 0xFFF84D60. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001074A4.

#### 0x001074B8 `init_pgrp_4d64_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x00058BE0 into parameter-group table entry 0xFFF84D64. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001074B8.

#### 0x001074C0 `init_pgrp_4d64_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00186B2C into parameter-group table entry 0xFFF84D64. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001074C0.

#### 0x001074D4 `init_pgrp_4d68_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x0005A2B4 into parameter-group table entry 0xFFF84D68. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001074D4.

#### 0x001074DC `init_pgrp_4d68_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00187E0C into parameter-group table entry 0xFFF84D68. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001074DC.

#### 0x001074F0 `init_pgrp_4d6c_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x0005A380 into parameter-group table entry 0xFFF84D6C. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001074F0.

#### 0x001074F8 `init_pgrp_4d6c_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x00187EA0 into parameter-group table entry 0xFFF84D6C. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x001074F8.

#### 0x0010750C `init_pgrp_4d70_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x0005CFD8 into parameter-group table entry 0xFFF84D70. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010750C.

#### 0x00107514 `init_pgrp_4d70_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x0018A520 into parameter-group table entry 0xFFF84D70. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107514.

#### 0x00107528 `init_pgrp_4d74_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x0005D090 into parameter-group table entry 0xFFF84D74. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107528.

#### 0x00107530 `init_pgrp_4d74_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x0018A5D8 into parameter-group table entry 0xFFF84D74. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107530.

#### 0x00107544 `init_pgrp_4d78_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x0005D0A4 into parameter-group table entry 0xFFF84D78. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107544.

#### 0x0010754C `init_pgrp_4d78_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x0018A5EC into parameter-group table entry 0xFFF84D78. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010754C.

#### 0x00107560 `init_pgrp_4d7c_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x0005D674 into parameter-group table entry 0xFFF84D7C. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107560.

#### 0x00107568 `init_pgrp_4d7c_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x0018AA78 into parameter-group table entry 0xFFF84D7C. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107568.

#### 0x0010757C `init_pgrp_4d80_rom` — stub
Four-instruction init stub from the boot parameter-table bank: writes the ROM default group image 0x0005D6A8 into parameter-group table entry 0xFFF84D80. The sibling stub installs the RAM copy; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x0010757C.

#### 0x00107584 `init_pgrp_4d80_ram` — stub
Four-instruction init stub from the boot parameter-table bank: writes the RAM working copy 0x0018AAAC into parameter-group table entry 0xFFF84D80. The sibling stub installs the ROM image; init dispatcher 0x000DA418 runs one of the pair. [O]
Cluster members: 0x00107584.

#### 0x00107598 `init_pgrp_4d84_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D6CC into parameter-group entry 0xFFF84D84 and stores the bitwise complement into integrity shadow word 0xFFF8D01C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107598.

#### 0x001075A8 `pgrp_def_4d84_rom` — stub
Two-instruction getter returning the ROM default pointer 0x0005D6CC for parameter-group entry 0xFFF84D84. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x001075A8.

#### 0x001075AC `init_pgrp_4d84_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AAD0 into parameter-group entry 0xFFF84D84 and stores the bitwise complement into integrity shadow word 0xFFF8D01C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001075AC.

#### 0x001075BC `pgrp_def_4d84_ram` — stub
Two-instruction getter returning the RAM-copy default pointer 0x0018AAD0 for parameter-group entry 0xFFF84D84. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x001075BC.

#### 0x001075CC `init_pgrp_4d88_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D728 into parameter-group entry 0xFFF84D88 and stores the bitwise complement into integrity shadow word 0xFFF8D020 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001075CC.

#### 0x001075DC `init_pgrp_4d88_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AB2C into parameter-group entry 0xFFF84D88 and stores the bitwise complement into integrity shadow word 0xFFF8D020 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001075DC.

#### 0x001075F8 `init_pgrp_4d8c_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D73C into parameter-group entry 0xFFF84D8C and stores the bitwise complement into integrity shadow word 0xFFF8D024 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001075F8.

#### 0x00107608 `pgrp_def_4d8c_rom` — stub
Two-instruction getter returning the ROM default pointer 0x0005D73C for parameter-group entry 0xFFF84D8C. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x00107608.

#### 0x0010760C `init_pgrp_4d8c_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AB40 into parameter-group entry 0xFFF84D8C and stores the bitwise complement into integrity shadow word 0xFFF8D024 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x0010760C.

#### 0x0010761C `pgrp_def_4d8c_ram` — stub
Two-instruction getter returning the RAM-copy default pointer 0x0018AB40 for parameter-group entry 0xFFF84D8C. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x0010761C.

#### 0x0010762C `init_pgrp_4d90_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D748 into parameter-group entry 0xFFF84D90 and stores the bitwise complement into integrity shadow word 0xFFF8D028 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x0010762C.

#### 0x0010763C `pgrp_def_4d90_rom` — stub
Two-instruction getter returning the ROM default pointer 0x0005D748 for parameter-group entry 0xFFF84D90. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x0010763C.

#### 0x00107640 `init_pgrp_4d90_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AB4C into parameter-group entry 0xFFF84D90 and stores the bitwise complement into integrity shadow word 0xFFF8D028 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107640.

#### 0x00107650 `pgrp_def_4d90_ram` — stub
Two-instruction getter returning the RAM-copy default pointer 0x0018AB4C for parameter-group entry 0xFFF84D90. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x00107650.

#### 0x00107654 `init_pgrp_4d94_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D750 into parameter-group entry 0xFFF84D94 and stores the bitwise complement into integrity shadow word 0xFFF8D02C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107654.

#### 0x00107664 `pgrp_def_4d94_rom` — stub
Two-instruction getter returning the ROM default pointer 0x0005D750 for parameter-group entry 0xFFF84D94. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x00107664.

#### 0x00107668 `init_pgrp_4d94_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AB54 into parameter-group entry 0xFFF84D94 and stores the bitwise complement into integrity shadow word 0xFFF8D02C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107668.

#### 0x00107678 `pgrp_def_4d94_ram` — stub
Two-instruction getter returning the RAM-copy default pointer 0x0018AB54 for parameter-group entry 0xFFF84D94. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x00107678.

#### 0x0010767C `init_pgrp_4d98_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D758 into parameter-group entry 0xFFF84D98 and stores the bitwise complement into integrity shadow word 0xFFF8D030 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x0010767C.

#### 0x0010768C `pgrp_def_4d98_rom` — stub
Two-instruction getter returning the ROM default pointer 0x0005D758 for parameter-group entry 0xFFF84D98. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x0010768C.

#### 0x00107690 `init_pgrp_4d98_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AB5C into parameter-group entry 0xFFF84D98 and stores the bitwise complement into integrity shadow word 0xFFF8D030 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107690.

#### 0x001076A0 `pgrp_def_4d98_ram` — stub
Two-instruction getter returning the RAM-copy default pointer 0x0018AB5C for parameter-group entry 0xFFF84D98. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x001076A0.

#### 0x001076A4 `init_pgrp_4d9c_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D760 into parameter-group entry 0xFFF84D9C and stores the bitwise complement into integrity shadow word 0xFFF8D034 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001076A4.

#### 0x001076B4 `pgrp_def_4d9c_rom` — stub
Two-instruction getter returning the ROM default pointer 0x0005D760 for parameter-group entry 0xFFF84D9C. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x001076B4.

#### 0x001076B8 `init_pgrp_4d9c_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AB64 into parameter-group entry 0xFFF84D9C and stores the bitwise complement into integrity shadow word 0xFFF8D034 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001076B8.

#### 0x001076C8 `pgrp_def_4d9c_ram` — stub
Two-instruction getter returning the RAM-copy default pointer 0x0018AB64 for parameter-group entry 0xFFF84D9C. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x001076C8.

#### 0x001076CC `init_pgrp_4da0_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D768 into parameter-group entry 0xFFF84DA0 and stores the bitwise complement into integrity shadow word 0xFFF8D038 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001076CC.

#### 0x001076DC `pgrp_def_4da0_rom` — stub
Two-instruction getter returning the ROM default pointer 0x0005D768 for parameter-group entry 0xFFF84DA0. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x001076DC.

#### 0x001076E0 `init_pgrp_4da0_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AB6C into parameter-group entry 0xFFF84DA0 and stores the bitwise complement into integrity shadow word 0xFFF8D038 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001076E0.

#### 0x001076F0 `pgrp_def_4da0_ram` — stub
Two-instruction getter returning the RAM-copy default pointer 0x0018AB6C for parameter-group entry 0xFFF84DA0. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x001076F0.

#### 0x00107730 `init_pgrp_4da4_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D770 into parameter-group entry 0xFFF84DA4 and stores the bitwise complement into integrity shadow word 0xFFF8D03C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107730.

#### 0x00107740 `init_pgrp_4da4_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AB74 into parameter-group entry 0xFFF84DA4 and stores the bitwise complement into integrity shadow word 0xFFF8D03C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107740.

#### 0x00107750 `init_pgrp_4da8_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D780 into parameter-group entry 0xFFF84DA8 and stores the bitwise complement into integrity shadow word 0xFFF8D040 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107750.

#### 0x00107760 `init_pgrp_4da8_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AB84 into parameter-group entry 0xFFF84DA8 and stores the bitwise complement into integrity shadow word 0xFFF8D040 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107760.

#### 0x00107770 `init_pgrp_4dac_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D790 into parameter-group entry 0xFFF84DAC and stores the bitwise complement into integrity shadow word 0xFFF8D044 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107770.

#### 0x00107780 `init_pgrp_4dac_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AB94 into parameter-group entry 0xFFF84DAC and stores the bitwise complement into integrity shadow word 0xFFF8D044 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107780.

#### 0x00107790 `init_pgrp_4db0_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D7A0 into parameter-group entry 0xFFF84DB0 and stores the bitwise complement into integrity shadow word 0xFFF8D048 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107790.

#### 0x001077A0 `init_pgrp_4db0_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018ABA4 into parameter-group entry 0xFFF84DB0 and stores the bitwise complement into integrity shadow word 0xFFF8D048 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001077A0.

#### 0x001077B0 `init_pgrp_4db4_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D7B0 into parameter-group entry 0xFFF84DB4 and stores the bitwise complement into integrity shadow word 0xFFF8D04C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001077B0.

#### 0x001077C0 `init_pgrp_4db4_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018ABB4 into parameter-group entry 0xFFF84DB4 and stores the bitwise complement into integrity shadow word 0xFFF8D04C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001077C0.

#### 0x0010780C `init_pgrp_4db8_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D7C0 into parameter-group entry 0xFFF84DB8 and stores the bitwise complement into integrity shadow word 0xFFF8D050 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x0010780C.

#### 0x0010781C `init_pgrp_4db8_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018ABC4 into parameter-group entry 0xFFF84DB8 and stores the bitwise complement into integrity shadow word 0xFFF8D050 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x0010781C.

#### 0x00107838 `init_pgrp_4dbc_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D7CC into parameter-group entry 0xFFF84DBC and stores the bitwise complement into integrity shadow word 0xFFF8D054 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107838.

#### 0x00107848 `init_pgrp_4dbc_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018ABD0 into parameter-group entry 0xFFF84DBC and stores the bitwise complement into integrity shadow word 0xFFF8D054 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107848.

#### 0x00107858 `init_pgrp_4dc0_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D814 into parameter-group entry 0xFFF84DC0 and stores the bitwise complement into integrity shadow word 0xFFF8D058 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107858.

#### 0x00107868 `init_pgrp_4dc0_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AC18 into parameter-group entry 0xFFF84DC0 and stores the bitwise complement into integrity shadow word 0xFFF8D058 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107868.

#### 0x00107878 `init_pgrp_4dc4_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D85C into parameter-group entry 0xFFF84DC4 and stores the bitwise complement into integrity shadow word 0xFFF8D05C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107878.

#### 0x00107888 `init_pgrp_4dc4_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AC60 into parameter-group entry 0xFFF84DC4 and stores the bitwise complement into integrity shadow word 0xFFF8D05C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107888.

#### 0x00107898 `init_pgrp_4dc8_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D8A4 into parameter-group entry 0xFFF84DC8 and stores the bitwise complement into integrity shadow word 0xFFF8D060 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107898.

#### 0x001078A8 `init_pgrp_4dc8_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018ACA8 into parameter-group entry 0xFFF84DC8 and stores the bitwise complement into integrity shadow word 0xFFF8D060 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001078A8.

#### 0x001078B8 `init_pgrp_4dcc_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D8EC into parameter-group entry 0xFFF84DCC and stores the bitwise complement into integrity shadow word 0xFFF8D064 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001078B8.

#### 0x001078C8 `init_pgrp_4dcc_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018ACF0 into parameter-group entry 0xFFF84DCC and stores the bitwise complement into integrity shadow word 0xFFF8D064 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001078C8.

#### 0x00107914 `init_pgrp_4dd0_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D934 into parameter-group entry 0xFFF84DD0 and stores the bitwise complement into integrity shadow word 0xFFF8D068 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107914.

#### 0x00107924 `init_pgrp_4dd0_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AD38 into parameter-group entry 0xFFF84DD0 and stores the bitwise complement into integrity shadow word 0xFFF8D068 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107924.

#### 0x00107934 `init_pgrp_4dd4_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D948 into parameter-group entry 0xFFF84DD4 and stores the bitwise complement into integrity shadow word 0xFFF8D06C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107934.

#### 0x00107944 `init_pgrp_4dd4_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AD4C into parameter-group entry 0xFFF84DD4 and stores the bitwise complement into integrity shadow word 0xFFF8D06C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107944.

#### 0x00107954 `init_pgrp_4dd8_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D95C into parameter-group entry 0xFFF84DD8 and stores the bitwise complement into integrity shadow word 0xFFF8D070 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107954.

#### 0x00107964 `init_pgrp_4dd8_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AD60 into parameter-group entry 0xFFF84DD8 and stores the bitwise complement into integrity shadow word 0xFFF8D070 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107964.

#### 0x00107974 `init_pgrp_4ddc_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D970 into parameter-group entry 0xFFF84DDC and stores the bitwise complement into integrity shadow word 0xFFF8D074 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107974.

#### 0x00107984 `init_pgrp_4ddc_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AD74 into parameter-group entry 0xFFF84DDC and stores the bitwise complement into integrity shadow word 0xFFF8D074 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107984.

#### 0x00107994 `init_pgrp_4de0_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D984 into parameter-group entry 0xFFF84DE0 and stores the bitwise complement into integrity shadow word 0xFFF8D078 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107994.

#### 0x001079A4 `init_pgrp_4de0_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AD88 into parameter-group entry 0xFFF84DE0 and stores the bitwise complement into integrity shadow word 0xFFF8D078 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001079A4.

#### 0x001079F0 `init_pgrp_4de4_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D998 into parameter-group entry 0xFFF84DE4 and stores the bitwise complement into integrity shadow word 0xFFF8D07C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x001079F0.

#### 0x00107A00 `pgrp_def_4de4_rom` — stub
Two-instruction getter returning the ROM default pointer 0x0005D998 for parameter-group entry 0xFFF84DE4. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x00107A00.

#### 0x00107A04 `init_pgrp_4de4_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018AD9C into parameter-group entry 0xFFF84DE4 and stores the bitwise complement into integrity shadow word 0xFFF8D07C (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107A04.

#### 0x00107A14 `pgrp_def_4de4_ram` — stub
Two-instruction getter returning the RAM-copy default pointer 0x0018AD9C for parameter-group entry 0xFFF84DE4. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x00107A14.

#### 0x00107A24 `init_pgrp_4de8_rom` — data-accessor
Seven-instruction table setter: writes the ROM default group image 0x0005D9A0 into parameter-group entry 0xFFF84DE8 and stores the bitwise complement into integrity shadow word 0xFFF8D080 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107A24.

#### 0x00107A34 `pgrp_def_4de8_rom` — stub
Two-instruction getter returning the ROM default pointer 0x0005D9A0 for parameter-group entry 0xFFF84DE8. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x00107A34.

#### 0x00107A38 `init_pgrp_4de8_ram` — data-accessor
Seven-instruction table setter: writes the RAM working copy 0x0018ADA4 into parameter-group entry 0xFFF84DE8 and stores the bitwise complement into integrity shadow word 0xFFF8D080 (checked by monitor 0x0011D44C). Sibling installs the alternate default; dispatcher 0x000DA418 selects. [O]
Cluster members: 0x00107A38.

#### 0x00107A48 `pgrp_def_4de8_ram` — stub
Two-instruction getter returning the RAM-copy default pointer 0x0018ADA4 for parameter-group entry 0xFFF84DE8. Consumed by the periodic integrity monitor 0x0011D44C, which double-reads and faults on persistent mismatch. [O]
Cluster members: 0x00107A48.

#### 0x00107A58 `rseq_modeq_en_step` — logic
Wrapper for the rseq mode qualifier: passes enable=1 when param_2 is outside {0,1}, else 0, into `rseq_modeq_step` 0x00107AA2. Called by `rseq_step` 0x000D8122 (unit 28's 9-state sequencer). 4-byte state {flag, mapped, mode, prev}. [O]
Cluster members: 0x00107A58.

#### 0x00107A6A `rseq_modeq_init` — logic
Initializes the 4-byte rseq mode qualifier: flag=0, mapped=0, mode=prev=6 (code 6 maps to 3). Called by factory 0x00107A7A and directly by `rseq_step` 0x000D8122. [O]
Cluster members: 0x00107A6A.

#### 0x00107A7A `rseq_modeq_new` — logic
Factory for the rseq mode qualifier: allocates 4 bytes via operator new 0x0009AA34 when param_1==0, initializes via 0x00107A6A, returns the object. callers='-' so it is reached through the sequencer framework's pointer tables. [O]
Cluster members: 0x00107A7A.

#### 0x00107AA2 `rseq_modeq_step` — logic
Steps the 4-byte mode qualifier: copies mode history, loads the live mode byte from 0xFFF89CDD, resets the change flag, maps codes via 0x00107AF2, then sets flag=1 when enabled and (group 0xFFF84CF4 byte-table check 0x00107B2A on entry mapped-1 is 1, or a transition to mode 8 while group[1]==1). [O]
Cluster members: 0x00107AA2.

#### 0x00107AF2 `rseq_modeq_map_code` — logic
Maps qualifier mode codes 8/7/6/5 -> 1/2/3/4 into state byte +1, only on change; other codes pass through unchanged. Called only by `rseq_modeq_step` 0x00107AA2. [O]
Cluster members: 0x00107AF2.

#### 0x00107B2A `rseq_modeq_tbl_check` — logic
Indexes the u8 table behind parameter-group entry 0xFFF84CF4 (1..4 -> 0..3) via `tbl_read_u8` 0x000B6C8A (spec 09 byte-array cal format) and returns entry!=0. Used by `rseq_modeq_step` as the change-qualifier mask. [O] Spec 09 cal-table access.
Cluster members: 0x00107B2A.

#### 0x00107B80 `sol_pressure_ctor` — logic
Constructor of the pressure (element VFS regulator) actuator class: allocates 0x28 bytes (operator new 0x0009AA34) if param_1==0, calls base `solcur_obj_new` 0x000D9C54 (channel+id), installs vtable 0x0004AED8 at +8 and the request pointer at +0xc. Instances at 0xFFF8B154.. (channels 0/2/4/1/3 = solenoids A/B/C/D/E). [O] Spec 12 sect.1.2 object table.
Cluster members: 0x00107B80.

#### 0x00107BD2 `sol_elem_step` — logic
Element-regulator cyclic step (vtable method, callers='-' via vtable 0x4AED8): reads request (+0xc) pressure s16 +2 and mode u8 +7. Mode 1 -> fast-fill SM `sol_elem_fastfill_sm` 0x00107D90 then (state==1 ? float ramp 0x000E4CE2+`rampt_f_step2` 0x000D1CDC x1000 : min-current 0x00107CFC); else fastfill(0)+`sol_elem_current_calc` 0x00107C3E. Result passes identity hook 0x00107D8E. [O] Spec 12 sect.3.
Cluster members: 0x00107BD2.

#### 0x00107C3E `sol_elem_current_calc` — logic
Element current calculation (spec 12 sect.3): base = `solcur_demand_curve` 0x000D9D18(p); channel 2 adds map 0x000D9EF6; invalid channel (not 0/1/3/4/2) reports event 0x1C4 id 10 and zeroes; adds temp correction 0x000D9F96, supply correction 0x000D9F10, clamps s16 (0x000B7888) then u16 (0x000B785C), finally min/max clamp 0x00107D6A. [O] Spec 12 sect.3.
Cluster members: 0x00107C3E.

#### 0x00107CFC `sol_elem_min_current` — logic
Element minimum/limitation current: request 0 with limitation flag +1 set -> `solcur_entry1_get` 0x000D9CEE (i_min); nonzero request: if below the state short (0x000BAA58) and below 0x31CE (12750 mbar) reports plausibility event 0x1C4 id 15; otherwise returns i_min or `solcur_mode56_value` 0x000D9CBC per flag +1. Called from step 0x00107BD2 and fast-fill 0x00107D90. [O] Spec 12 plausibility rule.
Cluster members: 0x00107CFC.

#### 0x00107D6A `sol_i_clamp_minmax` — library
Clamps the solenoid current value between group 0xFFF84C94 limits: clamp_u8 0x000B7786(param, `solcur_mode56_value` 0x000D9CBC (i_min=500), `solcur_entry1_get` 0x000D9CEE (i_max=8500)). Called from both current calculators and the fast-fill SM. [O] Spec 12 sect.3 clamp.
Cluster members: 0x00107D6A, 0x0010810A.

#### 0x00107D8E `sol_step_hook_ident` — stub
One-instruction identity (returns param_2): the vtable post-step hook slot for the pressure class, called at the end of `sol_elem_step` 0x00107BD2. Twin 0x0010812E serves the small (WK/Sys) class. [O]
Cluster members: 0x00107D8E, 0x0010812E.

#### 0x00107D90 `sol_elem_fastfill_sm` — logic
Fast-fill/step-mode state machine (state byte +0x10, 0/1/2), run until state settles: state 0 with fill request (request+0x5a==1) computes current from fill pressure request+0x42 via 0x00107C3E, initializes float ramp 0x000D1CB8 on +0x14 with value*10+0x000E4CE2(request), clamps 0x00107D6A and takes min-current 0x00107CFC -> state 1; state 1 arms timer 0x000BAD8C on +0x24 and exits to 2 when it expires, current reaches minimum, request+0x5a clears, or param_2==0. [O] Spec 12 sect.3 fast fill.
Cluster members: 0x00107D90.

#### 0x00107EFE `sol_onoff_ctor` — logic
Constructor of the on/off actuator class: allocates 0x10 bytes (operator new 0x0009AA34), base `solcur_obj_new` 0x000D9C54(this,ch,0), vtable 0x0004AEFC at +8, request byte param_3 at +0xc. Instances 0xFFF8B23C (ch7 MV Pos park release), 0xFFF8B24C (ch11 park hold), 0xFFF8B25C (ch10 open). [O] Spec 12 sect.1.2.
Cluster members: 0x00107EFE.

#### 0x00107F6E `sol_small_ctor` — logic
Constructor of the small (WK/Sys supply regulator) class: allocates 0x10 bytes (operator new 0x0009AA34), base `solcur_obj_new` 0x000D9C54(this,ch,type), vtable 0x0004AF20 at +8, request pointer param_4 at +0xc. Instances 0xFFF8B21C (ch5 WK), 0xFFF8B22C (ch6 Sys). [O] Spec 12 sect.1.2/sect.3.
Cluster members: 0x00107F6E.

#### 0x00107FB8 `sol_small_step` — logic
Small (WK/Sys) regulator cyclic step (vtable method via 0x4AF20): request mode u8 +7 == 0 -> `sol_small_current_calc` 0x00107FE0 with pressure s16 +2, else `sol_small_min_current` 0x0010809C; result passes identity hook 0x0010812E. [O] Spec 12 sect.3.
Cluster members: 0x00107FB8.

#### 0x00107FE0 `sol_small_current_calc` — logic
WK/Sys current calculation (spec 12 sect.3): base = `solcur_demand_curve` 0x000D9D18(p); channel 5 (WK) adds offset getter 0x000BA310 (0xFFF8E612); channel 6 (Sys) instead adds p x engine-speed map 0x000D9F44; other channels report event 0x1C2 id 10 and return 0; adds temp correction 0x000D9F96, clamps s16/u16 (0x000B7888/0x000B785C), final min/max clamp 0x0010810A. [O] Spec 12 sect.3.
Cluster members: 0x00107FE0.

#### 0x0010809C `sol_small_min_current` — logic
WK/Sys mirror of `sol_elem_min_current` 0x00107CFC: request 0 + limitation flag -> i_min 0x000D9CEE; low request below state short (0x000BAA58) and below 0x31CE (12750 mbar) reports event 0x1C2 id 0xb3; else i_min or `solcur_mode56_value` 0x000D9CBC. [O]
Cluster members: 0x0010809C.

#### 0x00108184 `solcur_cycle_step` — logic
Solenoid-current subsystem cycle step (called from task body 0x000DC4C4): first `solcur_deact_if_flag` 0x000DA0C4 on the channel-set object 0xFFF8E328; then evaluates activation via 0x00108260 (+0x108294) and steps 0x00108470 (state 0) or 0x00108462 on the manager object 0xFFF8E4B6, finishing with 0x0010843C. Callees 0x1082xx-0x1084xx belong to the next unit. [O]
Cluster members: 0x00108184.

#### 0x001081C0 `solcur_debounce_wrap` — logic
Wrapper: `solcur_debounce_step` 0x000DA10A on the channel set 0xFFF8E328 with param_2, then 0x00108574 on the manager 0xFFF8E4B6 with param_3. Entered from method slot 0x000DC3AA. [O]
Cluster members: 0x001081C0.

#### 0x001081DC `solcur_latch_wrap` — logic
Wrapper: `solcur_latch_step` 0x000DA0E2 on the channel set 0xFFF8E328, then 0x00108574 on the manager 0xFFF8E4B6 with param_2. Reached via 3-instr wrapper 0x001081F6. [O]
Cluster members: 0x001081DC.

#### 0x001081F6 `solcur_latch_wrap_def` — stub
Three-instruction wrapper: calls `solcur_latch_wrap` 0x001081DC with the default byte from 0xFFF89C2B. Method slot 0x000DC45E. [O]
Cluster members: 0x001081F6.

#### 0x001081FC `solcur_deactivate_wrap` — logic
Wrapper: `solcur_deactivate` 0x000DA152 on the channel set 0xFFF8E328, then 0x00108454 on the manager 0xFFF8E4B6. Cluster twin 0x00108212 calls `solcur_disable` 0x000DA16C instead. Method slot 0x000DC45E. [O]
Cluster members: 0x001081FC, 0x00108212.

#### 0x00108224 `solcur_cond_deact_wrap` — logic
Wrapper: conditional deactivate 0x000DA1A8 (skips when state 5 keeps the channel latched) on the channel set 0xFFF8E328, then indirect thunk 0x0010858A to 0x00108454 on the manager 0xFFF8E4B6. Method slot 0x000DC472. [O]
Cluster members: 0x00108224.

#### 0x00108234 `solcur_latch_gated` — logic
`solcur_latch_step` 0x000DA0E2 on the channel set 0xFFF8E328; only when flag 0xFFF89C2F is 0 does it also run 0x00108574 on the manager 0xFFF8E4B6 with param_2. Method slot 0x000DC47E. [O]
Cluster members: 0x00108234.

#### 0x00108260 `solcur_active_check` — logic
Activation predicate for `solcur_cycle_step` 0x00108184: true when the manager pointer 0xFFF8E4B6 is set AND constant check 0x001084E4 passes AND 0x0010825C(param_1) is true. [O]
Cluster members: 0x00108260.

## Unit summary

220 clusters, 223 named functions (0x00106ED0-0x00108260), 0 unknown.

Kind counts: stub 145, data-accessor 52, logic 24, library 2. (No irq/entry/dispatch; nothing here
is vector-reached — the whole unit is called from 0x000DA418, 0x0011D44C, 0x000D8122, vtables
0x4AED8/0x4AEFC/0x4AF20, and method-slot wrappers 0x000DC3AA-0x000DC4C4.)

The unit is two blocks:

1. **Parameter-group init bank** (0x00106ED0-0x00107A48, 193 functions): every entry writes one
   word of the calibration pointer table 0xFFF84C8C-0xFFF84DE8. Entries 0x00106ED0-0x00107584 are
   4-instruction `*entry = constant` stubs in rom/ram pairs (ROM image 0x53xxx-0x5Dxxx vs
   byte-identical RAM copy 0x182xxx-0x18Axxx — verified in the ELF); entries 0x00107598-0x00107A38
   additionally write `~value` into shadow words 0xFFF8D01C-0xFFF8D080; 18 two-instruction getters
   (0x001075A8-0x00107A48) return the same defaults for the periodic integrity monitor 0x0011D44C.
   Init dispatcher 0x000DA418 (r0c_app, 1461 instrs) calls the whole bank; which member of each
   rom/ram pair runs is the variant/calibration selection — worth confirming from its switch input.
   Spec-known group entries in this table: 0xFFF84C94 (spec 12 solenoid i_min/i_max), 0xFFF84CC4/
   0xFFF84CDC/0xFFF84CF8 (spec 11 shift thresholds), 0xFFF84CF4 (rseq mode-qualifier mask table,
   read by 0x00107B2A), 0xFFF84D14/0xFFF84D18 (spec 12 sol temp-correction / curve select).

2. **Solenoid actuator classes** (0x00107A58-0x00108260, 30 functions, spec 12): rseq mode
   qualifier (factory/init/step/map/table-check, called from 0x000D8122); the three actuator
   constructors `sol_pressure_ctor` (0x28-byte element VFS regulators, vtable 0x4AED8),
   `sol_onoff_ctor` (vtable 0x4AEFC), `sol_small_ctor` (WK/Sys, vtable 0x4AF20); element step +
   current calc + min-current + fast-fill SM; small step + current calc + min-current; and the
   solcur channel-set cycle wrappers over globals 0xFFF8E328/0xFFF8E4B6.

Cross-region leads:
- Unit owning 0x1082xx-0x1085xx (0x108294, 0x10843C, 0x108454, 0x108462, 0x108470, 0x108574,
  0x10858A): these are the solcur manager/channel helpers my wrappers call — cross-check against
  `solcur_cycle_step` 0x00108184 and `solcur_active_check` 0x00108260.
- Unit owning r0c_app 0x000DA418: the init dispatcher's pair selection logic decides rom-vs-ram
  defaults for all 92 table entries documented here.
- Unit owning r0c_app 0x000DC3AA-0x000DC4C4 / 0x000B83CA-0x000B85C8: method slots that drive the
  solcur wrappers in this unit.
- 0x0011D44C (r10_app, later unit) is the integrity monitor consuming the `pgrp_def_*` getters.
