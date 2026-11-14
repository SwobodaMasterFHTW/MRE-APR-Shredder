// Author: David
// Module: Control
// Rosenstil P-controller (Skript).
// Reads odom/goal/manual from SHM, sends cmd_vel to TCP 9999.
// Wire format: ---START---{"linear": X, "angular": Y}___END___\n

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <csignal>
#include <chrono>
#include <thread>
#include <string>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "shared_data.h"

static volatile sig_atomic_t g_stop = 0;
static void on_sig(int) { g_stop = 1; }

static uint64_t now_ns() {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static double norm(double a) { return atan2(sin(a), cos(a)); }
static double clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---------- SHM (nur das Nötigste) ----------
template <typename T>
static T* shm_open_read(const char* name, const char* sem_name, sem_t** out_sem) {
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd < 0) return nullptr;
    T* p = (T*)mmap(0, sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return nullptr;
    *out_sem = sem_open(sem_name, 0);
    return (*out_sem != SEM_FAILED) ? p : nullptr;
}

template <typename T>
static T read_safe(T* p, sem_t* s) {
    T c{}; if (!p || !s) return c;
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 100000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    if (sem_timedwait(s, &ts) == 0) { c = *p; sem_post(s); }
    return c;
}

// ---------- TCP ----------
static int tcp_connect(const std::string& ip, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &a.sin_addr);
    if (connect(s, (sockaddr*)&a, sizeof(a)) < 0) { close(s); return -1; }
    return s;
}

static void tcp_send(int sock, double v, double w) {
    if (sock < 0) return;
    char buf[128];
    int n = snprintf(buf, sizeof(buf),
        "---START---{\"linear\": %.4f, \"angular\": %.4f}___END___\n", v, w);
    send(sock, buf, n, MSG_NOSIGNAL);
}

// ---------- Rosenstil (Formel aus Skript) ----------
static void rosenstil(double x, double y, double th,
                      double xg, double yg, double thg,
                      double kp, double ka, double kb,
                      double vmax, double wmax,
                      double& v, double& w) {
    double dx = xg - x;
    double dy = yg - y;
    double rho   = sqrt(dx*dx + dy*dy);
    double alpha = norm(-th + atan2(dy, dx));
    double beta  = norm(-th - alpha);

    v = kp * rho;
    w = ka * alpha + kb * beta;

    // Ziel erreicht?
    if (rho < 0.10) { v = 0; w = 0; }

    v = clamp(v, -vmax, vmax);
    w = clamp(w, -wmax, wmax);
}

// ---------- main ----------
int main(int argc, char** argv) {
    std::string ip = "192.168.100.50";
    int port = 9999, rate = 20;
    double kp = 0.3, ka = 1.0, kb = 0.3, vmax = 0.15, wmax = 1.5;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--ip" && i+1 < argc) ip = argv[++i];
        else if (a == "--rate" && i+1 < argc) rate = atoi(argv[++i]);
    }

    struct sigaction sa{}; sa.sa_handler = on_sig;
    sigaction(SIGINT, &sa, 0); sigaction(SIGTERM, &sa, 0);

    sem_t *s_odom, *s_goal, *s_manual;
    OdomShared*   o = shm_open_read<OdomShared>  (SHM_ODOM_NAME,   SEM_ODOM_NAME,   &s_odom);
    GoalShared*   g = shm_open_read<GoalShared>  (SHM_GOAL_NAME,   SEM_GOAL_NAME,   &s_goal);
    ManualShared* m = shm_open_read<ManualShared>(SHM_MANUAL_NAME, SEM_MANUAL_NAME, &s_manual);

    if (!o || !g || !m) {
        fprintf(stderr, "SHM fehlt – ./test_tools init laufen lassen!\n");
        return 1;
    }

    int sock = tcp_connect(ip, port);
    if (sock < 0) {
        fprintf(stderr, "TCP connect zu %s:%d fehlgeschlagen!\n", ip.c_str(), port);
        return 1;
    }
    printf("[ctl] verbunden mit %s:%d @ %d Hz\n", ip.c_str(), port, rate);

    auto period = std::chrono::milliseconds(1000 / rate);
    uint64_t last_seq = 0;

    while (!g_stop) {
        auto t0 = std::chrono::steady_clock::now();

        OdomShared   odom = read_safe(o, s_odom);
        GoalShared   goal = read_safe(g, s_goal);
        ManualShared man  = read_safe(m, s_manual);
        uint64_t tn = now_ns();

        bool odom_ok = odom.valid && (tn - odom.stamp_ns < 500000000ULL);
        bool man_on  = man.enable && (tn - man.stamp_ns < 2000000000ULL);

        if (goal.valid && goal.seq != last_seq) {
            last_seq = goal.seq;
            printf("[ctl] Ziel: x=%.2f y=%.2f th=%.2f\n", goal.x, goal.y, goal.theta);
        }

        double v = 0, w = 0;

        if (!odom_ok)                    { v = 0; w = 0; }   // Safety-Stop
        else if (man_on)                 { v = man.linear_x; w = man.angular_z; }
        else if (goal.valid && goal.seq == last_seq) {
            rosenstil(odom.x, odom.y, odom.theta,
                      goal.x, goal.y, goal.theta,
                      kp, ka, kb, vmax, wmax, v, w);
        }
        // sonst: IDLE → v = w = 0

        tcp_send(sock, v, w);

        auto el = std::chrono::steady_clock::now() - t0;
        if (el < period) std::this_thread::sleep_for(period - el);
    }

    printf("[ctl] fahre herunter, sende Stopp\n");
    tcp_send(sock, 0.0, 0.0);
    close(sock);
    return 0;
}
