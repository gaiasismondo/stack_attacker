# acquisizione del sistema operativo
FROM ubuntu:20.04

#aggiornamento dei pacchetti del sistema operativo e
#installazione di pacchetti importanti base, quali:
#apt-utils      - strumenti per APT
#vim            - editor di testo
#sudo           - accesso root
#bash           - bash shell
#wget           - per ottenere file dalla rete
#curl           - per il trasferimento di dati
#gnupg          - implementazione di OpenPGP
#net-tools      - utilità generiche per la rete
#iputils-ping   - utilità generiche per la rete
#nmap           - scanner di porte
#postgresql     - database

RUN apt-get update && apt-get upgrade -y && DEBIAN_FRONTEND="noninteractive" TZ="Etc/UTC" apt-get install -y \
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
	postgresql \
	nano \
	iproute2 \
	netcat \
	&& rm -rf /var/lib/apt/lists/*

# download e installazione di metasploit-framework
RUN curl https://raw.githubusercontent.com/rapid7/metasploit-omnibus/master/config/templates/metasploit-framework-wrappers/msfupdate.erb > msfinstall && \
	chmod 755 msfinstall && \
	./msfinstall

# aggiunta utente msfuser in postgresql
RUN service postgresql start && echo "msf\nmsf" | sudo -u postgres createuser msfuser -S -R -P
# creazione db msfdb
RUN service postgresql start && sudo -u postgres createdb msfdb -O msfuser
# aggiungo alla cartella di metasploit-framework
# il file di configurazione contenente le credenziali per la connessione a msfdb
COPY ./database.yml /root/.msf4/database.yml

### TIVOLI ###
#preparazione dell'environment per l'utilizzo di SuperScript 
ADD tivoli/modules/ /opt/metasploit-framework/embedded/framework/modules/post/
ADD tivoli/resource/ /opt/metasploit-framework/embedded/framework/resource/
ADD tivoli/super/ /opt/metasploit-framework/embedded/framework/super/
ADD tivoli/dos_python/ /dos_python/
#risoluzione delle dipendenze
RUN msfconsole -q -x "bundle add paho-mqtt"

# quando viene avviato il container avvia postgresql
CMD service postgresql start && /bin/bash

### FOIS ###
ADD fois/scripts/ /fois/scripts
RUN chmod -R 755 /fois/scripts/.

### BALMA ###
ADD balma/ /balma
