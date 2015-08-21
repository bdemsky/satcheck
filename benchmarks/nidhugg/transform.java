import java.io.*;

public class transform {
	public static void main(String args[]) throws Exception {
		int num=Integer.parseInt(args[0]);
		String inputfile=args[1];
		String output=args[2];
		BufferedReader br=new BufferedReader(new FileReader(inputfile));
		FileWriter fw=new FileWriter(output);
		while(true) {
			String input=br.readLine();
			if (input==null)
				break;
			if (input.indexOf("problemsize")!=-1) {
				int index=input.indexOf("problemsize");
				String str=input.substring(index+12);
				fw.write("{\nint index;\n");
				fw.write("for (index=0;index<"+num+";index++) {\n");
				fw.write(str+"\n");
				fw.write("}\n}\n");
			} else
				fw.write(input+"\n");
		}
		br.close();
		fw.close();

	}

}
