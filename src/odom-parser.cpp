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
    std::array<double, 3> linear = {0,0,0};
    std::array<double, 3> angular = {0,0,0};
};

struct OdomData
{
    Pose pose;
    Twist twist;
    std::string frame_id = "odom";
};

// Function to extract pose and twist data from TCI json message
double find_entry(std::string message, std::string key1, std::string key2) {
    // Find the relevant starting position ("position" or "twist")
    size_t pos = message.find(key1);

    // Find the relevant entry type after start (f.e. "x", "y", "z" for positions)
    size_t start = message.find(key2, pos);

    // Find the beginning of the entry
    start = message.find(':', start) + 1;

    // Find the end of the entry
    size_t end = message.find_first_of(",}", start);

    // Extract, convert and return the value
    double entry = std::stod(message.substr(start, end - start));

    return entry;
}

int main() {
    // TCP connection setup
    int sock = 0;
    sockaddr_in server_addr{};
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Odometry Error: TCP socket failed\n");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, IP.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Odometry Error: Invalid address\n";
        return 1;
    }

    // Connect to server
    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Odometry Error: TCP connect failed\n");
        return 1;
    }

    // Create output struct
    OdomData* current_odom = new OdomData;

    // Read data and convert to output structs
    while(true) {
        // Receive response
        int bytes_received = read(sock, buffer, BUFFER_SIZE - 1);

        if(bytes_received > 0) {
            std::string message(buffer, bytes_received);

            // Populate Odom struct
            current_odom->pose.position[0] = find_entry(message, "\"position\":","\"x\":");
            current_odom->pose.position[1] = find_entry(message, "\"position\":","\"y\":");
            current_odom->pose.position[2] = find_entry(message, "\"position\":","\"z\":");

            current_odom->pose.orientation[0] = find_entry(message, "\"orientation\":","\"x\":");
            current_odom->pose.orientation[1] = find_entry(message, "\"orientation\":","\"y\":");
            current_odom->pose.orientation[2] = find_entry(message, "\"orientation\":","\"z\":");
            current_odom->pose.orientation[3] = find_entry(message, "\"orientation\":","\"w\":");

            current_odom->twist.linear[0] = find_entry(message, "\"linear\":","\"x\":");
            current_odom->twist.linear[1] = find_entry(message, "\"linear\":","\"y\":");
            current_odom->twist.linear[2] = find_entry(message, "\"linear\":","\"z\":");

            current_odom->twist.angular[0] = find_entry(message, "\"angular\":","\"x\":");
            current_odom->twist.angular[1] = find_entry(message, "\"angular\":","\"y\":");
            current_odom->twist.angular[2] = find_entry(message, "\"angular\":","\"z\":");

            // Print Pose data
            std::cout << "Odometry Position: X " << current_odom->pose.position[0]
                        << ", Y " << current_odom->pose.position[1]
                        << ", Z " << current_odom->pose.position[2] << "\n";

            std::cout << "Odometry Orientation: X " << current_odom->pose.orientation[0]
                        << ", Y " << current_odom->pose.orientation[1]
                        << ", Z " << current_odom->pose.orientation[2] 
                        << ", W " << current_odom->pose.orientation[3] << "\n";
        }
        else {
            std::cout << "Odometry Error: No bytes received during read" << "\n";
        }
    }

    // Error and Exception handling/Disconnection handling
    // TBD

    // Final Cleanup
    close(sock);
    delete current_odom;

    return 0;
}
