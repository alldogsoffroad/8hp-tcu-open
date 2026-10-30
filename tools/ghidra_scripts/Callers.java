import ghidra.app.script.GhidraScript; import ghidra.app.decompiler.*; import ghidra.program.model.address.Address; import ghidra.program.model.listing.Function; import java.io.*; import java.util.*;
public class Callers extends GhidraScript { public void run() throws Exception {
  String[] a=getScriptArgs(); var out=new PrintWriter(new FileWriter(a[0]));
  var di=new DecompInterface(); di.openProgram(currentProgram);
  var done=new LinkedHashSet<Address>();
  for (int i=1;i<a.length;i++){
    Address t=toAddr(Long.decode(a[i]));
    out.println("//######## callers of "+a[i]);
    for (var ref : getReferencesTo(t)) {
      var f=getFunctionContaining(ref.getFromAddress());
      if (f==null) { out.println("//  (non-code) "+ref.getFromAddress()); continue; }
      out.println("//  caller "+f.getName()+" @ "+f.getEntryPoint());
      if (done.add(f.getEntryPoint())) {
        var r=di.decompileFunction(f,120,monitor);
        out.println(r.decompileCompleted()? r.getDecompiledFunction().getC():"// FAILED");
      }
    }
  }
  out.close(); println("DONE "+done.size()); }}
