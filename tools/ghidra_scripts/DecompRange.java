import ghidra.app.script.GhidraScript; import ghidra.app.decompiler.*; import java.io.*;
public class DecompRange extends GhidraScript { public void run() throws Exception {
  String[] a=getScriptArgs(); var out=new PrintWriter(new FileWriter(a[0])); var di=new DecompInterface(); di.openProgram(currentProgram);
  long lo=Long.decode(a[1]), hi=Long.decode(a[2]); int n=0;
  for (var f: currentProgram.getFunctionManager().getFunctions(toAddr(lo), true)) {
    if (f.getEntryPoint().getOffset() > hi) break;
    var r=di.decompileFunction(f,90,monitor); n++;
    out.println("//==== "+f.getName()+" @ "+f.getEntryPoint()+" .. "+f.getBody().getMaxAddress());
    out.println(r.decompileCompleted()? r.getDecompiledFunction().getC():"// FAILED"); }
  out.close(); println("DECOMPILED="+n); }}
