#!/bin/bash
if [ $# -gt 1 ] || [ "$1" == "--help" ]; then
	>&2 echo -e "Usage: $0 [cert] [--help]" 
	echo -e "First configure all fields of config_join.cnf"
	echo -e "--help prints this info and exits"
	echo -e "If you just want to put the certificate in the right directories:
	$0 cert"	
	exit 1;
fi

CONFIG_FILE="config_all.json"
if [ ! -f "$CONFIG_FILE" ]; then
         >&2 echo "Configuration file $CONFIG_FILE must be in the directory in which $0 is running"
        exit 1;
fi


REGISTRY_CERTIFICATE=$(cat $CONFIG_FILE | grep "registry_certificate_file" | cut -f4 -d'"')

echo "Registry certificate's file: $REGISTRY_CERTIFICATE"
if [ ! -f "$REGISTRY_CERTIFICATE" ]; then
	>&2 echo "Certificate $REGISTRY_CERTIFICATE must be in the directory in which $0 is running"
	exit 1;
fi

red='\033[0;31m'
green='\033[0;32m'
purple='\033[0;35m'
default='\033[0m'

REGISTRY_HOST=$(cat $CONFIG_FILE | grep "registry_service" | cut -f4 -d'"')
REGISTRY_IP=$(cat $CONFIG_FILE | grep "IP_$REGISTRY_HOST" | cut -f4 -d'"')
REGISTRY_PORT=$(cat $CONFIG_FILE | grep "registry_port" | cut -f4 -d'"')

directory="$REGISTRY_IP:$REGISTRY_PORT"
echo "Registry's IP and port: $directory"

if [ ! -d "/etc/docker/certs.d" ]; then 
	echo -e "${purple}creating directory /etc/docker/certs.d ${default}"
	sudo mkdir /etc/docker/certs.d
	echo -e "${green}done ${default}"
fi
if [ ! -d "/etc/docker/certs.d/$directory" ]; then 
	echo -e "${purple}creating directory /etc/docker/certs.d/$directory ${default}"
	sudo mkdir /etc/docker/certs.d/$directory
	echo -e "${green}done ${default}"
fi
	
echo -e "${purple}copying the certificate to /etc/docker/certs.d/$directory/ca.crt and to /usr/local/share/ca-certificates/ca.crt ${default}"
sudo cp $REGISTRY_CERTIFICATE /etc/docker/certs.d/$directory/ca.crt
sudo cp $REGISTRY_CERTIFICATE /usr/local/share/ca-certificates/ca.crt
echo -e "${green}done ${default}"

if [ $# == 0 ]; then
	SWARM_MANAGER_HOST=$(cat $CONFIG_FILE | grep "swarm_manager" | cut -f4 -d'"')
	SWARM_MANAGER_IP=$(cat $CONFIG_FILE | grep "IP_$SWARM_MANAGER_HOST" | cut -f4 -d'"')
	SWARM_MANAGER_PORT=$(cat $CONFIG_FILE | grep "manager_port" | cut -f4 -d'"')
	SWARM_TOKEN=$(cat $CONFIG_FILE | grep "swarm_token" | cut -f4 -d'"')
	
	echo "Swarm manager's IP and port: $SWARM_MANAGER_IP:$SWARM_MANAGER_PORT"
	echo -e "${purple}joining the swarm  ${default}"
	sudo docker swarm join --token $SWARM_TOKEN $SWARM_MANAGER_IP:$SWARM_MANAGER_PORT
	echo -e "${green}done ${default}"
fi

RUN_AUDITBEAT=$(cat $CONFIG_FILE | grep "run_auditbeat" | cut -f4 -d'"')
if [ $RUN_AUDITBEAT == 1 ]; then
	KIBANA_HOST=$(cat $CONFIG_FILE | grep "opensearch-dashboards" | cut -f4 -d'"')
        KIBANA_IP=$(cat $CONFIG_FILE | grep "IP_$KIBANA_HOST" | cut -f4 -d'"')
	LOGSTASH_HOST=$(cat $CONFIG_FILE | grep "logstash-opensearch" | cut -f4 -d'"')
        LOGSTASH_IP=$(cat $CONFIG_FILE | grep "IP_$LOGSTASH_HOST" | cut -f4 -d'"')
	echo -e "${purple}configuring auditbeat with:
	IP kibana = $KIBANA_IP, IP logstash = $LOGSTASH_IP	${default}"
	sudo cp auditbeat.yml.template auditbeat.yml
	sed -i 's/IPkibana/'"$KIBANA_IP"'/g' auditbeat.yml
	sed -i 's/IPlogstash/'"$LOGSTASH_IP"'/g' auditbeat.yml
	sudo mv auditbeat.yml /etc/auditbeat/auditbeat.yml
	sudo chown root:root /etc/auditbeat/auditbeat.yml
	echo -e "${purple}starting auditbeat  ${default}"
	sudo systemctl start auditbeat
	runs=$(sudo systemctl status auditbeat | grep "Active: active (running)" | wc -l)
	if [ $runs -gt 0 ]; then
		echo -e "${green}done ${default}"
	else
		echo -e "${red}auditbeat not started
		   Troubleshooting:
		   sudo systemctl status auditbeat ${default}"
	fi
fi

