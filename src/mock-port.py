# Simulated TurtleBot odom port
# Severin Swoboda

# Pushes a standard odometry data entry message to a mock port
# Used for testing data transfers without the physical TurtleBot3

import socket
import time
import math

HOST = "0.0.0.0"
PORT = 5000

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind((HOST, PORT))
server.listen(1)

print(f"Mock TurtleBot listening on {PORT}...")

conn, addr = server.accept()
print(f"Connected by {addr}")

try:
    while True:

        # Example protocol: CSV terminated by newline
        data = "---START---{\"header\": {\"seq\": 2677, \"stamp\": {\"secs\": 1791561746, \"nsecs\": 269241069}, \"frame_id\": \"odom\"}, \"child_frame_id\": \"base_footprint\", \"pose\": {\"pose\": {\"position\": {\"x\": 0.00012417568359524012, \"y\": -1.572098881297279e-05, \"z\": 0.0}, \"orientation\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.002339929109439254, \"w\": 0.9999972581863403}}, \"twist\": {\"twist\": {\"linear\": {\"x\": 0.0007669904734939337, \"y\": 0.0, \"z\": 0.0}, \"angular\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.0019746297039091587}}}___END___\n"

        conn.sendall(data.encode())

        time.sleep(0.5)  # 10 Hz
except (BrokenPipeError, ConnectionResetError):
    print("Client disconnected")
finally:
    conn.close()
    server.close()