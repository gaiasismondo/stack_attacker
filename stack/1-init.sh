#!/bin/bash

usage="Usage: $0 \n
       creates manager node
       $0 --help to view usage"
blue='\033[0;35m'
green='\033[0;32m'
clear='\033[0m'


if [ $# -eq 1 ] &&  [  "$1" = "--help" ] ; then
	echo -e $usage
        exit 0;
fi

        echo -e "${blue}starting swarm manager ${clear}"
	sudo docker swarm init
        echo -e "${green}[+] done ${clear}"
