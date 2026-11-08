# Unit 39 (r10_app)

Region r10_app clusters 0x0011D984-0x00122946. The first 117 clusters are the calibration-source
selector stub bank (spec 09 §1.1): pairs switching each calibration group pointer between the
application copy (index at 0x56C90) and the calibration copy (0x180280) of the 12,198-entry
object index; all called from 0xF7950 on selector byte 0xFFF8A075 change.
#### 0x0011D984 `tbl_sel_fff84e7c_cal` — stub
Four-instruction selector stub: stores src 0x0018B4B0 into calibration group pointer 0xFFF84E7C, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Lone calibration-only selector here; no adjacent app twin (the struct-pointer pairs at 0x107250/0x107258 use a different caller). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011D998 `tbl_sel_fff84e80_app` — stub
Four-instruction selector stub: stores src 0x00061EF4 into calibration group pointer 0xFFF84E80, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011D9A0 `tbl_sel_fff84e80_cal` — stub
Four-instruction selector stub: stores src 0x0018B4E4 into calibration group pointer 0xFFF84E80, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011D9B4 `tbl_sel_fff84e84_app` — stub
Four-instruction selector stub: stores src 0x00061F0C into calibration group pointer 0xFFF84E84, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011D9BC `tbl_sel_fff84e84_cal` — stub
Four-instruction selector stub: stores src 0x0018B4FC into calibration group pointer 0xFFF84E84, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011D9D0 `tbl_sel_fff84e88_app` — stub
Four-instruction selector stub: stores src 0x00061F58 into calibration group pointer 0xFFF84E88, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011D9D8 `tbl_sel_fff84e88_cal` — stub
Four-instruction selector stub: stores src 0x0018B548 into calibration group pointer 0xFFF84E88, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011D9EC `tbl_sel_fff84e8c_app` — stub
Four-instruction selector stub: stores src 0x00061FC4 into calibration group pointer 0xFFF84E8C, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011D9F4 `tbl_sel_fff84e8c_cal` — stub
Four-instruction selector stub: stores src 0x0018B5B4 into calibration group pointer 0xFFF84E8C, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DA08 `tbl_sel_fff84e90_app` — stub
Four-instruction selector stub: stores src 0x00061FF8 into calibration group pointer 0xFFF84E90, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DA10 `tbl_sel_fff84e90_cal` — stub
Four-instruction selector stub: stores src 0x0018B5E8 into calibration group pointer 0xFFF84E90, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DA24 `tbl_sel_fff84e94_app` — stub
Four-instruction selector stub: stores src 0x00062094 into calibration group pointer 0xFFF84E94, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DA2C `tbl_sel_fff84e94_cal` — stub
Four-instruction selector stub: stores src 0x0018B684 into calibration group pointer 0xFFF84E94, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DA40 `tbl_sel_fff84e98_app` — stub
Four-instruction selector stub: stores src 0x000620D0 into calibration group pointer 0xFFF84E98, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DA48 `tbl_sel_fff84e98_cal` — stub
Four-instruction selector stub: stores src 0x0018B6C0 into calibration group pointer 0xFFF84E98, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DA5C `tbl_sel_fff84e9c_app` — stub
Four-instruction selector stub: stores src 0x000620F4 into calibration group pointer 0xFFF84E9C, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DA64 `tbl_sel_fff84e9c_cal` — stub
Four-instruction selector stub: stores src 0x0018B6E4 into calibration group pointer 0xFFF84E9C, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DA78 `tbl_sel_fff84ea0_app` — stub
Four-instruction selector stub: stores src 0x00062164 into calibration group pointer 0xFFF84EA0, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DA80 `tbl_sel_fff84ea0_cal` — stub
Four-instruction selector stub: stores src 0x0018B754 into calibration group pointer 0xFFF84EA0, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DA94 `tbl_sel_fff84ea4_app` — stub
Four-instruction selector stub: stores src 0x000621EC into calibration group pointer 0xFFF84EA4, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DA9C `tbl_sel_fff84ea4_cal` — stub
Four-instruction selector stub: stores src 0x0018B7DC into calibration group pointer 0xFFF84EA4, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DAB0 `tbl_sel_fff84ea8_app` — stub
Four-instruction selector stub: stores src 0x00062228 into calibration group pointer 0xFFF84EA8, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DAB8 `tbl_sel_fff84ea8_cal` — stub
Four-instruction selector stub: stores src 0x0018B818 into calibration group pointer 0xFFF84EA8, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DACC `tbl_sel_fff84eac_app` — stub
Four-instruction selector stub: stores src 0x00062254 into calibration group pointer 0xFFF84EAC, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DAD4 `tbl_sel_fff84eac_cal` — stub
Four-instruction selector stub: stores src 0x0018B844 into calibration group pointer 0xFFF84EAC, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.
#### 0x0011DAE8 `tbl_sel_fff84eb0_app` — stub
Four-instruction selector stub: stores src 0x00062278 into calibration group pointer 0xFFF84EB0, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). This group is the shift-map group (spec 09 §2.1, index 11642-11748). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DAF0 `tbl_sel_fff84eb0_cal` — stub
Four-instruction selector stub: stores src 0x0018B868 into calibration group pointer 0xFFF84EB0, selecting the calibration copy of the object index (0x180280 block). This group is the shift-map group (spec 09 §2.1, index 11642-11748). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DB04 `tbl_sel_fff84eb4_app` — stub
Four-instruction selector stub: stores src 0x00062424 into calibration group pointer 0xFFF84EB4, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DB0C `tbl_sel_fff84eb4_cal` — stub
Four-instruction selector stub: stores src 0x0018BA14 into calibration group pointer 0xFFF84EB4, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DB20 `tbl_sel_fff84eb8_app` — stub
Four-instruction selector stub: stores src 0x00062428 into calibration group pointer 0xFFF84EB8, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DB28 `tbl_sel_fff84eb8_cal` — stub
Four-instruction selector stub: stores src 0x0018BA18 into calibration group pointer 0xFFF84EB8, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DB3C `tbl_sel_fff84ebc_app` — stub
Four-instruction selector stub: stores src 0x0006242C into calibration group pointer 0xFFF84EBC, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). This group is the manual-mode/tip shift-limit group (spec 09 §2.5, index 11751-11806). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DB44 `tbl_sel_fff84ebc_cal` — stub
Four-instruction selector stub: stores src 0x0018BA1C into calibration group pointer 0xFFF84EBC, selecting the calibration copy of the object index (0x180280 block). This group is the manual-mode/tip shift-limit group (spec 09 §2.5, index 11751-11806). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DB58 `tbl_sel_fff84ec0_app` — stub
Four-instruction selector stub: stores src 0x0006250C into calibration group pointer 0xFFF84EC0, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DB60 `tbl_sel_fff84ec0_cal` — stub
Four-instruction selector stub: stores src 0x0018BAFC into calibration group pointer 0xFFF84EC0, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DB74 `tbl_sel_fff84ec4_app` — stub
Four-instruction selector stub: stores src 0x00062510 into calibration group pointer 0xFFF84EC4, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DB7C `tbl_sel_fff84ec4_cal` — stub
Four-instruction selector stub: stores src 0x0018BB00 into calibration group pointer 0xFFF84EC4, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DB90 `tbl_sel_fff84ec8_app` — stub
Four-instruction selector stub: stores src 0x00062524 into calibration group pointer 0xFFF84EC8, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DB98 `tbl_sel_fff84ec8_cal` — stub
Four-instruction selector stub: stores src 0x0018BB14 into calibration group pointer 0xFFF84EC8, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DBAC `tbl_sel_fff84ecc_app` — stub
Four-instruction selector stub: stores src 0x00062534 into calibration group pointer 0xFFF84ECC, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DBB4 `tbl_sel_fff84ecc_cal` — stub
Four-instruction selector stub: stores src 0x0018BB24 into calibration group pointer 0xFFF84ECC, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DBC8 `tbl_sel_fff84ed0_app` — stub
Four-instruction selector stub: stores src 0x00062540 into calibration group pointer 0xFFF84ED0, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DBD0 `tbl_sel_fff84ed0_cal` — stub
Four-instruction selector stub: stores src 0x0018BB30 into calibration group pointer 0xFFF84ED0, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DBE4 `tbl_sel_fff84ed4_app` — stub
Four-instruction selector stub: stores src 0x00062570 into calibration group pointer 0xFFF84ED4, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DBEC `tbl_sel_fff84ed4_cal` — stub
Four-instruction selector stub: stores src 0x0018BB60 into calibration group pointer 0xFFF84ED4, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DC00 `tbl_sel_fff84ed8_app` — stub
Four-instruction selector stub: stores src 0x00062594 into calibration group pointer 0xFFF84ED8, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DC08 `tbl_sel_fff84ed8_cal` — stub
Four-instruction selector stub: stores src 0x0018BB84 into calibration group pointer 0xFFF84ED8, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DC1C `tbl_sel_fff84edc_app` — stub
Four-instruction selector stub: stores src 0x000625B4 into calibration group pointer 0xFFF84EDC, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DC24 `tbl_sel_fff84edc_cal` — stub
Four-instruction selector stub: stores src 0x0018BBA4 into calibration group pointer 0xFFF84EDC, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DC38 `tbl_sel_fff84ee0_app` — stub
Four-instruction selector stub: stores src 0x000625C0 into calibration group pointer 0xFFF84EE0, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.
#### 0x0011DC40 `tbl_sel_fff84ee0_cal` — stub
Four-instruction selector stub: stores src 0x0018BBB0 into calibration group pointer 0xFFF84EE0, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DC54 `tbl_sel_fff84ee4_app` — stub
Four-instruction selector stub: stores src 0x000625D4 into calibration group pointer 0xFFF84EE4, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DC5C `tbl_sel_fff84ee4_cal` — stub
Four-instruction selector stub: stores src 0x0018BBC4 into calibration group pointer 0xFFF84EE4, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DC70 `tbl_sel_fff84ee8_app` — stub
Four-instruction selector stub: stores src 0x000625D8 into calibration group pointer 0xFFF84EE8, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DC78 `tbl_sel_fff84ee8_cal` — stub
Four-instruction selector stub: stores src 0x0018BBC8 into calibration group pointer 0xFFF84EE8, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DC8C `tbl_sel_fff84eec_app` — stub
Four-instruction selector stub: stores src 0x0006262C into calibration group pointer 0xFFF84EEC, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DC94 `tbl_sel_fff84eec_cal` — stub
Four-instruction selector stub: stores src 0x0018BC1C into calibration group pointer 0xFFF84EEC, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DCA8 `tbl_sel_fff84ef0_app` — stub
Four-instruction selector stub: stores src 0x00062644 into calibration group pointer 0xFFF84EF0, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DCB0 `tbl_sel_fff84ef0_cal` — stub
Four-instruction selector stub: stores src 0x0018BC34 into calibration group pointer 0xFFF84EF0, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DCC4 `tbl_sel_fff84ef4_app` — stub
Four-instruction selector stub: stores src 0x00062688 into calibration group pointer 0xFFF84EF4, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DCCC `tbl_sel_fff84ef4_cal` — stub
Four-instruction selector stub: stores src 0x0018BC78 into calibration group pointer 0xFFF84EF4, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DCE0 `tbl_sel_fff84ef8_app` — stub
Four-instruction selector stub: stores src 0x000626C8 into calibration group pointer 0xFFF84EF8, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DCE8 `tbl_sel_fff84ef8_cal` — stub
Four-instruction selector stub: stores src 0x0018BCB8 into calibration group pointer 0xFFF84EF8, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DCFC `tbl_sel_fff84efc_app` — stub
Four-instruction selector stub: stores src 0x000626E8 into calibration group pointer 0xFFF84EFC, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DD04 `tbl_sel_fff84efc_cal` — stub
Four-instruction selector stub: stores src 0x0018BCD8 into calibration group pointer 0xFFF84EFC, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DD18 `tbl_sel_fff84f00_app` — stub
Four-instruction selector stub: stores src 0x000626EC into calibration group pointer 0xFFF84F00, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DD20 `tbl_sel_fff84f00_cal` — stub
Four-instruction selector stub: stores src 0x0018BCDC into calibration group pointer 0xFFF84F00, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DD34 `tbl_sel_fff84f04_app` — stub
Four-instruction selector stub: stores src 0x00062728 into calibration group pointer 0xFFF84F04, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DD3C `tbl_sel_fff84f04_cal` — stub
Four-instruction selector stub: stores src 0x0018BD18 into calibration group pointer 0xFFF84F04, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DD50 `tbl_sel_fff84f08_app` — stub
Four-instruction selector stub: stores src 0x00062730 into calibration group pointer 0xFFF84F08, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). This group is the rev-limit forced-upshift group (spec 09 §2.5, index 11944-11946). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DD58 `tbl_sel_fff84f08_cal` — stub
Four-instruction selector stub: stores src 0x0018BD20 into calibration group pointer 0xFFF84F08, selecting the calibration copy of the object index (0x180280 block). This group is the rev-limit forced-upshift group (spec 09 §2.5, index 11944-11946). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DD6C `tbl_sel_fff84f0c_app` — stub
Four-instruction selector stub: stores src 0x0006273C into calibration group pointer 0xFFF84F0C, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DD74 `tbl_sel_fff84f0c_cal` — stub
Four-instruction selector stub: stores src 0x0018BD2C into calibration group pointer 0xFFF84F0C, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DD88 `tbl_sel_fff84f10_app` — stub
Four-instruction selector stub: stores src 0x00062744 into calibration group pointer 0xFFF84F10, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DD90 `tbl_sel_fff84f10_cal` — stub
Four-instruction selector stub: stores src 0x0018BD34 into calibration group pointer 0xFFF84F10, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.
#### 0x0011DDA4 `tbl_sel_fff84f14_app` — stub
Four-instruction selector stub: stores src 0x0006276C into calibration group pointer 0xFFF84F14, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DDAC `tbl_sel_fff84f14_cal` — stub
Four-instruction selector stub: stores src 0x0018BD5C into calibration group pointer 0xFFF84F14, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DDC0 `tbl_sel_fff84f18_app` — stub
Four-instruction selector stub: stores src 0x00062778 into calibration group pointer 0xFFF84F18, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DDC8 `tbl_sel_fff84f18_cal` — stub
Four-instruction selector stub: stores src 0x0018BD68 into calibration group pointer 0xFFF84F18, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DDDC `tbl_sel_fff84f1c_app` — stub
Four-instruction selector stub: stores src 0x000627F8 into calibration group pointer 0xFFF84F1C, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DDE4 `tbl_sel_fff84f1c_cal` — stub
Four-instruction selector stub: stores src 0x0018BDE8 into calibration group pointer 0xFFF84F1C, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DDF8 `tbl_sel_fff84f20_app` — stub
Four-instruction selector stub: stores src 0x000627FC into calibration group pointer 0xFFF84F20, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DE00 `tbl_sel_fff84f20_cal` — stub
Four-instruction selector stub: stores src 0x0018BDEC into calibration group pointer 0xFFF84F20, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DE14 `tbl_sel_fff84f24_app` — stub
Four-instruction selector stub: stores src 0x00062808 into calibration group pointer 0xFFF84F24, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DE1C `tbl_sel_fff84f24_cal` — stub
Four-instruction selector stub: stores src 0x0018BDF8 into calibration group pointer 0xFFF84F24, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DE30 `tbl_sel_fff84f28_app` — stub
Four-instruction selector stub: stores src 0x00062810 into calibration group pointer 0xFFF84F28, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DE38 `tbl_sel_fff84f28_cal` — stub
Four-instruction selector stub: stores src 0x0018BE00 into calibration group pointer 0xFFF84F28, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DE4C `tbl_sel_fff84f2c_app` — stub
Four-instruction selector stub: stores src 0x00062864 into calibration group pointer 0xFFF84F2C, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DE54 `tbl_sel_fff84f2c_cal` — stub
Four-instruction selector stub: stores src 0x0018BE54 into calibration group pointer 0xFFF84F2C, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DE68 `tbl_sel_fff84f30_app` — stub
Four-instruction selector stub: stores src 0x00062894 into calibration group pointer 0xFFF84F30, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DE70 `tbl_sel_fff84f30_cal` — stub
Four-instruction selector stub: stores src 0x0018BE84 into calibration group pointer 0xFFF84F30, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DE84 `tbl_sel_fff84f34_app` — stub
Four-instruction selector stub: stores src 0x00062970 into calibration group pointer 0xFFF84F34, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DE8C `tbl_sel_fff84f34_cal` — stub
Four-instruction selector stub: stores src 0x0018BF60 into calibration group pointer 0xFFF84F34, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DEA0 `tbl_sel_fff84f38_app` — stub
Four-instruction selector stub: stores src 0x00062984 into calibration group pointer 0xFFF84F38, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DEA8 `tbl_sel_fff84f38_cal` — stub
Four-instruction selector stub: stores src 0x0018BF74 into calibration group pointer 0xFFF84F38, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DEBC `tbl_sel_fff84f3c_app` — stub
Four-instruction selector stub: stores src 0x0006298C into calibration group pointer 0xFFF84F3C, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DEC4 `tbl_sel_fff84f3c_cal` — stub
Four-instruction selector stub: stores src 0x0018BF7C into calibration group pointer 0xFFF84F3C, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DED8 `tbl_sel_fff84f40_app` — stub
Four-instruction selector stub: stores src 0x000629D8 into calibration group pointer 0xFFF84F40, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DEE0 `tbl_sel_fff84f40_cal` — stub
Four-instruction selector stub: stores src 0x0018BFC8 into calibration group pointer 0xFFF84F40, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DEF4 `tbl_sel_fff84f44_app` — stub
Four-instruction selector stub: stores src 0x00062A14 into calibration group pointer 0xFFF84F44, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.
#### 0x0011DEFC `tbl_sel_fff84f44_cal` — stub
Four-instruction selector stub: stores src 0x0018C004 into calibration group pointer 0xFFF84F44, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DF10 `tbl_sel_fff84f48_app` — stub
Four-instruction selector stub: stores src 0x00062A18 into calibration group pointer 0xFFF84F48, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DF18 `tbl_sel_fff84f48_cal` — stub
Four-instruction selector stub: stores src 0x0018C008 into calibration group pointer 0xFFF84F48, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DF2C `tbl_sel_fff84f4c_app` — stub
Four-instruction selector stub: stores src 0x00062A1C into calibration group pointer 0xFFF84F4C, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DF34 `tbl_sel_fff84f4c_cal` — stub
Four-instruction selector stub: stores src 0x0018C00C into calibration group pointer 0xFFF84F4C, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DF48 `tbl_sel_fff84f50_app` — stub
Four-instruction selector stub: stores src 0x00062A20 into calibration group pointer 0xFFF84F50, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DF50 `tbl_sel_fff84f50_cal` — stub
Four-instruction selector stub: stores src 0x0018C010 into calibration group pointer 0xFFF84F50, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DF64 `tbl_sel_fff84f54_app` — stub
Four-instruction selector stub: stores src 0x00062A28 into calibration group pointer 0xFFF84F54, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DF6C `tbl_sel_fff84f54_cal` — stub
Four-instruction selector stub: stores src 0x0018C018 into calibration group pointer 0xFFF84F54, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DF80 `tbl_sel_fff84f58_app` — stub
Four-instruction selector stub: stores src 0x00062A30 into calibration group pointer 0xFFF84F58, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DF88 `tbl_sel_fff84f58_cal` — stub
Four-instruction selector stub: stores src 0x0018C020 into calibration group pointer 0xFFF84F58, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DF9C `tbl_sel_fff84f5c_app` — stub
Four-instruction selector stub: stores src 0x00062A6C into calibration group pointer 0xFFF84F5C, selecting the application copy of the calibration object index (12,198 entries at 0x56C90). This group is the lockup-strategy request-level group (spec 09 §3.1, index 12151-12194). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DFA4 `tbl_sel_fff84f5c_cal` — stub
Four-instruction selector stub: stores src 0x0018C05C into calibration group pointer 0xFFF84F5C, selecting the calibration copy of the object index (0x180280 block). This group is the lockup-strategy request-level group (spec 09 §3.1, index 12151-12194). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DFB8 `tbl_sel_fff84f60_cal` — stub
Four-instruction selector stub: stores src 0x00062B1C into calibration group pointer 0xFFF84F60, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DFC0 `tbl_sel_fff84f60_cal` — stub
Four-instruction selector stub: stores src 0x0018C10C into calibration group pointer 0xFFF84F60, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DFD4 `tbl_sel_fff84f64_cal` — stub
Four-instruction selector stub: stores src 0x00062B24 into calibration group pointer 0xFFF84F64, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.

