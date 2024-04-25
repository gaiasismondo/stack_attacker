FROM ubuntu:20.04

RUN apt-get install bash
RUN apt-get update
RUN apt-get install -y dsniff
RUN apt-get install -y sslsplit
RUN apt-get install -y net-tools
RUN apt-get install -y nmap
RUN apt-get install -y iputils-ping
RUN apt-get install -y vim
RUN apt-get install -y npm
RUN apt-get install -y iptables
RUN mkdir /var/run/sslproxy
COPY /MMSproxy /MMSproxy

