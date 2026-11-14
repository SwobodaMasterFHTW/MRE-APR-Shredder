// Odom Parser Program
// Severin Swoboda

// Based on sample program by: Lucas Muster
// Description sample code: Simple TCP client using sockets. Connects to server, sends message, and receives response.

#include <iostream>
#include <array>
#include <vector>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>

// TCP connection settings
constexpr int PORT = 9998;
constexpr int BUFFER_SIZE = 1024;
const std::string IP = "192.168.100.50";

// Output data structure
struct Pose
{
    std::array<double, 3> position = {0,0,0};
    std::array<double, 4> orientation = {0,0,0,0};
};

struct Twist
{
    std::array<double, 3> position = {0,0,0};
    std::array<double, 4> orientation = {0,0,0,0};
};

struct OdomData
{
    Pose pose;
    Twist twist;
    std::string frame_id = "odom";
};


int main() {
    //
    // TCP connection setup
    //

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

    // Read data and convert to output structs
    // Define output struct vector
    std::vector<OdomData> odom_output;

    for(int i = 0; i < 100; ++i) {
        // Receive response
        int bytes_received = read(sock, buffer, BUFFER_SIZE - 1);

        if(bytes_received > 0) {
            OdomData current_odom;
            std::string message(buffer, bytes_received);
            size_t pos_position = message.find("\"frame_id\":");

            if (pos_position != std::string::npos) {

                // Find the x position
                size_t pos = message.find("\"position\":");

                // Find the "x": after position
                size_t start = message.find("\"x\":", pos);

                // Find the beginning of the value
                start = message.find(':', start) + 1;

                // Find the end of the value
                size_t end = message.find_first_of(",}", start);

                // Extract the value
                double x = std::stod(message.substr(start, end - start));

                current_odom.pose.position[1] = x;
            }

            odom_output.push_back(current_odom);

            std::cout << "Current Position: X " << odom_output[i].pose.position[1] << "\n";
        }
        else {
            std::cout << "Some error has occured, no bytes received!" << "\n";
        }
    }

    // Data cleanup and parsing
    // TBD

    // Error handling/Disconnection handling
    // TBD

    close(sock);
    return 0;
}
