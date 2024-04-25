import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.*;

public class ConfigurationProperties {
    //private static final String fileProperties = "config.properties";

    //Si può aggiungere il tipo <String> - Meglio?
    public static synchronized ArrayList getProperties(String fileProperties) throws IOException {
        ArrayList list = new ArrayList();
        Map<String,String> props = getOrderedProperties(new FileInputStream(System.getProperty("user.dir") + "/" + fileProperties)); //carica i dati dallo stream in input

        Set set = props.entrySet();
        Iterator itr = set.iterator();

        while (itr.hasNext()) {
            Map.Entry entry = (Map.Entry)itr.next();
            list.add(entry.getValue());
        }

        return list;
    }


    public static Map<String, String> getOrderedProperties(InputStream in) throws IOException{
        Map<String, String> mp = new LinkedHashMap<>();
        (new Properties(){
            public synchronized Object put(Object key, Object value) {
                return mp.put((String) key, (String) value);
            }
        }).load(in);
        return mp;
    }

    //Per ora non usato - da provare
    public static synchronized void setProperties(String fileProperties, String[] key, String[] value) throws IOException {
        Properties properties = new Properties();
        properties.load(new FileInputStream(System.getProperty("user.dir") + "/" + fileProperties));
        properties.setProperty(key[0], value[0]);
        properties.setProperty(key[1], value[1]);
        properties.setProperty(key[2], value[2]);
        properties.setProperty(key[3], value[3]);
        properties.store(new FileOutputStream(System.getProperty("user.dir") + "/" + fileProperties, false), null);
    }
}