#### 0x0011DFDC `tbl_sel_fff84f64_cal` — stub
Four-instruction selector stub: stores src 0x0018C114 into calibration group pointer 0xFFF84F64, selecting the calibration copy of the object index (0x180280 block). Called only by 0xF7950, which runs all selectors of one kind when calibration-source byte 0xFFF8A075 changes (0 = application copy, 1 = calibration copy). Cluster of 1. [O] spec 09 §1.1.


#### 0x0011DFF0 `shift_mode_s16_get` — data-accessor
Five-instruction getter: returns the s16 pointed to by pointer-slot +0x24 of the shared
shift-module object at PTR_DAT_0011e1f8. 0x11E0C6 reads the same slot as a mode value with the
module's 1->2 latch remap, so this reads that mode word. Called by 0x12F91C only. Cluster of 1.
[O] read; [I] role.

#### 0x0011DFFA `shift_gear_code_resolve` — logic
Resolves an index into the object at PTR_DAT_0011e1f8 (slots +0x20/+0xc/+0x24/+0x38) and runs a
3-arg function-pointer lookup (PTR_FUN_0011e20c) with it; input byte from PTR_DAT_0011e1fc
getter is treated <0x19 -> slot 0, 0x19..0x63 -> index-0x19, ==0x21 -> special path via
0x11E1C2, >0x63 -> error report FP(...,0x7e,0). Result goes to *(short*)(param_1+2), optionally
ANDed with a mask from PTR_DAT_0011e204+8 (|2 when byte +0xD set). Called by 0xF83F2; calls
0x11E1C2, 0x12155E, 0xF7EC8, 0xF82D4, 0xF82EC. Cluster of 1. [I] byte looks gear-coded
(0x19..=gear band, 0x21 special).

