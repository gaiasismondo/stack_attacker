import org.apache.kafka.clients.producer.*;
import org.apache.kafka.common.serialization.LongSerializer;
import org.apache.kafka.common.serialization.StringSerializer;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Properties;

public class ProducerKafka {

    //Variabili che rappresentano alcune proprietà del producer
    private String bootstrapServers;
    private String clientID;
    private int sendMessageCount;
    private String TOPIC;

    public ProducerKafka() {
        //costruttore vuoto
    }

    private void impostaProprieta() throws IOException {
        ArrayList list = ConfigurationProperties.getProperties("producerConfig.properties");
        String[] keys = (String[]) list.toArray(new String[list.size()]);

        this.bootstrapServers = keys[0];
        this.clientID = keys[1];
        this.sendMessageCount = (Integer.parseInt(keys[2]));
        this.TOPIC = keys[3];
    }

    private Producer<Long, String> createProducer() {
        Properties p = new Properties();
        p.put(ProducerConfig.BOOTSTRAP_SERVERS_CONFIG,bootstrapServers);
        p.put(ProducerConfig.CLIENT_ID_CONFIG, clientID);
        p.put(ProducerConfig.KEY_SERIALIZER_CLASS_CONFIG, LongSerializer.class.getName());
        p.put(ProducerConfig.VALUE_SERIALIZER_CLASS_CONFIG, StringSerializer.class.getName());
        return new KafkaProducer<>(p);
    }

    public void runProducerTest() throws Exception {
        impostaProprieta();
        final Producer<Long, String> producer = createProducer();
        long time = System.currentTimeMillis();

        try {
            for (long index = time; index < time + sendMessageCount; index++) {
                final ProducerRecord<Long, String> record =
                        new ProducerRecord<>(TOPIC, index, "Valore topic " + index);

                RecordMetadata metadata = producer.send(record).get();

                long elapsedTime = System.currentTimeMillis() - time;
                System.out.printf("sent record(key=%s value=%s) " +
                                "meta(partition=%d, offset=%d) time=%d\n",
                        record.key(), record.value(), metadata.partition(),
                        metadata.offset(), elapsedTime);

            }
        } finally {
            producer.flush();
            producer.close();
        }
    }


    public void runProducer(ArrayList<String> res) throws Exception {
        impostaProprieta();
        final Producer<Long, String> producer = createProducer();
        long time = System.currentTimeMillis();

        try {
            long index = time;
            int id = 1; //numero della riga del file csv caricata
            for (String row : res) {
                final ProducerRecord<Long, String> record =
                        new ProducerRecord<>(TOPIC, index, id + "," + row.substring(1, row.length()-1));
                index++;
                id++;

                RecordMetadata metadata = producer.send(record).get();

                long elapsedTime = System.currentTimeMillis() - time;
                System.out.printf("sent record(key=%s value=%s) " +
                                "meta(partition=%d, offset=%d) time=%d\n",
                        record.key(), record.value(), metadata.partition(),
                        metadata.offset(), elapsedTime);
            }
        } finally {
            producer.flush();
            producer.close();
        }
    }


    public static void main(String[] args) throws Exception {
        ProducerKafka producer = new ProducerKafka();
        producer.runProducerTest();
    }
}