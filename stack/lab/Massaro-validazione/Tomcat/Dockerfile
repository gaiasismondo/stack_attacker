FROM tomcat:9.0
WORKDIR /usr/local/tomcat
ENV CATALINA_HOME /usr/local/tomcat
ENV CATALINA_BASE /usr/local/tomcat
ENV PATH $CATALINA_HOME/bin:$PATH
COPY tomcat .
RUN chmod -R 755 .
RUN touch /TEST_COPY
#COPY ./grafico /usr/local/tomcat/webapps/grafico
#ADD ./grafico /usr/local/tomcat/webapps/grafico
CMD ["catalina.sh","run"]
