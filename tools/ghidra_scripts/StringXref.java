import ghidra.app.script.GhidraScript; import ghidra.app.decompiler.*; import ghidra.program.model.address.*; import ghidra.program.model.listing.*; import ghidra.program.model.mem.*;
import java.io.*; import java.util.*;
public class StringXref extends GhidraScript { public void run() throws Exception {
  String[] a=getScriptArgs(); var out=new PrintWriter(new FileWriter(a[0]));
  List<String> pats=new ArrayList<>(); for (int i=1;i<a.length;i++) pats.add(a[i].toLowerCase());
  var di=new DecompInterface(); di.openProgram(currentProgram);
  Set<Function> done=new LinkedHashSet<>();
  var it=currentProgram.getListing().getDefinedData(true);
  while (it.hasNext()) { var d=it.next();
    Object v=d.getValue(); if (!(v instanceof String)) continue;
    String s=((String)v).toLowerCase();
    boolean hit=false; for (String p:pats) if (s.contains(p)) {hit=true;break;}
    if (!hit) continue;
    out.println("//STR @ "+d.getAddress()+" \""+((String)v).replace("\n","\\n").replace("\r","")+"\"");
    for (var ref : getReferencesTo(d.getAddress())) {
      var f=getFunctionContaining(ref.getFromAddress());
      if (f!=null) done.add(f); else out.println("//   xref from non-fn "+ref.getFromAddress());
    }
  }
  for (var f: done) { out.println("//==== "+f.getName()+" @ "+f.getEntryPoint());
    var r=di.decompileFunction(f,120,monitor);
    out.println(r.decompileCompleted()? r.getDecompiledFunction().getC():"// FAILED"); }
  out.close(); println("DONE "+done.size()+" functions"); }}