#### 0x0011E0C2 `shift_result_s16_get` — stub
Two-instruction accessor returning *(short*)(param_1+2) — the result slot written by 0x11DFFA.
Called by 0xF7864 (the interface copier of spec 09 §3.1). Cluster of 2 (member 0x13C160 same
pattern on another object; identical body). [O].

#### 0x0011E0C6 `shift_base_select_gated` — logic
Selects the active "base" byte param_1[1] of a request object: default from PTR_DAT_0011e21c
(+0x18, or +0x1B when slot +0x20 char == 1); forces request path (param_1[1]=*param_1, latch
param_1[4]=1) only when enable chain passes (flags +0x3e/+0x9a, indirect predicate
PTR_FUN_0011e224, request code 0x0F special-cased to PTR_DAT_0011e230); overridden by limit
checks (speed map +0x4e vs slot +0x10, state byte +0x72 != 2, torque counter 0xFF-[+0x70] vs
slot +0x18) which revert to slot +0x14. Persists latch to PTR_DAT_0011e228. Called by 0xF83F2
after 0x11DFFA. Cluster of 1. [I].

#### 0x0011E1C2 `shift_spec_code_map` — logic
Pure code conversion for the special path of 0x11DFFA: 9->0, 8->1, 15->4, 48->8, 44->10,
anything else 0xFF. Called only by 0x11DFFA. Cluster of 1. [O] mapping; [I] meaning of codes.

#### 0x0011E23C `shift_speed_filter_scale` — logic
Per-cycle step: copies s16 from input object +6, clamps to a floor byte ([0x84] of
PTR_DAT_0011e2c8), feeds a ramp/PT1 function pointer (PTR_FUN_0011e2cc) on a sub-object at
param_1+1, then a vtable +0x14 curve; outputs abs(x)*10/32 scaled byte at +0x1a and word at
+0x1c; clears both when input flag +8 != 0. Called by 0xF9570. Callees 0xFD3E0, 0x121CC6,
0x121CD6. Cluster of 1. [I].

#### 0x0011E2D8 `shift_speed_ring_avg_band` — logic
Ring-buffer filter: writes param_2 into slot [(+0x28)&] of param_1, advances the index over a
length from struct PTR_DAT_0011e42c slot +4 (clamped 1..20), averages all slots into +0x2e,
classifies the mean against a curve via PTR_FUN_0011e43c into band 0..3 at +0x2b, and sets
plausibility flag +0x2a when mean/new value leave the window from slots +8/+0x14 (row
[*PTR_DAT_0011e430+9]); first-pass init (PTR_FUN_0011e438, 0x78), busy latch to PTR_DAT_0011e428
(1->2 remap). Called by 0xF9570. Cluster of 1. [I] subject looks like a speed signal.

#### 0x0011E440 `shift_window_latch_gate` — logic
Runs the window hysteresis 0x11E4D4(param_1, param_2), then gates a second value (indirect
getter PTR_FUN_0011e520 over PTR_DAT_0011e51c, converted by PTR_FUN_0011e528 using slot +0xc)
through the latch state: param_1[1] = converted value only while latched. Called by 0xF9570.
Cluster of 1. [I].

#### 0x0011E474 `shift_dual_thresh_state3` — logic
If flag slot +0x18 of object PTR_DAT_0011e524 == 1 AND both converted values
(PTR_FUN_0011e534 on slots +0x10 and +0x14, argument s16 from PTR_DAT_0011e530+0x40) reach
thresholds (byte PTR_DAT_0011e52c and PTR_DAT_0011e530[0x3e]), sets param_1[1]=3. Called by
0xF9570; calls 0x1216E4. Cluster of 1. [I] two-threshold engage condition.

