import ghidra.app.script.GhidraScript; import ghidra.program.model.symbol.SourceType; import java.nio.file.*;
public class ApplySeeds extends GhidraScript { public void run() throws Exception {
  int fn=0, lb=0;
  for (String line : Files.readAllLines(Paths.get(getScriptArgs()[0]))) {
    line=line.trim(); if (line.isEmpty() || line.startsWith("#")) continue;
    String[] f=line.split("\t"); var a=toAddr(Long.decode(f[0]));
    if (f.length>1 && f[1].equals("label")) { createLabel(a, f.length>2?f[2]:"lbl_"+f[0], true, SourceType.USER_DEFINED); lb++; continue; }
    disassemble(a); if (getFunctionAt(a)==null && createFunction(a,null)!=null) fn++;
  }
  analyzeChanges(currentProgram); println("SEEDS functions="+fn+" labels="+lb); }}
