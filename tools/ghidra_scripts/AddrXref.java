import ghidra.app.script.GhidraScript; import ghidra.app.decompiler.*; import ghidra.program.model.address.Address; import ghidra.program.model.listing.Function;
import java.io.*; import java.util.*;
public class AddrXref extends GhidraScript { public void run() throws Exception {
  String[] a=getScriptArgs(); var out=new PrintWriter(new FileWriter(a[0]));
  Set<Function> fns=new LinkedHashSet<>();
  for (int i=1;i<a.length;i++) {
    Address t=currentProgram.getAddressFactory().getAddress(a[i]);
    var sym=currentProgram.getSymbolTable().getPrimarySymbol(t);
    out.println("//ADDR "+a[i]+" name="+(sym!=null?sym.getName():"?"));
    for (var ref : getReferencesTo(t)) {
      var ins=getInstructionAt(ref.getFromAddress());
      out.println("//   "+ref.getFromAddress()+" "+(ins!=null?ins.toString():"(data)")+" "+ref.getReferenceType());
      var f=getFunctionContaining(ref.getFromAddress());
      if (f!=null) fns.add(f);
      else out.println("//      (no containing function)");
    }
  }
  var di=new DecompInterface(); di.openProgram(currentProgram);
  for (var f: fns) { out.println("//==== "+f.getName()+" @ "+f.getEntryPoint());
    var r=di.decompileFunction(f,120,monitor);
    out.println(r.decompileCompleted()? r.getDecompiledFunction().getC():"// FAILED"); }
  out.close(); println("DONE "+fns.size()+" functions"); }}
