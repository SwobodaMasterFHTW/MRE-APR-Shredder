// Sample odometry data reader
// Severin Swoboda

// Using shared memory and semaphores for data race condition
// Functionality has been tested, works in tandem with the odom-parser.cpp program

#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>

// Include the odometry data struct
#include "odom_data.h"


int main() {

    // Open existing shared memory
    int shm_fd = shm_open(
        "/odom",
        O_RDWR,
        0666
    );

    // Error flag
    if (shm_fd < 0) {
        std::cout << "*Read Error*: Shared memory failed" << "\n";
        return 1;
    }

    // Map shared memory
    OdomData* odom = (OdomData*)mmap(
        nullptr,
        sizeof(OdomData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        shm_fd,
        0
    );

    // Open existing semaphore
    sem_t* mutex = sem_open(
        "/odom_sem",
        0
    );

    // Error flag
    if (mutex == SEM_FAILED) {
        std::cout << "*Read Error*: Semaphore failed to open" << "\n";
        return 1;
    }

    while (true) {
        // Create local struct
        OdomData OdomPose;

        // Lock shared memory via semaphore
        sem_wait(mutex);

        // Read new data from shared memory
        OdomPose.pose.position[0] = odom->pose.position[0];
        OdomPose.pose.position[1] = odom->pose.position[1];
        OdomPose.pose.position[2] = odom->pose.position[2];

        OdomPose.pose.orientation[0] = odom->pose.orientation[0];
        OdomPose.pose.orientation[1] = odom->pose.orientation[1];
        OdomPose.pose.orientation[2] = odom->pose.orientation[2];
        OdomPose.pose.orientation[3] = odom->pose.orientation[2];

        // Unlock shared memory via semaphore
        sem_post(mutex);

        // Print new Pose data
        std::cout << "Reader Position: X " << odom->pose.position[0]
                    << ", Y " << odom->pose.position[1]
                    << ", Z " << odom->pose.position[2] << "\n";

        std::cout << "Reader Orientation: X " << odom->pose.orientation[0]
                    << ", Y " << odom->pose.orientation[1]
                    << ", Z " << odom->pose.orientation[2] 
                    << ", W " << odom->pose.orientation[3] << "\n";

        // Wait for new signal
        usleep(100000);
    }

    return 0;
}