#### 0x0011E4D4 `lib_window_hysteresis` — library
Window latch on param_1[0]: set to 1 when lo(*obj[0]) <= x <= hi(*obj[1]); cleared to 0 when
outside by tolerance *obj[2]; returns the previous state. Used by 0x11E440. Object from
PTR_DAT_0011e524. Cluster of 1. [O].

#### 0x0011E538 `shift_flag_164_get` — data-accessor
Returns the byte at **(byte**)(object PTR_DAT_0011e7cc + 0x164). Called by 0xF9C4C and by the
situation step 0x11E56C (reset gating). Cluster of 1. [O] access; [I] meaning.

#### 0x0011E546 `shift_once_capture_val` — logic
One-shot on guard byte DAT_0011e7d0: on first call captures s16 (PTR_DAT_0011e7d4 only when
pointer-slot +0x160 of object PTR_DAT_0011e7cc holds 1, else 0) into param_1+0x88 and sets the
guard. Called by 0xF9CB6 <- 0xC21E4 (init path). Cluster of 1. [O].

#### 0x0011E56C `shift_situation_step` — logic
Core per-cycle step of the shift-situation object (866 instr, called by 0xF9570): rotates value
history (+1/+6/+2/+0xa/+3 slots), clamps a filter chain (PTR_FUN_0011e7f0 on sub-objects
param_1+4 and +9, floors [0x84]), evaluates a chain of ~9 enable/inhibit conditions each
setting a bit in word +0x7e (via PTR_FUN_0011ea34/0011ed00 bit-setter, indices 0..8) and
latching a 1->2 remapped state cell, computes blended target (+0x26 clamped between scale
outputs, +0x9a delta, +0x27 via PTR_FUN_0011ed20), runs sub-steps 0x11F0B4, 0x11F172, 0x11F0EE,
0x11EF14, 0x11EE50-path 0x11E538, torque blend ((x*1000+y*-1000)/d)*1000/0x188B into +0x21,
slip-ish error +0x92, and ramp objects at +0xe/+0x13/+0x18 (PTR_FUN_0011efec). First-cycle
detect via globals DAT_fff8d4a8/DAT_fff8e6fb. Callees include 0x121A56 (bilinear map interp,
spec 09 §1.2), 0x1218FA, 0x121C94, 0x121CAA, 0x121CD6, 0x121D10, 0x11F0B4/EE/172, 0x11EF14,
0x12164A, 0x1216E4, 0x1213F6, 0x121432, 0x12155E, 0x1207E0(0xF9570 path). Cluster of 1. [I]
overall role; mechanics [O].

#### 0x0011EE50 `shift_situation_reset` — logic
Reset for the situation object: zeroes five s16 at +4..+0x14, calls vtable +0x20 (reset) on the
five ramp sub-objects at +0x10/+0x24/+0x38/+0x4c/+0x60, clears outputs +0x7c..+0x92 and +0xa0,
and clears bytes [0x34], [0x2d..0x33] of global struct PTR_DAT_0011f010. Called by 0xF9C4C.
Cluster of 1. [O].

#### 0x0011EF14 `shift_torque_offset_calc` — logic
Builds a torque/speed offset sum for the situation object: from base s16 +0x86 and curve
lookups on pointer-slots +0x154/+0x158/+0x15c of object PTR_DAT_0011efe4 (PTR_FUN_0011eff4),
sums into +0xaa, target +0x90 = base+sum; converts through the vtable +0x14 curve of the
sub-objects at +0x38/+0x4c/+0x60 into +0x88/+0x8a/+0x8e. Branch on flag slot +0x150 chooses
which of +0x88/+0x8e leads. Called by 0x11E56C; calls 0x1218FA, 0x121CD6. Cluster of 1. [I].

#### 0x0011F0B4 `shift_map_lookup_sel` — logic
Two-way table lookup: param_2==1 selects pointer-slot +0xD4 of object PTR_DAT_0011f290, else
+0xC8; row index = byte *PTR_DAT_0011f28c; the map is evaluated by PTR_FUN_0011f294 (0x121A56
bilinear interpolation, spec 09 §1.2) with column count from *param_1+0x58 word. Called by
0x11E56C. Cluster of 1. [O] mechanics.

#### 0x0011F0EE `shift_point_bank_dispatch` — logic
Scales a raw word (PTR_DAT_0011f29c) by curve slot +0xC4 of PTR_DAT_0011f290
(PTR_FUN_0011f2a8), subtracts threshold +0xC0, applies a hysteresis latch (PTR_DAT_0011f2ac:
set below scaled-threshold, clear below raw), then dispatches to table bank 0x11F186 or
0x11F2B8 with the situation byte *PTR_DAT_0011f298 and word param_1+0x7c. Called by 0x11E56C.
Cluster of 1. [I].

#### 0x0011F172 `shift_flags_clear_check` — logic
Returns 1 when the situation flag word param_1+0x7e is zero or param_2 (force flag) is set,
else 0. Called by 0x11E56C before continuing the step. Cluster of 1. [O].

#### 0x0011F186 `shift_point_lut_bank_a` — dispatch
Case body 1..8 over base code param_2: picks a 5-pointer level-table bank from object
PTR_DAT_0011f28c-relative offsets +0x00/+0x18/+0x30/+0x48/+0x60/+0x78/+0x90/+0xA8, row by
current level byte *PTR_DAT_0011f28c, evaluates via PTR_FUN_0011f294 (bilinear 0x121A56) with
column count [3] (=5 levels), then scales via PTR_FUN_0011f2a0; unknown codes report 0x55 and
return 0. Called by 0x11F0EE. Cluster of 1. [I] five-level bank matches driver-type levels
(spec 09 §2.4).

#### 0x0011F2B8 `shift_point_lut_bank_b` — dispatch
Identical dispatch to 0x11F186 but bank offsets +0x0C/+0x24/+0x3C/+0x54/+0x6C/+0x84/+0x9C/
+0xB4 from object PTR_DAT_0011f3b8 — the second bank selected by 0x11F0EE's hysteresis.
Called by 0x11F0EE. Cluster of 1. [I].

#### 0x0011F3CC `shift_map_lookup_4c` — data-accessor
Thin wrapper: evaluates the map at pointer-slot +0x4C of object PTR_DAT_0011f67c with argument
byte *PTR_DAT_0011f678 through PTR_FUN_0011f680, returning the byte. Called by 0xF9454; calls
0x121432. Cluster of 1. [O] mechanics; [I] content.

#### 0x0011F3E6 `shift_blend_init_vals` — logic
Initialises a blend object: +0x4C from a 3-arg lookup (PTR_FUN_0011f688 over
PTR_DAT_0011f684), +0x3C = 2000 constant, +0x3E from a 1-arg lookup, +0x49 = 2. Called by
0xF94CE. Cluster of 1. [O].

#### 0x0011F410 `shift_level_blend_step` — logic
Blending step (260 instr, called by 0xF9CDC): keeps an accumulator (param_1[0xC]) and count
(param_1[0xD]) of a gated value, produces a x10 rolling average +0xF, a ramp weight byte +0x12
via curve slot +0x40, and blends previous +0x3E with the average:
(u1*(100-w) + u6*w)/100 -> +0x13, returning it. Handles first-cycle (DAT_fff8d4ac/
DAT_fff8e4e6), situation code 0x09 transitions (PTR_DAT_0011f698 [0x3d]/[0x3E], 1->2 remap
latch PTR_DAT_0011f69c), filter chains on sub-objects (+0 vtable, +5 PTR_FUN_0011f6b0/0x14),
and ramp/settle paths 0x11F6BC, 0x11F836, 0x11F8BC, 0x11F8D2. Weight blend matches the
driver-type level blending of spec 09 §2.4. Cluster of 1. [I] role; blend arithmetic [O].

#### 0x0011F6BC `shift_cond_mask_build` — logic
Builds a 12-bit condition/inhibit bitmap from the situation inputs: bit1 force param_2, bit2
(flags from PTR_FUN_0011f8e4/e8/f8 getters on PTR_DAT_0011f8dc), bit4 flag ==1, bit8 value
sVar19 outside [slot6, slot1], bit0x10 gear-mask (1<<(gear-1), gear = *PTR_DAT_0011f8fc) not in
allow word *puVar27[6], bit0x20 bVar23 < *slot+0x1c byte, bit0x40 sVar8 < *slot8, bit0x80
sVar20 inside (slot+0xc, slot+0x10) window, bit0x100 getter ==2, bit0x200 sVar18 outside
(slot+0x14 window), bit0x400 scaled uVar17 < word +0x38c. Called by 0x11F410. Cluster of 1.
[O] mechanics; [I] meanings.

