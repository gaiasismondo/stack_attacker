#!/bin/bash

usage="Usage: $0\n
       configures the environment: to be called before starting the stack\n
       (requires file config.json to create the registry)\n
       $0 --help to view this info\n"
purple='\033[0;35m'
green='\033[0;32m'
default='\033[0m'

CONFIG_FILE="config_all.json"
if [ ! -f "$CONFIG_FILE" ]; then
	 >&2 echo "Configuration file $CONFIG_FILE must be in the directory in which $0 is running"
        exit 1;
fi

nodename=$(hostname)

if [ $# -eq 1 ] &&  [  "$1" = "--help" ] ; then
	echo -e $usage
        exit 0;
fi

echo -e "${purple}preparing configuration ${default}"
python3 build_compose.py
python3 build_compose.py -stack
echo -e "${green}[+] done ${default}"


REGISTRY_HOST=$(cat $CONFIG_FILE | grep "registry_service" | cut -f4 -d'"')
if [ "$REGISTRY_HOST" != "$nodename" ]; then
	echo "The registry is not on this host. Setting the registry certificate"
	REGISTRY_CERTIFICATE_NAME=$(cat $CONFIG_FILE | grep "registry_certificate_file" | cut -f4 -d'"')
	REGISTRY_CERTIFICATE="./certs/$REGISTRY_CERTIFICATE_NAME"
	echo "Registry certificate's file: $REGISTRY_CERTIFICATE"
	if [ ! -f "$REGISTRY_CERTIFICATE" ]; then
		>&2 echo "Certificate $REGISTRY_CERTIFICATE must be in the directory in which $0 is running"
		exit 1;
	fi

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
fi
