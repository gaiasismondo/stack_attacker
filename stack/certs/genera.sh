#!/bin/bash

config_line=$(cat ../config.json |grep Manager_IP)
ipstring=(${config_line//:/ })
preip=(${ipstring[1]//\"/ })
MANAGER_IP=${preip[0]}

cp template-san.cnf san.cnf
sed -i 's/MANAGERIP/'"$MANAGER_IP"'/g' san.cnf
openssl genrsa 2048 > domain.key
chmod 400 domain.key
openssl req -new -x509 -nodes -sha1 -days 365 -key domain.key -out domain.crt -config san.cnf
certdir=/etc/docker/certs.d/$MANAGER_IP:5000
sudo mkdir -p $certdir
sudo cp domain.crt $certdir/ca.crt
sudo cp domain.crt /usr/local/share/ca-certificates/ca.crt
