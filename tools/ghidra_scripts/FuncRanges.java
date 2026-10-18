import ghidra.app.script.GhidraScript; import java.io.*;
public class FuncRanges extends GhidraScript { public void run() throws Exception { var out=new PrintWriter(new FileWriter(getScriptArgs()[0]));
  for (var f: currentProgram.getFunctionManager().getFunctions(true)) { var b=f.getBody();
    out.printf("%x %x %x%n", f.getEntryPoint().getOffset(), b.getMinAddress().getOffset(), b.getMaxAddress().getOffset()); }
  out.close(); }}
