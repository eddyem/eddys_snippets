#!/bin/sh

# https://gist.github.com/zapstar/4b51d7cfa74c7e709fcdaace19233443
mkdir -p ca/private
chmod 700 ca/private
# NOTE: I'm using -nodes, this means that once anybody gets
# their hands on this particular key, they can become this CA.
openssl req \
    -x509 \
    -nodes \
    -days 36524 \
    -newkey rsa:4096 \
    -keyout ca/private/ca_key.pem \
    -out ca/ca_cert.pem \
    -subj "/C=RU/ST=KChR/L=Bukovo/O=SAO RAS/CN=sao.ru"

# Create server private key and certificate request
mkdir -p server/private
chmod 700 ca/private
openssl genrsa -out server/private/server_key.pem 4096
openssl req -new \
    -key server/private/server_key.pem \
    -out server/server.csr \
    -subj "/C=RU/ST=KChR/L=Bukovo/O=SAO RAS"

# Create client private key and certificate request
mkdir -p client/private
chmod 700 client/private
openssl genrsa -out client/private/client_key.pem 4096
openssl req -new \
    -key client/private/client_key.pem \
    -out client/client.csr \
    -subj "/C=RU/ST=KChR/L=Bukovo/O=SAO RAS"

# Generate certificates
openssl x509 -req -days 36524 -in server/server.csr \
    -CA ca/ca_cert.pem -CAkey ca/private/ca_key.pem \
    -CAcreateserial -out server/server_cert.pem
openssl x509 -req -days 36524 -in client/client.csr \
    -CA ca/ca_cert.pem -CAkey ca/private/ca_key.pem \
    -CAcreateserial -out client/client_cert.pem

# Now test both the server and the client
# On one shell, run the following
# openssl s_server -CAfile ca/ca_cert.pem -cert server/server_cert.pem -key server/private/server_key.pem -Verify 1
# On another shell, run the following
# openssl s_client -CAfile ca/ca_cert.pem -cert client/client_cert.pem -key client/private/client_key.pem
# Once the negotiation is complete, any line you type is sent over to the other side.
# By line, I mean some text followed by a keyboard return press.
