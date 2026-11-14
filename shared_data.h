// Author: David
// Module: Shared Memory Contract
#pragma once
#include <cstdint>

#define SHM_ODOM_NAME     "/april_odom"
#define SHM_GOAL_NAME     "/april_goal"
#define SHM_MANUAL_NAME   "/april_manual"
#define SHM_CMDSTAT_NAME  "/april_cmdstat"
#define SEM_ODOM_NAME     "/april_sem_odom"
#define SEM_GOAL_NAME     "/april_sem_goal"
#define SEM_MANUAL_NAME   "/april_sem_manual"
#define SEM_CMDSTAT_NAME  "/april_sem_cmdstat"
#define SHM_FORMAT_VERSION 1u

struct OdomShared {
    uint32_t version; uint32_t _pad0; uint64_t stamp_ns;
    double   x, y, theta, vx, wz;
    uint8_t  valid; uint8_t _pad1[7];
};
struct GoalShared {
    uint32_t version; uint32_t _pad0; uint64_t stamp_ns; uint64_t seq;
    double   x, y, theta, tol_xy, tol_theta;
    uint8_t  valid, cancel; uint8_t _pad1[6];
};
struct ManualShared {
    uint32_t version; uint32_t _pad0; uint64_t stamp_ns;
    double   linear_x, angular_z;
    uint8_t  enable; uint8_t _pad1[7];
};
struct CmdStatShared {
    uint32_t version; uint32_t _pad0; uint64_t stamp_ns; uint64_t goal_seq;
    double   linear_x, angular_z, rho, alpha, beta, distance_to_goal, heading_error;
    uint8_t  mode, goal_reached; uint8_t _pad1[6];
};
