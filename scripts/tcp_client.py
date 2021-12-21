import socket
import sys


HOST = sys.argv[1]
PORT = int(sys.argv[2])


# Create a TCP/IP socket.
socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
socket.settimeout(300)
socket.connect((HOST, PORT))
print ("Start TCP Client")
print ("Ready to receive data:")
# Create a socket (SOCK_STREAM means a TCP socket)
#with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
# Connect to server and send data
while 1:

    try:
        received = socket.recv(1460)
    except Exception:
        print("connection is closed")
        break
    print("Received data: ",received)
    socket.send(received)
    print ("sent echo data")
	
print ("closing socket")
socket.close()
