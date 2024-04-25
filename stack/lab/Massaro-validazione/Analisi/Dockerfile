FROM ubuntu:bionic

WORKDIR /home/Kafka-application

RUN apt-get update
RUN apt-get install nano
RUN apt-get install -y openjdk-11-jdk
RUN apt-get install octave -y
RUN apt-get install liboctave-dev -y
RUN apt-get install -y make
COPY Octave/io-2.6.3.tar.gz /home/Kafka-application/io-2.6.3.tar.gz
RUN octave --eval 'pkg install io-2.6.3.tar.gz'
COPY Kafka-application /home/Kafka-application

ENTRYPOINT ["java","-jar","Kafka-example.jar"]
