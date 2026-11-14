// Author: David
// Module: Test Tools
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "shared_data.h"

static uint64_t now_ns() {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}
template <typename T> struct Writer {
    T* p = nullptr; sem_t* s = nullptr;
    bool open(const char* n, const char* sn) {
        int fd = shm_open(n, O_RDWR, 0666); if (fd < 0) return false;
        p = (T*)mmap(0, sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); close(fd);
        s = sem_open(sn, 0); return (p != MAP_FAILED && s != SEM_FAILED);
    }
    void put(const T& v) {
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_nsec += 100000000L;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        if (sem_timedwait(s, &ts) == 0) { *p = v; sem_post(s); }
    }
};
template <typename T> bool create_shm(const char* n, const char* sn) {
    shm_unlink(n); sem_unlink(sn);
    int fd = shm_open(n, O_CREAT | O_RDWR, 0666);
    if (fd < 0) return false;
    ftruncate(fd, sizeof(T));
    void* m = mmap(0, sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); close(fd);
    T* t = (T*)m; memset(t, 0, sizeof(T)); t->version = SHM_FORMAT_VERSION; munmap(m, sizeof(T));
    sem_t* s = sem_open(sn, O_CREAT, 0666, 1); if (s != SEM_FAILED) sem_close(s);
    return true;
}
int main(int argc, char** argv) {
    if (argc < 2) return 1;
    std::string cmd = argv[1];
    if (cmd == "init") {
        create_shm<OdomShared>(SHM_ODOM_NAME, SEM_ODOM_NAME);
        create_shm<GoalShared>(SHM_GOAL_NAME, SEM_GOAL_NAME);
        create_shm<ManualShared>(SHM_MANUAL_NAME, SEM_MANUAL_NAME);
        create_shm<CmdStatShared>(SHM_CMDSTAT_NAME, SEM_CMDSTAT_NAME);
        printf("[tools] SHM angelegt\n");
    } else if (cmd == "destroy") {
        shm_unlink(SHM_ODOM_NAME); shm_unlink(SHM_GOAL_NAME); shm_unlink(SHM_MANUAL_NAME); shm_unlink(SHM_CMDSTAT_NAME);
        sem_unlink(SEM_ODOM_NAME); sem_unlink(SEM_GOAL_NAME); sem_unlink(SEM_MANUAL_NAME); sem_unlink(SEM_CMDSTAT_NAME);
        printf("[tools] SHM gelöscht\n");
    } else if (cmd == "odom") {
        Writer<OdomShared> w; if (!w.open(SHM_ODOM_NAME, SEM_ODOM_NAME)) return 1;
        auto t0 = std::chrono::steady_clock::now();
        while (true) {
            double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            OdomShared o{}; o.version = SHM_FORMAT_VERSION; o.stamp_ns = now_ns();
            o.x = sin(t); o.y = 1.0 - cos(t); o.theta = t; o.valid = 1;
            w.put(o);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    } else if (cmd == "goal") {
        Writer<GoalShared> wg; Writer<ManualShared> wm;
        if (!wg.open(SHM_GOAL_NAME, SEM_GOAL_NAME) || !wm.open(SHM_MANUAL_NAME, SEM_MANUAL_NAME)) return 1;
        while (true) {
            GoalShared g{}; g.version = SHM_FORMAT_VERSION; g.stamp_ns = now_ns(); g.seq = 1;
            g.x = 2.0; g.y = 0.0; g.theta = 0.0; g.tol_xy = 0.1; g.tol_theta = 0.1; g.valid = 1;
            wg.put(g);
            ManualShared m{}; m.version = SHM_FORMAT_VERSION; m.stamp_ns = now_ns(); m.enable = 0;
            wm.put(m);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else if (cmd == "sink") {
        int serv = socket(AF_INET, SOCK_STREAM, 0); int one = 1;
        setsockopt(serv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(9999);
        bind(serv, (sockaddr*)&a, sizeof(a)); listen(serv, 1);
        printf("[tools] Sink lauscht auf 9999\n");
        int cl = accept(serv, 0, 0); char buf[1024];
        while (true) {
            ssize_t n = recv(cl, buf, sizeof(buf)-1, 0);
            if (n <= 0) break;
            buf[n] = 0; printf("%s\n", buf);
        }
    }
    return 0;
}
