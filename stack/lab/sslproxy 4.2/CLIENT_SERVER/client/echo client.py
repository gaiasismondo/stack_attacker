#!/usr/bin/env python3

import socket

HOST = '172.17.0.2'  # The server's hostname or IP address
PORT = 3782      # The port used by the server

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    s.sendall(b'Hello, world')
    data = s.recv(2048)

print('Received', repr(data))