#### 0x0011F836 `shift_ratio_calc` — logic
Computes +0x42 = scaled((map at slot +0x34, row gear-1) * +0x4a / 100) via PTR_FUN_0011f914/
0x121CD6, then returns ((ref word PTR_DAT_0011f91c * +0x46)/dt - (+0x38 + +0x42)) * 100 /
(+0x44 + +0x4a), guarding zero divisors (dt, sum). Called by 0x11F410. Cluster of 1. [I] a
normalized ratio/percentage.

#### 0x0011F8BC `shift_thresh_state2` — logic
Returns 2 when the word via pointer-slot +0x2C of PTR_DAT_0011f8f4 <= *(u32*)(param_1+0x34),
else 0; result stored as blend-mode byte +0x49 by caller 0x11F410. Cluster of 1. [O].

#### 0x0011F8D2 `shift_clear_34_30` — stub
Four instructions: zeroes the u32 accumulator slots +0x34 and +0x30 of the blend object.
Called by 0x11F410 on situation-code-9 entry. Cluster of 1. [O].

#### 0x0011F924 `shift_point_track_update` — logic
Track/hold update for shift-point pairs: rotates previous (+4/+0x12/+5/+0x16 out of +1/+6/+2/
+0xa), loads new min/max word+byte pairs from input object *param_1 (+0x22..+0x30), ordered by
flag *PTR_DAT_0011fa9c, keeps byte maxima (+0xd, +3), publishes pairs to global struct
PTR_DAT_0011faa0 (+6..+0xa), averages word pairs (+0x1a, +6), computes four scaled deltas via
0x11FA72 into +7/+0x1e/+8/+0x22, word maxima to +9/+0x26 and publishes +9 to
PTR_DAT_0011faa4. Called by 0xF9570. Cluster of 1. [I] shift-point/agreement tracker.

#### 0x0011FA72 `shift_delta_scale_calc` — logic
Percentage-of-range delta: scale(((value - reference) * 10000 / range) * 100 / 0x240) with
range byte *PTR_DAT_0011faa8, via PTR_FUN_0011faac (0x121CD6). Called 4x by 0x11F924. Cluster
of 1. [O] arithmetic.

#### 0x0011FAB0 `shift_condition_latch_eval` — logic
Evaluates three trigger conditions (range margin bVar2 via map slot 0/param_7 window checks,
derived-value margin bVar3 through 0x11FCA8 vs scaled threshold, reserve margin bVar4 from
map slot +8 and *0x10-scaling), latches active flag +0x14 and reason bitmap +0x18
(1=range, 2=derived, 4=reserve), forced off when param_2/param_11 set; keeps a timer +0x16
via PTR_FUN_0011fd20 when inactive. Called by 0xF9570. Cluster of 1. [I].

#### 0x0011FCA8 `shift_torque_ratio_calc` — logic
Floating-point ratio for 0x11FAB0: (base s16 (PTR_DAT_0011fd24+0x7c) * byte slot+0xc) /
(max(speed FP, floor) * K1/K2) * K3/K4 with constants DAT_0011fd2c..0x0011fd3c, scaled by
PTR_FUN_0011fd40. Cluster of 1. [I] torque-vs-speed normalisation.

#### 0x0011FD44 `shift_state7_condition` — logic
Filter/condition step for 0x11FAB0's object: clamps word slot+0x1C to floor [0x84], ramps
sub-object param_1 (PTR_FUN_0011fe40), converts through vtable +0x14 -> +0x1e, then sets
param_1[7]=1 when (speed uVar7/uVar8 >= threshold slot+0x14 AND cVar10==0) OR (sVar2 <
converted AND cVar11==0) AND uVar5 (slot+0x4e word) > 200 AND byte [0x52]==0; cleared whenever
slot +0x72 != 2. Cluster of 1. [O] mechanics; [I] meaning.

#### 0x0011FE44 `shift_torque_model_step` — logic
Orchestrator of the situation torque model (113 instr, called by 0xF9570): reads elapsed time
via 0x11FF48 (abort with report 0x5E when zero), then runs 0x11FF6A (six ramp filters), 0x120136
(accel/delta), 0x1200A0 (rate), 0x12022A, 0x12025A -> +0xFA, 0x12028A -> +0x106, 0x120244 ->
+0xDE, 0x1202F6 -> +0xE0, 0x120672 -> +0xF4, 0x1204F8 -> inhibit byte +0xEF (gating +0xFE),
0x1206D4 -> +0xFC, 0x1205E4 -> +0xDC, 0x120378 -> +0xE4 (invalid flag +0xF5 when -1), then
0x120756, 0x120714 and 0x120332 -> +0xF6. Cluster of 1. [I] role from structure.

#### 0x0011FF48 `shift_time_since_last` — logic
Elapsed-time getter for 0x11FE44: calls PTR_FUN_00120090(now - *param_1) on counter
PTR_DAT_0012008c and stores now into *param_1. Cluster of 1. [O].

#### 0x0011FF6A `shift_ramp_filters_step` — logic
For six source pointer-slots (+4, +0x10, +0x3C, +8, +0x14, +0xC) of object
PTR_DAT_00120094: clamps each word to floor [0x84] of PTR_DAT_00120098 (reporting 0x85..0x8A
when clamped) and steps the ramp/PT1 sub-objects at param_1+0x58/+0x6C/+0xBC/+0x80/+0x94/
+0xA8 via PTR_FUN_0012009c. Called by 0x11FE44. Cluster of 1. [O].

#### 0x001200A0 `shift_rate_calc` — logic
When param_4 == 0: scales param_3*100/0x20, converts through vtable +0x14 curves of
sub-objects +0x80 (+0xD8) and +0x94 (+0xF0), computes change rate ((new-old)*1000)/dt and
converts through +0xA8's curve into +0xDA. dt = param_2 (guarded to 1). Called by 0x11FE44.
Cluster of 1. [O] mechanics.

#### 0x00120136 `shift_accel_delta_calc` — logic
Stores scaled param_3 into +0xD0 (negated for mode param_4==2, alternative scale
PTR_FUN_0012035c for param_5==2), computes ramped delta (((new-old)*10000/dt)*100/0x240) into
+0xD4, clamps it to +/- the reference from slot +0x1C (latching reason byte
PTR_DAT_00120360), filters through the +0x58/+0x6C curves into +0xD6/+0xF2 and stores +0xD2.
Called by 0x11FE44. Cluster of 1. [I] acceleration-like delta.

#### 0x0012022A `shift_diff_f0_f2` — logic
Ten instructions: +0xE2 = scale(+0xF0 - +0xF2) via 0x121CD6. Called by 0x11FE44. Cluster of 1.
[O].

#### 0x00120244 `shift_curve_lookup_18` — logic
Evaluates the curve at pointer-slot +0x18 of PTR_DAT_00120354 with argument word +0x106
(PTR_FUN_00120364 -> 0x12164A); result stored to +0xDE by caller. Cluster of 1. [O].

#### 0x0012025A `shift_scaled_curve_bc` — logic
value = FP(PTR_DAT_00120368) (0x121C9E) -> scale (0x121C4A) -> vtable +0x14 curve of
sub-object +0xBC -> scale (PTR_FUN_00120370); result +0xFA. Called by 0x11FE44. Cluster of 1.
[O].

#### 0x0012028A `shift_sq_ratio_calc` — logic
When threshold slot+0x54 < +0xD0: ratio = ((+0xFA^2 * 10/0x400) * word slot+0x34) /
((+0xD0^2 * 100)/0x8199), scaled into +0xF8; returns +0xF8 + +0xE2 (zero when below
threshold). Squared-value quotient suggests inertia/power relation. Called by 0x11FE44.
Cluster of 1. [O] arithmetic; [I] meaning.

#### 0x001202F6 `shift_sum_clamp_20k` — logic
+0xDE + curve(slot +0x1C over +0xD6) (0x12164A), scaled and clamped to +/-20000; returned and
stored to +0xE0 by 0x11FE44. Cluster of 1. [O].

#### 0x00120332 `shift_thresh_latch_out` — logic
Hysteresis on +0xEC vs stored reference PTR_DAT_00120374: returns latch byte (1 above, 0
below, previous otherwise) and updates the reference; stored to +0xF6. Called by 0x11FE44.
Cluster of 1. [O].

#### 0x00120378 `shift_demand_max_ramps` — logic
Priority-max demand builder (170 instr, called by 0x11FE44 -> +0xE4): starts from map
PTR_FUN_001205b0(slot+0x28, gear-1, flag +0xF6), raises to words via slots +0x40/+0x44/+0x48
when enable bytes [0x89]/[0x8A]/[0x8B] set (conditions: +0xFA above slot+0x5C latches byte
slot+0x60; flags [0x66]/[0x7B] latch [0x68]; getters cVar13/14/15), optional ceiling slot+0x4C
when [0x8C], floor [0x84] with report 0x84; inactive path (+0xEF set) uses slot+0x50 word;
finally ramps three sub-objects at param_1+4/+0x20/+0x3C toward the result via
PTR_FUN_001205CC. Cluster of 1. [I] demand/level selector feeding ramps.

