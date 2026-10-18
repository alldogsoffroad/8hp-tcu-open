# Ghidra headless helper scripts

Generic scripts used with `analyzeHeadless` for the TCU analysis (no firmware data in here).
The Ghidra project itself is derived from the dumps and lives outside the repository
(`~/opt/ghidra_projects/tcu`).

```
G=~/opt/ghidra_12.1.4_PUBLIC/support/analyzeHeadless
P=~/opt/ghidra_projects
S="$PWD/tools/ghidra_scripts"
# one-time import (the ELF wrapper declares ARM; force SH-2A big-endian)
$G $P tcu -import 1034420267_sh7254r.elf -processor SuperH:BE:32:SH-2A \
   -scriptPath "$S" -postScript ApplySeeds.java 1034420267_sh2a.seeds.tsv
# decompile functions containing the given addresses
$G $P tcu -process 1034420267_sh7254r.elf -noanalysis -readOnly -scriptPath "$S" \
   -postScript DecompIn.java out.c 0x1234 0x5678
```

| Script | Arguments | Purpose |
|---|---|---|
| `ApplySeeds.java` | seeds.tsv | create functions / labels from `addr<TAB>function|label[<TAB>name]` |
| `MakeFns.java` | addr… | disassemble and create functions (needs a writable run, no `-readOnly`) |
| `DecompIn.java` | out.c addr… | decompile the functions containing each address |
| `AsmRange.java` | out.txt start end | disassembly listing of a range |
| `FuncRanges.java` | out.txt | entry, start, end of every function |
| `DecompRange.java` | out.c lo hi | decompile every function whose entry lies in lo..hi |
