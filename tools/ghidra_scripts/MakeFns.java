import ghidra.app.script.GhidraScript;
public class MakeFns extends GhidraScript { public void run() throws Exception { int n=0;
  for (String s: getScriptArgs()) { var a=toAddr(Long.decode(s)); disassemble(a); if (getFunctionAt(a)==null && createFunction(a,null)!=null) n++; }
  analyzeChanges(currentProgram); println("CREATED="+n); }}
