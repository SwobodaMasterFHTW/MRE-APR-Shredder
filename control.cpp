#include <iostream>
#include <cmath>
#include <algorithm>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>   // socket, connect, send
#include <netinet/in.h>   // AF_INET, SOCK_STREAM
#include <cstdio>         // snprintf Fehlerbehandlung
#include <cstring>        // memset, strerror
#include <semaphore.h>    //Posix semaphore Sprerren und Freigeben
#include <sys/mman.h>     //memory map
#include <fcntl.h>        // O_CREAT, O_RDWR
#include <unistd.h>       // close mman
#include <time.h>         // nanosleep
#include "shared_data.h" //Shared Memory Contract
#include <csignal>          

volatile sig_atomic_t g_stop = 0;

using namespace std;

double X = 0; //X position Roboter
double Y = 0; //Y position Roboter
double THETA = 0; //Winkel Roboter

double X_GOAL = 0; //X position Ziel
double Y_GOAL = 0; //Y position Ziel
double THETA_GOAL = 0; //Winkel Ziel

double Vmax = 1.0; //Maximale Geschwindigkeit
double Wmax = 1.0; //Maximale Winkelgeschwindigkeit

double kp = 0.3; //Proportionaler Faktor für lineare Geschwindigkeit
double ka = 1.0; //Proportionaler Faktor für Winkelgeschwindigkeit
double kb = 0.3; //Proportionaler Faktor für Winkelgeschwindigkeit

constexpr int  BufferSize = 128; //Größe des Buffers für die Kommunikation mit dem Roboter

constexpr char ROBOT_IP[]   = "192.168.100.50";
constexpr int  ROBOT_PORT   = 9999;

struct Deltaresult {

    double p;
    double alpha;
    double beta;

};

// Struct ergebnis der MotionCommands Funktion
struct MotionResult {

    double v;
    double w;

};  

void Readodometry(OdomData* shm, sem_t* sem) {

    sem_wait(sem); //Sprerren des Shared Memory Bereichs, um gleichzeitigen Zugriff zu verhindern

    //Lese die Daten aus dem Shared Memory Bereich
    X = shm->pose.position[0];
    Y = shm->pose.position[1];
    THETA = 2.0 * atan2(shm->pose.orientation[2],
                    shm->pose.orientation[3]);
    
    sem_post(sem); //Freigeben des Shared Memory Bereichs, um anderen Prozessen Zugriff zu ermöglichen

}

//Odometry zu p, alpha, beta
Deltaresult DeltaTarget(double x, double y, double theta, double x_goal, double y_goal, double theta_goal) {

    double delta_x = x_goal - x;
    double delta_y = y_goal - y;
    double delta_theta = theta_goal - theta;

    double p = sqrt(delta_x * delta_x + delta_y * delta_y);
    double alpha = -theta + atan2(delta_y, delta_x) ;
    double beta = - theta - alpha;

    return Deltaresult{p, alpha, beta};

}

// Deltaresult in MotionResult umrechnen
MotionResult MotionCommands (Deltaresult delta, double kp, double ka, double kb, double vmax, double wmax) {

    double v = kp * delta.p;
    double w = ka * delta.alpha + kb * delta.beta;

    // Ziel erreicht?
    if (delta.p < 0.10) { v = 0; w = 0; }

    v = clamp(v, -vmax, vmax);
    w = clamp(w, -wmax, wmax);

    return MotionResult{v, w};
}

// Funktion zum Senden von Befehlen an den Roboter über TCP/IP
double sendCommands(int sock,double v, double w) {

    // Befehle an den Roboter senden
    char buf[BufferSize];
    int n = snprintf(buf, sizeof(buf),
        "---START---{\"linear\": %.4f, \"angular\": %.4f}___END___\n", v, w);
    send(sock, buf, n, MSG_NOSIGNAL);

    return 0;
}

void on_signal(int sig) {
    g_stop = 1;
}

int main() {
    
    signal(SIGINT, on_signal);    // Ctrl+C
    signal(SIGTERM, on_signal);   // kill-Befehl

    //Shared Memory und Semaphore initialisieren
    //-------------------------------------------------------
    
    sem_t* mutex = sem_open("/odom_sem", 0);   // 0 = existiert schon

    if (mutex == SEM_FAILED) {
    perror("sem_open");
    return 1;
    }

    //Nummer SHM
    int fd = shm_open("/odom", O_RDWR, 0666);

    //Fehlermeldung, wenn shm_open fehlschlägt und return
    if (fd == -1) {
        perror("shm_open");
        return 1;
    }

    //Mappe den Shared Memory Bereich in den Adressraum des Prozesses
    OdomData* shm = (OdomData*) mmap(NULL, sizeof(OdomData), PROT_READ , MAP_SHARED, fd, 0);
    
    //Fehlermeldung, wenn mmap fehlschlägt und return
    if (shm == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    close(fd); //Schließe den File Descriptor, da er nicht mehr benötigt wird
    //-------------------------------------------------------


    //TCP/IP Verbindung zum Roboter aufbauen
    //-------------------------------------------------------
        // TCP/IP Verbindung zum Roboter aufbauen
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return -1; } // return -1 bei Fehler

    // Roboter Adresse vorbereiten
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));                 //alles nullen
    addr.sin_family = AF_INET;                      // IPv4 
    addr.sin_port   = htons(ROBOT_PORT);            // htons: Little-Endian → Big-Endian
    inet_pton(AF_INET, ROBOT_IP, &addr.sin_addr);   //zu binäre Form konvertieren
    
   //Handshake
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) { 
    perror("connect");
    close(sock);
    return -1;
    }
    //-------------------------------------------------------

    //Schleife
    //-------------------------------------------------------
    while(!g_stop) {
        
        
    Readodometry(shm, mutex); //Lese die aktuelle Position und Orientierung des Roboters aus dem Shared Memory


    MotionResult cmd = MotionCommands(DeltaTarget(X, Y, THETA, X_GOAL, Y_GOAL, THETA_GOAL), kp, ka, kb, Vmax, Wmax);
    sendCommands(sock, cmd.v, cmd.w); //Befehle an Roboter senden Über TCP/IP
    
    usleep(50000); //50ms warten

    }

    //Cleanup
    //-------------------------------------------------------
    close(sock);                        //Schließe die TCP/IP Verbindung zum Roboter
    munmap(shm, sizeof(OdomData));      //Entferne die Zuordnung des Shared Memory Bereichs aus dem Adressraum des Prozesses
    sem_close(mutex);                   //Freigeben des Shared Memory Bereichs, um anderen Prozessen Zugriff zu ermöglichen
    


    return 0;
}



