import ghidra.app.script.GhidraScript; import ghidra.app.decompiler.*; import ghidra.program.model.address.Address; import ghidra.program.model.listing.Function; import java.io.*; import java.util.*;
public class SymDecomp extends GhidraScript { public void run() throws Exception {
  String[] a=getScriptArgs(); var out=new PrintWriter(new FileWriter(a[0])); var pat=java.util.regex.Pattern.compile(a[1]);
  var di=new DecompInterface(); di.openProgram(currentProgram);
  var st=currentProgram.getSymbolTable();
  var done=new LinkedHashSet<Address>();
  for (var sym : st.getAllSymbols(true)) {
    if (!pat.matcher(sym.getName()).find()) continue;
    Address ad=sym.getAddress();
    if (done.contains(ad)) continue;
    var f=getFunctionAt(ad);
    if (f==null) f=getFunctionContaining(ad);
    if (f!=null && !done.contains(f.getEntryPoint())) {
      done.add(f.getEntryPoint());
      out.println("//==== "+sym.getName()+" @ "+f.getEntryPoint());
      var r=di.decompileFunction(f,120,monitor);
      out.println(r.decompileCompleted()? r.getDecompiledFunction().getC():"// FAILED");
    } else if (f==null) {
      out.println("//SYMBOL(non-code) "+sym.getName()+" @ "+ad);
      for (var ref : getReferencesTo(ad)) {
        out.println("//   xref from "+ref.getFromAddress());
        var ff=getFunctionContaining(ref.getFromAddress());
        if (ff!=null && !done.contains(ff.getEntryPoint())) {
          done.add(ff.getEntryPoint());
          var rr=di.decompileFunction(ff,120,monitor);
          out.println("//==== [xref target] "+ff.getName()+" @ "+ff.getEntryPoint());
          out.println(rr.decompileCompleted()? rr.getDecompiledFunction().getC():"// FAILED");
        }
      }
    }
  }
  out.close(); println("DONE "+done.size()); }}