#### 0x001204F8 `shift_inhibit_bits_eval` — logic
Builds the inhibit bitmap for 0x11FE44 (+0xEF): bit1 when param_2 set and enable word
slot+0x38 & 1 (latches byte PTR_DAT_001205d4 from slot+0x58), bit2 when +0xF4 clear or counter
*PTR_DAT_001205d0 <= byte slot+0x2C (enable & 2), bit4 when param_3 set (& 4, latch
PTR_DAT_001205d8), bit0x80 when param_4 == 2 and byte *PTR_DAT_001205DC > 6 (& 0x80, latch
PTR_DAT_001205E0); enable bytes [0x86]/[0x87]/[0x88] OR the bits in. Cluster of 1. [O]
mechanics; [I] inhibit semantics.

#### 0x001205E4 `shift_offset_curves_sum` — logic
Three curve lookups (slots +0x74/+0x78/+0x7C of PTR_DAT_001207c0 over +0xFE, scalars
[0x43]/[0x53] of PTR_DAT_001207c4) -> +0x10A/+0x10C/+0x10E; sum with +0xFC scaled and clamped
to +/-20000 (0x1218FA); result -> +0xDC. Called by 0x11FE44. Cluster of 1. [O].

#### 0x00120672 `shift_engage_cond_latch` — logic
Latch PTR_DAT_001207d4 (stored to +0xF4): set to 1 when scaled +0xD8 (0x121C4A) exceeds word
slot+0x20 AND slot+0x4E word > 200 AND byte [0x52] == 0; cleared to 0 when slot+0x72 != 2.
Same enable/1-2 idiom as 0x11FD44. Called by 0x11FE44. Cluster of 1. [O].

#### 0x001206D4 `shift_curve_sum_clamp` — logic
Curve lookup (slot +0x30 over +0xFE, arg word PTR_DAT_001207d8) -> +0x108, added to +0xFE and
clamped to +/-20000 (0x1218FA); result -> +0xFC. Called by 0x11FE44. Cluster of 1. [O].

#### 0x00120714 `shift_scale_pair_calc` — logic
Publishes two scaled pairs from ramp outputs: +0xEC/+0xEA = scale(byte slot+0x24 * +0x104/10)
and +0xE8/+0xE6 = scale(byte * +0x100/10), via 0x1218FA. Called at the end of the torque-model
step 0x11FE44. Cluster of 1. [O].

#### 0x00120756 `shift_ramp_outputs_read` — logic
When +0xE4 != -1 (demand valid): converts the three ramped sub-objects at +4/+0x20/+0x3C
through their vtable +0x14 curves (over +0xDC/+0xFE/+0xFC) into scaled +0x100/+0x102/+0x104
(0x1218FA). Called by 0x11FE44. Cluster of 1. [O].

#### 0x001207E0 `shift_word_32_publish` — logic
Copies word +0x32 of object *param_1 (zeroed when its state byte +0x34 == 2) into param_1 word
+1 and global PTR_DAT_001207f8. Called by 0xF9570. Cluster of 1. [O].

#### 0x00120814 `shift_thunk_128_a` — stub
Nine-instruction thunk: tail-calls the handler at DAT_00120a28 (0x1277C2) with the embedded
context object at param_1+0x128. Called by 0xF85DC. Cluster of 1. [O].

#### 0x00120832 `shift_sub_128_ptr` — stub
Returns param_1+0x128 — pointer to the embedded shared context sub-object used by all
situation handlers. Called by 0xF84EE. Cluster of 1. [O].

#### 0x0012083A `shift_thunk_128_b` — stub
Thunk: calls DAT_00120a2c (0x1277B0) with param_1+0x128. Called by 0xF8316. Cluster of 1. [O].

#### 0x00120858 `shift_sub_reset_two` — stub
Two indirect calls resetting sub-objects: PTR_FUN_00120a30(param_1+0x8C) and the thunk to
0x130FE0(param_1+0x128). Called by 0xF8340. Cluster of 1. [O] mechanics; [I] reset.

#### 0x001208B2 `shift_thunk_sub_c` — stub
Thunk: calls DAT_00120a48 (0x12CC86) with the sub-object at param_1+0xC. Called by 0xF860C.
Cluster of 1. [O].

#### 0x001208C0 `shift_situation_dispatch` — dispatch
The situation/base-code dispatcher (495 instr, called by 0x123990): stores code to +0x1D4 and
switches param_2 (codes 1,6,8,9,10,11,12,13,15,16,18,20,21,22,24,27,28,30,31,32,39,44,60,62)
to one of 24 handler function pointers, each receiving its sub-object (offsets +0,+1,+3,+8,+9,
+0xC,+0x58,+0x62,+0x68,+0x5E,+0x8C,+0x10C..+0x122), the shared context +0x128, and the common
argument bundle; unknown codes return 0x37. Handler targets 0x12AA94..0x12D6E8 (other unit).
This instantiates the "situation handlers" of spec 09 §2.4. Cluster of 1. [O] structure;
[I] situation meanings.

#### 0x00120D34 `shift_situation_eval_dispatch` — dispatch
Companion dispatch of 0x1208C0 (369 instr, called by 0x123AA2): same 24 situation codes and
sub-object offsets, but each handler is called in a lighter evaluate form (fewer args); code
0x0F gated by an enable getter (PTR_FUN_00120fdc) returning 0; unknown codes 0x37. Handler
targets 0x129D1A..0x12D7A6. Cluster of 1. [O] structure.

#### 0x0012106A `shift_status_bits_aggregate` — logic
Aggregates status from ~15 situation sub-objects (offsets +0,+2,+0x58,+0x5E,+0x67,+0x8C,
+0x10C,+0x110,+0x11F,+0x120,+0x121,+0x124 and global PTR_DAT_001212a4): each handler reports
into locals, ORed into a rising mask (bits 1,2,0x40,0x10,0x800,0x200,0x20,0x2000,0x1000,
0x10000,0x80,0x100) and a falling mask (2,0x40,0x1000,0x10000,0x100,0x200000,0x400000), then
PTR_FUN_001212d0(param_1+0x128, rising, falling). Called by 0xF8356. Cluster of 1. [I]
edge/change notification.

#### 0x0012120C `shift_wrap_slot4_call` — stub
Wrapper: PTR_FUN_001212d8 (0x1214B4) on pointer-slot +4 of PTR_DAT_001212d4 with (param_2,
param_3). Called by 0xF8506. Cluster of 1. [O].

#### 0x00121224 `shift_wrap_slot8_call` — stub
Same as 0x12120C but pointer-slot +8 of PTR_DAT_001212d4. Called by 0xF85C0. Cluster of 1. [O].

#### 0x0012123C `shift_subs_teardown_seq` — logic
Sequence of six indirect calls over sub-objects (+0xC with +0x128, +0x122, +3 with +0x128,
+0x8C, +0x58) preceded by a no-arg FP — teardown/finalise pass. Called by 0xF8356 (after
0x12106A). Cluster of 1. [I].

#### 0x001212F4 `shift_subs_setup_seq` — logic
Four indirect setup calls: (+0xC, param_2), (+0x122), (+0x128), (+0x54, byte +0x1EF). Called by
0xF8490. Cluster of 1. [I] setup pass.

#### 0x00121326 `shift_thunk_128_c` — stub
Thunk to 0x12EC10 with param_1+0x128. Called by 0xF8356. Cluster of 1. [O].

#### 0x00121330 `shift_thunk_128_d` — stub
Thunk to 0x12EC2E with param_1+0x128. Called by 0x123990 (the dispatch owner). Cluster of 1.
[O].

#### 0x0012134C `shift_get_u16_128_a` — stub
Returns u16 via PTR_FUN_001213d0 (0x128BD4) on sub-object +0x128. Called by 0xF8618. Cluster
of 2: member 0x0012135E identical shape via 0x128BE0 (caller 0xF8624) — sibling getter.
[O].

#### 0x00121370 `shift_get_u8_128` — stub
Returns byte via DAT_001213d8 (0x128BF8) on +0x128. Called by 0xF8630. Cluster of 1. [O].

#### 0x00121382 `shift_get_u8_sub3` — stub
Returns byte via DAT_001213dc (0x12B4E8) on sub-object +3. Called by 0xF8642. Cluster of 1.
[O].

#### 0x001213E8 `libbitset_or_u8` — library
Sets bit (1<<(n&0x1F)) in the target byte (bit 0 clears nothing). 7 instructions; callers
0x1250F4, 0x131012, 0x1229CC. Cluster of 1. [O].

