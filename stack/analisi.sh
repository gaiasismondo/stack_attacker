#/bin/bash
ANALYSIS_CONT=$(sudo docker ps | grep analisi |cut -c 1-6)
sudo docker exec -it $ANALYSIS_CONT /bin/bash 

