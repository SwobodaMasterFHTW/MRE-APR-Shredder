// Author: David
// Module: Shared Memory Contract
#pragma once
#include <cstdint>

#define SHM_ODOM_NAME     "/odom"
#define SHM_GOAL_NAME     "/goal"
#define SHM_MANUAL_NAME   "/manual"
#define SHM_CMDSTAT_NAME  "/cmdstat"
#define SEM_ODOM_NAME     "/sem_odom"
#define SEM_GOAL_NAME     "/sem_goal"
#define SEM_MANUAL_NAME   "/sem_manual"
#define SEM_CMDSTAT_NAME  "/sem_cmdstat"
#define SHM_FORMAT_VERSION 1u

//Odom
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

//Goal Pose
struct GoalPose
{
    double position[2] = {0,0};// x, y
    double orientation[1] = {0}; // only yaw is used
    uint8_t Goal_enable = 0; // 0 = invalid, 1 = valid werden daten geschrieben
    
    //Manual Control
    double Manual_linear_x = 0;
    double Manual_angular_z = 0;
    uint8_t Manual_enable = 0; // 0 = disable, 1 = enable manual control
};
