FROM ubuntu:20.04

RUN apt-get update && apt-get upgrade -y

RUN DEBIAN_FRONTEND="noninteractive" apt-get install -y \
    apt-utils \
	vim \
	sudo \
	bash \
	wget \
	curl \
	gnupg \
	net-tools \
	iputils-ping \
	python \
	python3 \
	nmap \
	nano \
	iproute2 \
	netcat \
    librdkafka-dev \
    libyajl-dev \
    kafkacat \
    tcpdump \
	telnet \
	netcat \
	&& rm -rf /var/lib/apt/lists/*


