import java.io.File;
import java.io.FileNotFoundException;
import java.util.Scanner;

public class stdout_View {

    private final File ris;

    public stdout_View(File ris) {
        this.ris = ris;
    }

    public void show_results() throws FileNotFoundException {
        //legge file ris e stampa contenuto a terminale
        Scanner sc = new Scanner(ris);

        while (sc.hasNextLine())
            System.out.println(sc.nextLine());
    }
}
