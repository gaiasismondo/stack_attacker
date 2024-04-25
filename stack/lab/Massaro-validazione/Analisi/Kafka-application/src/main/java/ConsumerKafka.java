import org.apache.kafka.clients.consumer.*;
import org.apache.kafka.common.serialization.LongDeserializer;
import org.apache.kafka.common.serialization.StringDeserializer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Properties;

public class ConsumerKafka {

    //Variabili che rappresentano alcune proprietà del consumer
    private String bootstrapServers; //att-detec-iot:9092
    private String consumerGroupID; //java-group-consumer
    private String TOPIC; //evidences

    private Logger logger; //logger della classe che permette di stampare i dati prelevati
    private long timeout; //tempo, in millisecondi, passato ad aspettare se i dati non sono disponibili nel buffer
    private int giveUp;
    private int noRecordsCount;
    private boolean evidenza; //indica se sono arrivati dei nuovi dati

    public ConsumerKafka() {
        //costruttore vuoto
    }

    private void impostaProprieta() throws IOException {
        ArrayList list = ConfigurationProperties.getProperties("consumerConfig.properties");
        String[] keys = (String[]) list.toArray(new String[list.size()]);

        this.bootstrapServers = keys[0];
        this.consumerGroupID = keys[1];
        this.TOPIC = keys[2];
        this.giveUp = (Integer.parseInt(keys[3]));
        this.timeout = (Long.parseLong(keys[4]));

        this.logger = LoggerFactory.getLogger(ConsumerKafka.class.getName());
        this.noRecordsCount = 0;
        this.evidenza = false; //falso indica che non sono arrivati dati e non serve fare l'analisi del modello, true indica che sono arrivati nuovi dati
    }

    private Consumer<Long,String> createConsumer() {
        //Crea e popola le proprietà degli oggetti
        Properties p = new Properties();
        p.setProperty(ConsumerConfig.BOOTSTRAP_SERVERS_CONFIG, bootstrapServers);
        p.setProperty(ConsumerConfig.KEY_DESERIALIZER_CLASS_CONFIG, LongDeserializer.class.getName());
        p.setProperty(ConsumerConfig.VALUE_DESERIALIZER_CLASS_CONFIG, StringDeserializer.class.getName());
        p.setProperty(ConsumerConfig.GROUP_ID_CONFIG, consumerGroupID);
        p.setProperty(ConsumerConfig.AUTO_OFFSET_RESET_CONFIG, "earliest");

        final Consumer<Long,String> consumer = new KafkaConsumer<>(p); //Crea il consumer
        consumer.subscribe(Collections.singletonList(TOPIC)); //Iscrizione al topic

        return consumer;
    }


    public boolean runConsumer() throws IOException {
        impostaProprieta();
        final Consumer<Long,String> consumer = createConsumer();
        ConsumerRecords<Long,String> records;

        //Consuma i record
        while (true) {
            records = consumer.poll(timeout);

            if (records.count() == 0) {
                noRecordsCount++;
                if (noRecordsCount > giveUp) break;
                else continue;
            }

            for (ConsumerRecord record : records) {
                logger.info("Received new record: \n" +
                        "Key: " + record.key() + ", " +
                        "Value: " + record.value() + ", " +
                        "Topic: " + record.topic() + ", " +
                        "Partition: " + record.partition() + ", " +
                        "Offset: " + record.offset() + "\n");

                // Ricevuto il nuovo record, si passano i dati al controller che analizzerà il campo value per modificare il file input
                Controller.getDatiConsumer((String)record.key(), record.topic(), (String)record.value());
                evidenza = true;
            }

            consumer.commitAsync();
        }

        consumer.close();
        return evidenza;
    }


    public static void main(String[] args) throws IOException {
        ConsumerKafka consumer = new ConsumerKafka();
        consumer.runConsumer();
    }
}