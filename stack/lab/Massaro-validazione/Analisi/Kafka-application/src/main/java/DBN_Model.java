import java.io.*;

public class DBN_Model {

    private File input;
    private File script;
    private File ris;

    public DBN_Model(File input, File script, File ris) {
        this.input = input;
        this.script = script;
        this.ris = ris;
    }

    public void runModel() {
        try{
            ProcessBuilder p = new ProcessBuilder("octave", "" + script, "" + input, "" + ris);
            Process proc = p.start();

            BufferedReader stdInput = new BufferedReader(new InputStreamReader(proc.getInputStream()));

            BufferedReader stdError = new BufferedReader(new InputStreamReader(proc.getErrorStream()));

            getResults(stdInput, stdError);

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void getResults(BufferedReader stdInput, BufferedReader stdError) throws IOException {
        // Read the output from the command:
        System.out.println("Here is the standard output of the command:\n");
        String s;
        while ((s = stdInput.readLine()) != null)
            System.out.println(s);

        // Read any errors from the attempted command:
        System.out.println("Here is the standard error of the command (if any):\n");
        while ((s = stdError.readLine()) != null)
            System.out.println(s);
    }

    public void update_input(String evidence, Integer valore) throws IOException {
        FileWriter fw = new FileWriter(input,true); //the true will append the new data
        fw.write(evidence + ", " + valore + "\n"); //appends the string to the file
        fw.close();
    }

    public File getInput() {
        return input;
    }

    public void setInput(File input) {
        this.input = input;
    }

    public File getScript() {
        return script;
    }

    public void setScript(File script) {
        this.script = script;
    }

    public File getRis() {
        return ris;
    }

    public void setRis(File ris) {
        this.ris = ris;
    }
}
