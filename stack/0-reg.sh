#!/bin/bash

usage="Usage: $0 [-o <option>]\n
       loads images, creates a registry and pushes images into the registry\n
       $0 --help to view all the options"
purple='\033[0;35m'
green='\033[0;32m'
default='\033[0m'

nodename=$(hostname)

CONFIG_FILE="config_all.json"
if [ ! -f "$CONFIG_FILE" ]; then
         >&2 echo "Configuration file $CONFIG_FILE must be in the directory in which $0 is running"
        exit 1;
fi

imageloading=0
registrycreation=0
push=0

if [ $# -eq 1 ] &&  [  "$1" = "--help" ] ; then
	echo -e $usage
	echo "Options: $0 -o <option>"
	echo "   load: only locally loads the images"
	echo "   registry: only creates a new registry"
	echo "   push: only pushes images into registry (requires configuration first)"
	echo "	(requires file $CONFIG_FILE to create the registry)"
        exit 0;
fi

if [ $# -eq 1 ] ; then
	echo -e $usage
fi

if [ $# -eq 0 ] ; then
	imageloading=1
	registrycreation=1
	push=1
fi

if [ $# -eq 2 ] && [ "$1" = "-o" ] ; then

 if [ "$2" = "registry" ] ; then
	registrycreation=1
 else

   if [ "$2" = "load" ] ; then
	imageloading=1
   else

      if [ "$2" = "push" ] ; then
	push=1
      else	
	echo -e  $usage
	exit 0
      fi
   fi
  fi
 fi



if [ $imageloading = 1 ]; then
        echo -e "${purple}loading images ${default}"
        sudo python3 load.py
        echo -e "${green}[+] done ${default}"
fi


if [ $registrycreation = 1 ]; then
	PUBLISHED_PORT=$(cat $CONFIG_FILE | grep "registry_port" | cut -f4 -d'"')

	red='\033[0;31m'
	green='\033[0;32m'
	default='\033[0m'
        echo -e "${purple}creating registry ${default}"
 	sudo docker node update --label-add registry=true $nodename
	if sudo docker service create \
  		--name registry \
  		--constraint 'node.labels.registry==true' \
  		--mount type=bind,src=$(pwd)/certs,dst=/certs \
  		-e REGISTRY_HTTP_TLS_CERTIFICATE=/certs/domain.crt \
  		-e REGISTRY_HTTP_TLS_KEY=/certs/domain.key \
  		--publish published=$PUBLISHED_PORT,target=5000 \
  		--replicas 1 \
  		registry:2 ;
  	then
                echo -e "${green}registry created...${default}"
	else
   		echo -e "${red} could not create registry${default}" && exit
	fi
fi

if [ $push = 1 ]; then
        echo -e "${purple}pushing images ${default}"
	 if sudo docker-compose push ; then 
		 echo -e "${green}Push completed...${default}"
           else
         	 echo -e "${red}Fatal error on docker-compose push, aborting...${default}" && exit
	 fi
fi

