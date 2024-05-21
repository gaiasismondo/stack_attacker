#Viene salvato l'id del container Metasploit_new nella variabile METASPLOIT_CONT
#Viene salvato il percorso corrente nella variabile PWD
#Viene lanciato setupAttacks.py passandogli come argomento PWD
METASPLOIT_CONT=$(sudo docker ps | grep metasploit |cut -c 1-6)

echo "configuring IPs for the attack"
PWD=$(pwd)
python3 setupAttacks.py $PWD
echo "done"

# kills older instances of msfrcpd
sudo docker exec -i $METASPLOIT_CONT /bin/bash -c 'kill `pidof msfrpcd` 2> /dev/null'

# Start database service for metasploit and initialize it
sudo docker exec -i $METASPLOIT_CONT /bin/bash -c 'service postgresql start'
sudo docker exec -i $METASPLOIT_CONT /bin/bash -c 'msfdb init'

# Start rpc servers for exploiting and backdooring 
# Vengono avviate due istanze del servizio Metasploit remote procedure call in ascolto sulle porte 1234 e 1235 
# 1234 viene usata per exploiting e 1235 per backdooring 
sudo docker exec -i $METASPLOIT_CONT /bin/bash -c 'msfrpcd -p 1234 -P password ;  msfrpcd -p 1235 -P password '
#sudo docker exec -i $METASPLOIT_CONT /bin/bash -c 'msfrpcd -p 1235 -P password &'

echo 'Metasploit container initialized'

python3 main_procedure.py
python3 main_procedure.py

