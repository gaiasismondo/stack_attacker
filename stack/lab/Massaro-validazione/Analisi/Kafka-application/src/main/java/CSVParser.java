import com.opencsv.CSVReader;

import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.Arrays;

public class CSVParser {

    public static ArrayList<String> readCSV(File csv) {
        CSVReader reader;
        ArrayList<String> lines = new ArrayList<>();

        try {
            //parsing a CSV file into CSVReader class constructor
            reader = new CSVReader(new FileReader(csv));
            String[] nextLine;
            int riga = 0;

            //reads one line at a time
            while ((nextLine = reader.readNext()) != null) {
                if(riga != 0) { //vengono prese tutte le righe del CSV tranne l'intestazione con i nomi dei campi
                    lines.add(Arrays.toString(nextLine));
                }
                riga++;
            }
        }
        catch(Exception e) {
            e.printStackTrace();
        }
        return lines;
    }
}
