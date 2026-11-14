// Odom Parser Program
// Severin Swoboda

// Based on sample program by: Lucas Muster
// Description sample code: Simple TCP client using sockets. Connects to server, sends message, and receives response.

#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>

// TCP connection settings
constexpr int PORT = 9997;
constexpr int BUFFER_SIZE = 1024;
const std::string IP = "192.168.100.50";

int main() {
    int sock = 0;
    sockaddr_in server_addr{};
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, IP.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address\n";
        return 1;
    }

    // Connect to server
    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        return 1;
    }

    // Send message
    // const char* message = "Hello from client.";
    // send(sock, message, strlen(message), 0);

    // Receive response
    int bytes_received = read(sock, buffer, BUFFER_SIZE - 1);
    if (bytes_received > 0) {
        std::cout << "Server response: " << buffer << "\n";
    }

    close(sock);
    return 0;
}
