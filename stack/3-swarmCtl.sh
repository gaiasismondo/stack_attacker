#!/bin/bash

STACK_NAME="detection"
CONFIG_FILE="config_all.json"
if [ ! -f "$CONFIG_FILE" ]; then
         >&2 echo "Configuration file $CONFIG_FILE must be in the directory in which $0 is running"
        exit 1;
fi

SERVICES_STARTLINE=$(cat $CONFIG_FILE |grep -n "containers" |cut -f1 -d:)
SERVICES_NEXT_SECTION=$(cat $CONFIG_FILE |grep -n "other_targets" |cut -f1 -d:)
SERVICE_NO=$(($SERVICES_NEXT_SECTION - $SERVICES_STARTLINE - 2))

off=0
on=0
helpme=0
stato=0
verbose=0

if [ $# = 0 ]; then
	 off=1
         on=1
fi

if  (($#>0))
then
	if [ "$1" = "v" ] || [ "$1" = "status" ] || [ "$1" = "s" ] || ([ $# == 2 ] && [ "$2" == "v" ]); then
		stato=1
	fi

	if [ "$1" = "vv" ] || [ "$1" = "vstatus" ] || [ "$1" = "ss" ] || ([ $# == 2 ] && [ "$2" == "vv" ]); then
                verbose=1
        fi

	if [ "$1" = "v" ] || [ "$1" = "vv" ]; then
		off=1
		on=1
	fi

	if [ "$1" = "stop" ]; then
		on=0
		off=1
        fi
	if [ "$1" = "start" ]; then
		on=1
		off=0
        fi

	if [ "$1" = "--help" ]; then
                helpme=1 
        fi
fi

if [ "$helpme" -gt "0" ]; then 
	echo "Usage:
	$0 [<command>] [option] 
	with no command: restarts the stack
	commands:
		--help: prints these usage notes
		start: starts or updates the services
		stop: stops the services
		status or s: prints out the stack status
		vstatus or ss: prints out the stack status without truncating info
	options:		
		v: restarts the stack and prints the status at the end
		vv: restarts the stack and prints the status at the end not truncating info
	stack_name: $STACK_NAME
	"	
	exit 0
fi

if [ "$off" -gt "0" ]; then
	running=$(sudo docker stack ps $STACK_NAME | grep 'Running' | wc -l)
	if ((running>0))
	then
		sudo docker stack rm $STACK_NAME;
        	while ((running>0))
        	do
                	running=$(sudo docker stack ps $STACK_NAME | grep 'Running' | wc -l)
                	echo "stack $STACK_NAME: $running services still running...";
                	sleep 2;
        	done
	fi
fi

if [ "$on" -gt "0" ]; then
	sudo docker stack deploy -c stack-compose.yml $STACK_NAME;
	i=0;
	while ((i<SERVICE_NO))
	do
		i=$(sudo docker stack ps $STACK_NAME | grep 'Running' | wc -l)
        	echo "stack $STACK_NAME: $i services out of $SERVICE_NO started";
		sleep 2;
	done
	rejected=$(sudo docker stack ps $STACK_NAME | grep 'Rejected' | wc -l)
	ready=$(sudo docker stack ps $STACK_NAME | grep 'Ready' | wc -l)
	shutdown=$(sudo docker stack ps $STACK_NAME | grep 'Shutdown' | wc -l)
	wrong=$(($rejected+$shutdown+$ready))
	if [ "$wrong" -gt "0" ] ; then
		echo "	*****************************************************
	ATTENTION! Some services are not coming up correctly:
	$(sudo docker stack ps $STACK_NAME | grep 'Rejected')
	$(sudo docker stack ps $STACK_NAME | grep 'Shutdown')

	Troubleshooting:
	--Check that all required volumes are in the correct path
	*****************************************************"
	fi

fi

if [ "$stato" -gt "0" ]; then
 sudo docker stack ps $STACK_NAME
fi
if [ "$verbose" -gt "0" ]; then
 sudo docker stack ps $STACK_NAME --no-trunc
fi