#### 0x001213F6 `libbitset_or_u16` — library
Word variant of 0x1213E8. Callers 0xFBBB8, 0xFB9E0, 0xFBA2C, 0x11E56C (the +0x7E flag word
setter of the situation step). Cluster of 1. [O].

#### 0x00121404 `lib_bit_test` — library
Returns (mask(1<<(n&0x1F)) & value) != 0. Called by 0x1229CC. Cluster of 1. [O].

#### 0x00121414 `lib_gear_idx_map6` — library
Gear-index remap between two encodings: flag==0 and idx 8 -> 7; flag!=0: idx 1 -> 8, else
idx+6. Callers 0x12F67A, 0x131C54, 0x131D70, 0x12FEB8. [I] gear-numbering conversion.
Cluster of 1.

#### 0x00121432 `tbl_byte_at_get` — data-accessor
Bounds-checked byte read from a count-prefixed byte table (count at offset 0, data at +4):
index >= count -> error report 0x4D and 0. Heavily used (62 callers across the shift module,
incl. 0xF9570, 0x121ED0, 0x124570, 0x1229CC). Cluster of 1. [O].

#### 0x0012145A `tbl_word_at_get` — library
Bounds-checked s16 read from a count-prefixed word table (count at +0, data at +4, stride 2):
index >= count -> report 0x4D and 0. Callers 0xFA9DC, 0x100C58, 0x1314B8. Cluster of 2:
member 0x00121482 is the same accessor used by 26 shift-module callers (incl. 0x11E2D8,
0x11F836, 0x124DDE). [O].

#### 0x001214B4 `tbl2d_byte_get` — library
Bounds-checked 2-D byte-table read t[b*n1 + a] from a table headed by u32 n1, n2 (data at +8);
out-of-range -> report 0x4D, 0. Matches the byte-table format of spec 09 §1.2. 39 callers
across the shift module. Cluster of 1. [O].

#### 0x00121508 `tbl2d_word_get` — library
Bounds-checked 2-D s16 read t[b*n1 + a] (header n1,n2, data at +8, stride 2); report 0x4D and
0 out of range. Word-table format of spec 09 §1.2. Callers incl. 0x13068C, 0x13166 0,
0x12AE5C. Cluster of 1. [O].

#### 0x0012155E `tbl2d_word_get_b` — library
Duplicate of the 0x121508 accessor (same layout and 0x4D report) wired to a second report-data
pointer; 18 callers (incl. 0x11DFFA, 0x120378, 0x11E56C, 0x124DDE). Cluster of 1. [O].

#### 0x001215B6 `curve_u8_interp` — library
Linear interpolation of a u8 curve (u32 n, x[n] bytes at +4, y[n] bytes at +4+n): binary-scan
search, clamped at both ends. Callers 0xFF330, 0x13068C, 0x1309C2, 0x1251BC. Cluster of 1. [O]
(spec 09 §1.2 curve format).

#### 0x0012164A `curve_s16_interp` — library
Linear interpolation of an s16 curve (u32 n, s16 x[n] at +4, s16 y[n]); clamped at the ends.
14 callers incl. 0x121ED0, 0x11F0EE, 0x1028FA. Cluster of 1. [O] spec 09 §1.2.

#### 0x001216E4 `curve_u16_interp` — library
Linear interpolation of a u16 curve; clamped ends. 37 callers (situation step 0x11E56C, many
0xFFxxx situation predicates). Cluster of 1. [O].

#### 0x00121784 `map_u8_bilinear_interp` — library
Bilinear interpolation of a byte map (u32 nx, u32 ny, u8 x[nx], u8 y[ny], u8 z[ny][nx]),
clamped at the edges. Callers 0xFF970, 0x12C15E, 0x124138, 0x12B90C, 0x12C230, 0xF869C.
Cluster of 1. [O].

#### 0x001218FA `map_s16_bilinear_interp` — library
Bilinear interpolation of an s16 map (rows = y), clamped. 46 callers across the shift module
(incl. 0x11EF14, 0x1205E4, 0x121DA8). Cluster of 1. [O] spec 09 §1.2.

#### 0x00121A56 `map_u16_bilinear_interp` — library
Bilinear interpolation of a u16 map, clamped — the interpolator cited as `0x121A56` in
spec 09 §1.2; 41 callers (incl. 0x11F0B4, 0x11F186, 0x11F2B8, 0x122570, 0x12501E).
Cluster of 1. [O] spec 09 §1.2.

#### 0x00121BF0 `lib_pt1_ramp_step` — library
Ramp/integrator step toward target param_1 (x1000 scaled): dt = min(now - last, param_3),
step = scale((param_1*1000 - state)/dt) via 0x121CD6, state += step*dt, last = now (counter
PTR_DAT_00121c68). Callers 0x1017AC, 0xFCFC2, 0xF869C. Cluster of 1. [O] mechanics; [I] PT1.

#### 0x00121C4A `lib_abs_s16` — library
Saturating absolute value of s16 (-0x8000 -> 0x7FFF). Callers 0x12025A, 0x120136, 0x120672,
0xFCFC2. Cluster of 1. [O].

#### 0x00121C6C `lib_abs_s32` — library
Absolute value of s32 (-0x80000000 -> DAT_00121d98). Callers 0x11FAB0, 0x12ABB8. Cluster of 1.
[O].

#### 0x00121C94 `lib_clamp_u32` — library
Clamps a u32 to DAT_00121d98 (max). Callers 0x11FF48, 0x11E56C. Cluster of 1. [O].

#### 0x00121D50 `lib_f2u16_sat` — library
Saturating float -> u16 conversion (0.0..threshold, 0xFFFFFFFF above/outside). Callers 0xFF330,
0x13068C, 0x1309C2, 0x1251BC. Cluster of 1. [O].

#### 0x00121D70 `lib_f2s16_sat` — library
Saturating float -> s16 conversion with a validity window (DAT_00121da0/0x00121da4), else
-0x8000/0x7FFF. Caller 0x11FCA8. Cluster of 1. [O].

#### 0x00121DA8 `shift_two_curve_max_check` — logic
Evaluates two curves (slots +8 and +0xC of PTR_DAT_00121eac, over byte *PTR_DAT_00121eb0 and
s16 +0x3AE/+0xE0), scales both (0x121C9E/0x121CFA), writes the max at param_1+2, and raises
flag *param_1 = 1 when the max exceeds word slot 0 (or falls inside slot0-slot1 band => 0).
Called by 0xF90E8. Cluster of 1. [I] threshold monitor.

#### 0x00121E4C `shift_curve_thresh_check` — logic
Sibling of 0x121DA8: one curve (slot +0x18) scaled into param_1+6, flag +4 = 1 below word
slot +0x10, 0 at/above slot+0x10+slot+0x14. Called by 0xF90E8. Cluster of 1. [O] mechanics.

#### 0x00121ED0 `shift_dtlevel_calc` — logic
Driver-type level computation of spec 09 §2.4 [O]: for situation code param_2 (1, 0x10, 0x3E,
0x15, 0x1C, 0x1E, 0x1F, 0x1B, 0x0B, 0x2C) selects a calibration object from
PTR_DAT_0012212c slots (+0..+0x44; flag [0x22] picks variants), evaluates it over the
sportiness value *PTR_DAT_00122124 (curve 0x12164A, map 0x12138-form, or level-table +
0x122164 for codes 1/0x10/0x3E), scale cap 400/200/100 (0-4.00/2.00/1.00); > cap -> report
0x98 and -200, unknown code -> report 0x97 and -400; negative result sets error outputs,
otherwise 0x122066 splits the level. Callers 0xF8B9A, 0xF8B74. [O] structure, spec 09 §2.4;
[I] situation-code meanings.

#### 0x00122066 `shift_dtlevel_split` — logic
Splits the level (0..400) as spec 09 §2.4 [O]: lower = v/100, upper = (v+99)/100, weight =
v%100 (via PTR_FUN_00122148); integer level picked with hysteresis band 50 +/-
offset-byte from slot +0x48 (bVar5/bVar6); mode -2 updates only the previous-level byte +3,
normal mode stores v, level, lower, upper, weight (param_1[0]/+1/+2/+5/+3). Called by
0x121ED0. Cluster of 1. [O] spec 09 §2.4.

#### 0x00122164 `shift_dtlevel_curve_lookup` — logic
Selects one of the five driver-type level curves (pointer-slots +0x10/+0x14/+0x18/+0x1C/+0x20
of PTR_DAT_001221d8 by index 1..5) and interpolates the 2-D map (0x1218FA) over (param_3,
*PTR_DAT_001221d4); bad index -> report 0x99, -300. Called by 0x121ED0. The five curves match
the 0-4 levels of spec 09 §2.4. Cluster of 1. [O] structure.

