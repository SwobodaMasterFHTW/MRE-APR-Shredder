// Odometry data structure
// Severin Swoboda 

#include <cstdint>

#ifndef ODOM_DATA_H
#define ODOM_DATA_H

struct Pose
{
    double position[3] = {0,0,0};
    double orientation[4] = {0,0,0,0};
};

struct Twist
{
    double linear[3] = {0,0,0};
    double angular[3] = {0,0,0};
};

struct OdomData
{
    Pose pose;
    Twist twist;
    char frame_id[32] = "odom";

    bool active = 0; // Connection status
    uint64_t timestamp = 0; // Last active message timestamp
    uint64_t sequence_counter = 0; // Total number of processed messages
};

#endif