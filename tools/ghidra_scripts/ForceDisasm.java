import ghidra.app.script.GhidraScript; import ghidra.app.decompiler.*; import ghidra.program.model.address.Address; import ghidra.program.model.listing.Function; import ghidra.program.model.mem.MemoryBlock; import java.io.*; import java.util.*;
public class ForceDisasm extends GhidraScript { public void run() throws Exception {
  String[] a=getScriptArgs(); var out=new PrintWriter(new FileWriter(a[0]));
  // 1) disassemble every executable block fully
  for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
    if (!b.isExecute()) continue;
    Address p=b.getStart();
    while (p.compareTo(b.getEnd())<0 && !monitor.isCancelled()) {
      if (!currentProgram.getListing().isInstruction(p)) { try { disassemble(p); } catch(Exception e){} }
      p=p.add(2);
    }
  }
  analyzeChanges(currentProgram);
  // 2) refs to the requested symbol addresses
  var di=new DecompInterface(); di.openProgram(currentProgram);
  var done=new LinkedHashSet<Address>();
  for (int i=1;i<a.length;i++){
    Address t=toAddr(Long.decode(a[i]));
    out.println("//######## refs to "+a[i]);
    for (var ref : getReferencesTo(t)) {
      var f=getFunctionContaining(ref.getFromAddress());
      out.println("//  ref "+ref.getReferenceType()+" from "+ref.getFromAddress()+(f!=null?" in "+f.getName():""));
      if (f!=null && done.add(f.getEntryPoint())) {
        var r=di.decompileFunction(f,180,monitor);
        out.println("//==== "+f.getName()+" @ "+f.getEntryPoint());
        out.println(r.decompileCompleted()? r.getDecompiledFunction().getC():"// FAILED");
      }
    }
  }
  out.close(); println("DONE "+done.size()); }}
