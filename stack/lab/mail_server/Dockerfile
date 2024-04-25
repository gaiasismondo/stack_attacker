# sistema operativo
FROM ubuntu:20.04

# installazione dipendenze opensmtpd
RUN apt-get update && apt-get install -y \
	wget \
	autoconf \
	bison \
	libtool \
	libevent-dev \
	make \
	zlib1g-dev \
	libssl-dev \
	libasr-dev \
	&& rm -rf /var/lib/apt/lists/*
	
# installo net-tools per avere a disposizione ifconfig
# e rendere maggiormente agevoli i test
RUN apt-get update && apt-get install -y \
	net-tools \
	&& rm -rf /var/lib/apt/lists/*

# installazione ssh
RUN apt-get update && DEBIAN_FRONTEND="noninteractive" TZ="Etc/UTC" apt-get install -y \
	ssh \
	&& rm -rf /var/lib/apt/lists/*

# download e installazione di opensmtpd
RUN wget https://www.opensmtpd.org/archives/opensmtpd-6.6.1p1.tar.gz \
	&& tar xzvf opensmtpd-6.6.1p1.tar.gz && cd opensmtpd-6.6.1p1 \
	&& ./configure && make && make install

# configurazione di opensmtpd
RUN cd /etc && mkdir mail \
	&& echo "root: bob\n\
	postmaster: root\n\
	hostmaster: root\n\
	webmaster: root" > ./mail/aliases \
	&& echo "example.org" > /usr/local/etc/mailname \
	&& echo "table aliases file:/etc/mail/aliases\n\
	listen on 0.0.0.0\n\
	action \"local\" mbox alias <aliases>\n\
	match from any for domain \"example.org\" action \"local\"\n\
	match for local action \"local\"\n" > /usr/local/etc/smtpd.conf

# aggiunta utenti e gruppi necessari per il funzionamento di opensmtpd
RUN mkdir /var/empty && useradd -r -d /var/empty -s /bin/false _smtpd \
	&& useradd -r -d /var/empty -s /bin/false _smtpq \
	&& useradd -ms /bin/bash bob

# quando viene avviato il container avvia ssh e opensmtpd
CMD smtpd && /bin/bash

