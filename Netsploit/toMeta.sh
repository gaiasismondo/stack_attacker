METASPLOIT_CONT=$(sudo docker ps | grep metasploit |cut -c 1-6)
sudo docker exec -it $METASPLOIT_CONT /bin/bash

