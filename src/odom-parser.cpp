// Odom Parser Program
// Severin Swoboda

// Based on sample program by: Lucas Muster
// Description sample code: Simple TCP client using sockets. Connects to server, sends message, and receives response.

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <csignal>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include "odom_data.h" // Include the odometry data struct


// Shutdown flag
volatile std::sig_atomic_t running = 1;

// TCP connection settings
constexpr int PORT = 5000;
constexpr int BUFFER_SIZE = 1024;
const std::string IP = "127.0.0.1";

// Overwrite in case of shutdown
void signal_handler(int signal) {
    running = 0;
}

// Function to extract pose and twist data from TCP json message
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

// TCP connect function
// Creating a new socket for each attempt
// Tries to reconnect, with increasing wait time
int tcp_connect(const sockaddr_in& server_addr) {
    uint32_t wait_time = 5;

    while (running) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);

        // Check if socket creation was successful
        if (sock < 0) {
            std::cerr << "Odometry Error: Socket creation failed\n";
            sleep(wait_time);
            continue;
        }

        // Try to connect, exit if successful
        if (connect(sock, (const sockaddr*)&server_addr,
                    sizeof(server_addr)) == 0) {

            std::cout << "Odometry Message: TCP connection successful\n";
            return sock;
        }

        std::cerr << "Odometry Error: TCP connect failed\n"
                  << "\tAttempting reconnect in " << wait_time << " seconds\n";

        // Close, wait, increase wait time, then try again
        close(sock);
        sleep(wait_time);

        if (wait_time < 60)
            wait_time = wait_time + 5;
    }

    return -1;
}

int main() {
    //
    // Setup before the main process
    //

    // Register shutdown handlers, ready to overwrite 'running' variable
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // TCP connection setup
    int sock = -1;
    sockaddr_in server_addr{};
    char buffer[BUFFER_SIZE] = {0};

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, IP.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Odometry Error: Invalid address\n";
        return 1;
    }

    // Create shared memory
    int shm_fd = shm_open(
        "/odom",
        O_CREAT | O_RDWR,
        0666
    );

    // Error flag
    if (shm_fd < 0) {
        std::cerr << "Odometry Error: Shared memory failed\n";
        return 1;
    }

    ftruncate(shm_fd, sizeof(OdomData));

    // Map shared memory
    OdomData* current_odom = (OdomData*)mmap(
        nullptr,
        sizeof(OdomData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        shm_fd,
        0
    );

    // Create semaphore
    sem_t* mutex = sem_open(
        "/odom_sem",
        O_CREAT,
        0666,
        1
    );

    // Error flags
    if (mutex == SEM_FAILED) {
        std::cerr << "Odometry Error: Semaphore failed to open\n";
        return 1;
    } 
    else if (current_odom == MAP_FAILED) {
        std::cerr << "Odometry Error: Mapping of shared memory failed\n";
        return 1;
    }

    //
    // Main process loop
    // Only engage while not interrupted by shutdown handler
    // 
    while(running) {
        // Create socket and try connection
        sock = tcp_connect(server_addr);

        // Check if process was interrupted
        if (sock < 0) {
            break;
        }

        // Update connection status
        sem_wait(mutex);
        current_odom->active = true;
        sem_post(mutex);

        while(running) {
            // Receive response
            int bytes_received = read(sock, buffer, BUFFER_SIZE - 1);

            // If data was received, parse, share and print updated data
            if(bytes_received > 0) {
                std::string message(buffer, bytes_received);

                // Lock shared memory via semaphore
                sem_wait(mutex);

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

                // Update status parameters
                current_odom->active = true;

                current_odom->timestamp =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    ).count();

                current_odom->sequence_counter++;

                // Unlock shared memory via semaphore
                sem_post(mutex);

                // Print pose data
                std::cout << "Odometry Position: X " << current_odom->pose.position[0]
                            << ", Y " << current_odom->pose.position[1]
                            << ", Z " << current_odom->pose.position[2] << "\n";

                std::cout << "Odometry Orientation: X " << current_odom->pose.orientation[0]
                            << ", Y " << current_odom->pose.orientation[1]
                            << ", Z " << current_odom->pose.orientation[2] 
                            << ", W " << current_odom->pose.orientation[3] << "\n";
                
                // Wait for new signal
                usleep(100000);
            }
            // Else break, to then prepare for reconnect
            else {
                std::cerr << "Odometry Error: TCP disconnected or read failed\n";
                break;
            }
        }

        // Lock shared memory via semaphore
        sem_wait(mutex);
        current_odom->active = false;
        sem_post(mutex);

        close(sock);
    }

    // Final cleanup in case of shutdown
    std::cout << "Odometry Message: Shutdown detected, starting cleanup\n";

    // Update message status
    sem_wait(mutex);
    current_odom->active = false;
    sem_post(mutex);

    // Close socket, descriptor and semaphore, release descriptor
    close(sock);
    munmap(current_odom, sizeof(OdomData));
    close(shm_fd);
    sem_close(mutex);

    std::cout << "Odometry Message: Cleanup complete, exiting\n";

    return 0;
}
