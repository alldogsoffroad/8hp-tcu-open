import ghidra.app.script.GhidraScript; import ghidra.app.decompiler.*; import java.io.*;
public class DecompIn extends GhidraScript { public void run() throws Exception {
  String[] a=getScriptArgs(); var out=new PrintWriter(new FileWriter(a[0])); var di=new DecompInterface(); di.openProgram(currentProgram);
  for (int i=1;i<a.length;i++){ var f=getFunctionContaining(toAddr(Long.decode(a[i]))); if (f==null){out.println("//==== none @ "+a[i]); continue;}
    var r=di.decompileFunction(f,90,monitor); out.println("//==== "+f.getName()+" @ "+f.getEntryPoint()); out.println(r.decompileCompleted()? r.getDecompiledFunction().getC():"// FAILED"); }
  out.close(); println("DONE"); }}
