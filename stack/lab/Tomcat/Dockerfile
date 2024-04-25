FROM tomcat:9.0
WORKDIR /usr/local/tomcat
COPY tomcat .
RUN chmod -R 755 .
RUN touch /TEST_COPY
CMD ["catalina.sh","run"]