#### 0x001221E8 `shift_map_thresh_flag` — logic
Map (slot +2 of PTR_DAT_00122260, bilinear via 0x121A56-class FP with [0x42]/[0x3E]) scaled by
curve slot +0xC over *PTR_DAT_0012226c (x/100) into param_1+2; flag *param_1 = 1 above word
slot 1, 0 below (slot1 - slot0) window (FP 0x121CAA). Called by 0xF90E8. Cluster of 1. [O]
mechanics.

#### 0x0012227C `shift_src_select_ramp` — logic
Selects one of two source words (slots +0x10/+0x14 of PTR_DAT_00122488) by comparing byte
param_1+0x15 against *PTR_DAT_0012248c (state latched to PTR_DAT_00122490 / DAT_fff8e6fc),
ramps param_1 to it (floor [0x84], reset via vtable +0x20 on source change), converts and
clamps through vtable +0x14, runs the state latch 0x122358, and combines flags (+5) from
*PTR_DAT_0012249c, +0x16, plus the 1->2 latch of slot +0xC. Called by 0xF9088. Cluster of 1.
[I].

#### 0x00122358 `shift_state16_latch_eval` — logic
Evaluates the param_1+0x16 state toggle from scaled speed (PTR_FUN_001224a8), map slot+0x1C
and slot+0xB values, curve slot+0x20 vs converted sVar16, byte *PTR_DAT_001224c4, and
speed/limit words (slots 9/0/1, PTR_DAT_001224ac, +0x15): set-1 needs map1 > byte, no
*PTR_DAT_001224a0 inhibit, curve > converted, uVar3 > speed, and the speed/gear sub-conditions;
set-0 is the mirrored chain with *PTR_DAT_001224cc guard. Called by 0x12227C. Cluster of 1.
[O] mechanics; [I] meaning.

#### 0x001224D0 `shift_revlimit_enabled` — logic
Enable check: FP(PTR_DAT_0012255c) != 0 AND *PTR_DAT_00122564 == 0 AND 0x122536() (gear
allowed). Part of the rev-limit forced-upshift group 0xFFF84F08 users (spec 09 §2.5).
Callers 0xF8F0A, 0x122506. Cluster of 1. [I] role from spec 09 §2.5 user list.

#### 0x00122506 `shift_revlimit_enabled_lvl` — logic
0x1224D0() AND param_2 > FP(PTR_DAT_0012255c) value — rev-limit enable with a level threshold.
Called by 0xF8F1C. Cluster of 1. [I].

#### 0x00122536 `shift_revlimit_gear_allowed` — logic
Bit test: word via PTR_DAT_00122568 has bit 1<<(*PTR_DAT_0012256c) set (and != 0xFFFF) —
per-gear allow mask. Listed in spec 09 §1.1 as a user of rev-limit group 0xFFF84F08 (§2.5).
Called by 0x1224D0. Cluster of 1. [O] mechanics; [I] gear-allow role.

#### 0x00122570 `shift_revlimit_upshift_calc` — logic
Rev-limit forced-upshift engine (405 instr, called by 0xF9038; spec 09 §2.5): evaluates four
map pairs (slots +0x20/+0x10/+0x18/+0x28 of PTR_DAT_001227ac, transitions 0/1, args
[0x3E]/[0x12]/[0x13]/+0x14) into +8/+0xE/+0x14/+0x1A and +0xA/+0x10/+0x16/+0x1C, with
10000 "never" sentinels when disabled (flags [0x3f], [0x52], word +0x4E < 201); +2 = min of
the up limits (cap 10000), +4 = max of the down limits. Then window check against scaled
speed uVar20/uVar21 (margins slots +0/+4/+8/+0xC via PTR_FUN_001227b0): crossing a limit
increments per-transition counters +6/+0xC/+0x12/+0x18 and total +0x21 (notification FPs
0x12298x on struct PTR_DAT_00122988), sets active flag *param_1 and flags +0x1E/+0x1F/+0x20;
counter check/reset path via 0x12295A/0x122946; also calls 0xC23D4-0xC2448 (event/report
helpers, other unit). Cluster of 1. [O] mechanics; [I] exact transition semantics — spec 09
§2.5 says these correspond to ~5,600 engine-rpm upshift points.

#### 0x00122922 `shift_revlimit_state_capture` — logic
One-shot on DAT_001229a0: copies bytes [2],[6],[4],[0] of global struct PTR_DAT_00122988 into
param_1 fields +6/+0x12/+0xC/+0x21 (the counter slots of 0x122570). Called by 0xF8DEC (init
path). Cluster of 1. [O].

#### 0x00122946 `shift_revlimit_counters_clear` — logic
Clears the five rev-limit counters +6/+0xC/+0x12/+0x18/+0x21 of the 0x122570 object. Called by
0x122570 on its reset path. Cluster of 1. [O].

## Unit summary

220 clusters, 223 named functions (3 multi-member clusters: 0x11E0C2+0x13C160, 0x12134C+
0x12135E, 0x12145A+0x121482).

Kinds: stub 133 (117 calibration-selector stubs + thunks/clears), logic 60, library 22,
dispatch 4 (0x11F186, 0x11F2B8, 0x1208C0, 0x120D34), data-accessor 4. Unknown-kind entries: 0
— but see the inference list below.

What this unit is:

1. **Calibration-source selector bank (0x11D984–0x11DFDC, 117 stubs, spec 09 §1.1 [O]).**
   Pairs writing each RAM group pointer 0xFFF84E7C–0xFFF84F64 with either the application
   copy (0x56C90+4k) or the calibration copy (0x180280+4k) of the 12,198-entry object index.
   All called from 0xF7950 when selector byte 0xFFF8A075 changes. Known groups in range:
   0xFFF84EB0 shift maps, 0xFFF84EBC tip limits, 0xFFF84F08 rev-limit, 0xFFF84F5C lockup
   strategy. 0x0011D984 is a lone calibration-only setter (its struct-pointer analogue pair
   is 0x107250/0x107258, called from 0xDA418 — not this unit).

2. **Shift-situation / torque-model engine (0x11DFF0–0x1206D4).** Per-cycle steps called from
   the 0xF9570 orchestrator: 0x11E56C (866-instr situation step), 0x11FE44 (torque-model
   orchestrator) and their filter/ramp/threshold helpers.

3. **Situation-handler dispatch layer (0x1208C0/0x120D34 + thunks 0x120814–0x121382).**
   24 situation codes dispatch to handler function pointers over sub-objects of a common
   context at +0x128; the actual handlers (0x1277B0–0x130FE0) are in another unit.

4. **Calibration interpolation/timeout library (0x12145A–0x121D70).** The spec 09 §1.2
   accessors: curve u8/s16/u16, byte/s16 2-D tables, bilinear u8/s16/u16 maps (0x121A56 is the
   one cited in the spec), plus ramp/abs/clamp/float-convert helpers. 0x121432/0x12145A are the
   byte/word table readers with the 0x4D bounds report.

5. **Driver-type level chain (0x121ED0 → 0x122066, plus 0x122164) — spec 09 §2.4 [O].**
   Situation-code-keyed curve/map lookup over the sportiness value → level 0–400 → split into
   lower/upper/weight with 50±offset hysteresis.

6. **Rev-limit forced-upshift (0x1224D0/0x122506/0x122536/0x122570/0x122922/0x122946) —
   spec 09 §2.5.** Enable checks, per-gear allow bit, four map-pair limit computation with
   10000=never sentinels, window-cross counters.

Weakest inferences (no `unknown` kind used; these are named for mechanics only, meaning is
[I]): 0x11DFFA/0x11E0C6 (gear-coded resolve/base gate), 0x11E23C/0x11E2D8 (speed filter/ring
average — subject assumed), 0x11F6BC (condition bitmap meanings), 0x11F924 (point tracker),
0x120378 (demand max), 0x1204F8 (inhibit bits), 0x12106A (status aggregate), 0x122066-adjacent
situation codes, 0x12227C/0x122358 (source select + latch).

Cross-region leads:
- 0x1208C0/0x120D34 dispatch into handlers 0x1277B0–0x130FE0 (other r10_app unit) — those
  units should cross-check the 24 situation codes and the +0x128 context layout.
- 0x122570 calls report/event helpers 0xC23D4–0xC2448 (0xC0000-area unit).
- Callers of this unit's engine live at 0xC21E4/0xC22DA/0xF8xxx/0xF9xxx (r0c_app unit);
  0xF9570 is the situation orchestrator that owns 0x11E56C/0x11FE44/0x122570.
- 0x107250/0x107258 (same selector family, struct pointers for group 0xFFF84D0C, caller
  0xDA418) belong to whichever unit holds 0x107xxx.
