#!/bin/bash


 config_line=$(cat ../config.json |grep Manager_IP)
        ipstring=(${config_line//:/ })
        preip=(${ipstring[1]//\"/ })
        MANAGER_IP=${preip[0]}

	echo $MANAGER_IP
