
#Id container con elastichsearc
id_container=74eb216d5848

#trasferimento e spacchettamento del file
#sudo mkdir certificati 
cd certificati &&
#sudo unzip /usr/share/elasticsearch/elasticsearch-ssl-http.zip  &&

echo "Unzip completato" 

#sudo rm -r /usr/share/elasticsearch/elasticsearch-ssl-http.zip &&
# Trasformo in formato pem
sudo openssl pkcs12 -in ca/ca.p12 -out ca/ca.crt -clcerts -nokeys 
sudo openssl pkcs12 -in elasticsearch/http.p12 -out elasticsearch/node.crt -clcerts -nokeys 
sudo openssl pkcs12 -in elasticsearch/http.p12 -out elasticsearch/node.key  -nocerts -nodes 
chmod 777 ca/ca.crt
chmod 777 elasticsearch/node.crt
chmod 777 elasticsearch/node.key

echo "Convertito"

sudo docker cp ca/ca.crt $id_container:/usr/share/opensearch/config/ca.crt 
sudo docker cp elasticsearch/node.crt  $id_container:/usr/share/opensearch/config/node.crt 
sudo docker cp elasticsearch/node.key $id_container:/usr/share/opensearch/config/node.key 

echo "Aggiunto tutto al container"

#Aggiungo file ca a logstash
sudo cp ca/ca.crt ../logstash/config/ca.crt

echo "Aggiunto a Logstash"
