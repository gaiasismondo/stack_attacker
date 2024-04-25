import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Scanner;

public class Controller {

    private static File input;
    private static File script;
    private static File ris;
    private static File image;
    private static File csv;
    private static int sceltaModalita; //indica se raccogliere tutte le evidenze (1) o soltanto quelle nuove (2)
    private static boolean modificato = false; //indica se il file di input è stato aggiornato (true) o no (false)

    private static void impostaFile() throws IOException {
        ArrayList list = ConfigurationProperties.getProperties("controllerConfig.properties");
        String[] keys = (String[]) list.toArray(new String[list.size()]);

        input = new File(keys[0]);
        script = new File(keys[1]);
        ris = new File(keys[2]);
        image = new File(keys[3]);
        csv = new File(keys[4]);
    }


    public static void getDatiConsumer(String key, String topic, String value) throws FileNotFoundException {
        // Stampa valori prelevati
        System.out.println("Dati prelevati:\n" + "Chiave: " + key + "\nTopic: " + topic + "\nValore: " + value + "\n");

        // Modifica del file di input
        modificaInput(value);
    }

    private static void modificaInput(String value) throws FileNotFoundException {
        Scanner sc = new Scanner(input);
        DBN_Model_complete m = new DBN_Model_complete(input, script, ris, image, csv);

        // Metodi per cercare una stringa: contains(), matches() e pattern.matcher().find()
        System.out.println("Stringa value ha file integrity?: "+value.contains("\"type\" : \"file_integrity\""));
        //EVIDENZA -> NonExTopic
        if(value.contains("(not allowed)") && value.contains("\"protocol\" : \"mqtt\"")) {
            while (sc.hasNextLine() && sceltaModalita == 2) {
                if(sc.nextLine().contains("NonExTopic")) {
                    System.out.println("Evidenza già raccolta: non verrà salvata");
                    sc.close();
                    return;
                }
            }
            try
            {
                m.update_input("NonExTopic", 2);
                modificato = true;
            }
            catch(IOException e)
            {
                System.err.println("IOException: " + e.getMessage());
            }
        }

        //EVIDENZA -> CheckIntBrokerConf
        //else if(value.contains("\"type\" : \"file_integrity\"") ) {//&& value.contains("\"file_name\" : \"mosquitto.conf\"")
        else if(value.contains("\"title\":\"cp /lib/x86_64-linux-gnu/libnss")){
            while (sc.hasNextLine() && sceltaModalita == 2) {
                if(sc.nextLine().contains("CheckIntBrokerConf")) {
                    System.out.println("Evidenza già raccolta: non verrà salvata");
                    sc.close();
                    return;
                }
            }
            try
            {
                m.update_input("CheckIntBrokerConf", 1);
                modificato = true;
            }
            catch(IOException e)
            {
                System.err.println("IOException: " + e.getMessage());
            }
        }

        //EVIDENZA -> AllTopicSubs
        else if(value.contains("(all topics)") && value.contains("\"protocol\" : \"mqtt\"")) {
            while (sc.hasNextLine() && sceltaModalita == 2) {
                if(sc.nextLine().contains("AllTopicSubs")) {
                    System.out.println("Evidenza già raccolta: non verrà salvata");
                    sc.close();
                    return;
                }
            }
            try
            {
                m.update_input("AllTopicSubs", 2);
                modificato = true;
            }
            catch(IOException e)
            {
                System.err.println("IOException: " + e.getMessage());
            }
        }

        //EVIDENZA -> MsgFreqBF
        else if(value.contains("hits")) {
            while (sc.hasNextLine() && sceltaModalita == 2) {
                if(sc.nextLine().contains("MsgFreqBF")) {
                    System.out.println("Evidenza già raccolta: non verrà salvata");
                    sc.close();
                    return;
                }
            }
            try
            {
                m.update_input("MsgFreqBF", 2);
                modificato = true;
            }
            catch(IOException e)
            {
                System.err.println("IOException: " + e.getMessage());
            }
        }

        //Non modifico l'input
        else {
            System.out.println("Valore recuperato non riconosciuto\n");
        }
    }

    private static void scelteUtente() throws IOException {
        Scanner sc = new Scanner(System.in);
        System.out.println("Inserire:\n" +
                "1-per mantenere nel file di input tutte le evidenze raccolte\n" +
                "2-per cancellare dal file di input tutte le evidenze raccolte");
        int scelta = sc.nextInt();

        if(scelta == 2) {
            FileOutputStream writer = new FileOutputStream(input);
            System.out.println("Reset effettuato");
            writer.close();
        }

        System.out.println("Inserire:\n" +
                "1-per raccogliere tutte le evidenze ricevute\n" +
                "2-per raccogliere solo le nuove evidenze (non presenti nel file di input)");
        sceltaModalita = sc.nextInt();

        sc.close();
    }

    public static void main(String[] args) throws Exception {
        impostaFile();

        DBN_Model_complete m = new DBN_Model_complete(input, script, ris, image, csv);
        stdout_View v = new stdout_View(ris);
        ConsumerKafka consumerKafka = new ConsumerKafka();

        //metodo che:
        // 1- resetta il file in.txt se l'utente sceglie di cancellare le evidenze già raccolte
        // 2- permette all'utente di decidere se raccogliere nel file di input tutte le evidenze o solo quelle nuove
        scelteUtente();

        System.out.println("\nIl consumer Kafka si mette in ascolto:");
        boolean e = consumerKafka.runConsumer();

        //Una volta terminato il polling, se almeno una evidenza è stata ricevuta e il file di input è stato modificato,
        //si richiama il modello per eseguire lo script octave
        if(e && modificato) {
            System.out.println("\nEsecuzione script Octave:");
            m.runModel();
            v.show_results();
            System.out.println("Per visualizzare i risultati ottenuti aprire il proprio browser all'indirizzo: http://93.45.108.44:8080/grafico");

            System.out.println("\nIl producer Kafka invia i risultati sul broker:");
            ProducerKafka producer = new ProducerKafka();
            ArrayList<String> res = CSVParser.readCSV(csv);
            producer.runProducer(res);

            System.out.println("\nI risultati saranno trasmessi su OpenSearch tramite Logstash");
        }
        else
            System.out.println("\nNessun dato ricevuto, non eseguo lo script Octave");

        System.out.println("---------------FINE---------------");
    }
}
