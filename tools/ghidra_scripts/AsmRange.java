import ghidra.app.script.GhidraScript; import java.io.*;
public class AsmRange extends GhidraScript { public void run() throws Exception { String[] a=getScriptArgs(); var out=new PrintWriter(new FileWriter(a[0]));
  var start=toAddr(Long.decode(a[1])); var end=toAddr(Long.decode(a[2]));
  var it=currentProgram.getListing().getInstructions(new ghidra.program.model.address.AddressSet(start,end),true);
  while(it.hasNext()){ var ins=it.next(); out.println(ins.getAddress()+"  "+ins); } out.close(); }}
