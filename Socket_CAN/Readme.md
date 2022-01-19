CANbus messages retranslation from BTA bus into Dome bus

RUN server:

    ./soccanserver -i localhost -l log.log -a ca/ca/ca_cert.pem -c ca/server/server_cert.pem -k ca/server/private/server_key.pem


RUN client:

    ./soccanclient -i localhost -a ca/ca/ca_cert.pem -c ca/client/client_cert.pem -k ca/client/private/client_key.pem



Compile i686: 
export LDFLAGS=-m32
