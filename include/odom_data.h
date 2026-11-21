// Odometry data structure
// Severin Swoboda 

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
};

#endif