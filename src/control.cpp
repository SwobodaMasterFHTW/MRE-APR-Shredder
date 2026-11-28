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
uint8_t GOAL_VALID = 0; //Werden Daten geschrieben? 0 = nein, 1 = ja

double Vmax = 1.0; //Maximale Geschwindigkeit
double Wmax = 1.0; //Maximale Winkelgeschwindigkeit

double kp = 0.3; //Proportionaler Faktor für lineare Geschwindigkeit
double ka = 1.0; //Proportionaler Faktor für Winkelgeschwindigkeit
double kb = 0.3; //Proportionaler Faktor für Winkelgeschwindigkeit

//Semaphore und Shared Memory Variablen
sem_t* mutex_odom = nullptr; //Semaphore für Shared Memory Bereich Odom
sem_t* mutex_goal = nullptr; //Semaphore für Shared Memory Bereich Goal
sem_t* mutex_manual = nullptr; //Semaphore für Shared Memory Bereich Manual
OdomData* shm_odom = nullptr; //Shared Memory Bereich Odom
GoalPose* shm_goal = nullptr; //Shared Memory Bereich Goal
ManualControl* shm_manual = nullptr; //Shared Memory Bereich Manual

//TCP/IP Variablen
int TCPsock = -1; //Socket für TCP/IP Verbindung zum Roboter

bool   manual_mode  = false;   // false = autonom, true = manuell
double manual_v     = 0.0;     // m/s
double manual_w     = 0.0;     // rad/s

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

void ReadGoalPose(GoalPose* shm, sem_t* sem) {

    sem_wait(sem); //Sprerren des Shared Memory Bereichs, um gleichzeitigen Zugriff zu verhindern

    //Lese die Daten aus dem Shared Memory Bereich
    X_GOAL = shm->position[0];
    Y_GOAL = shm->position[1];
    THETA_GOAL = shm->orientation[0];
    GOAL_VALID = shm->Goal_enable;
    manual_mode = shm->Manual_enable;

    sem_post(sem); //Freigeben des Shared Memory Bereichs, um anderen Prozessen Zugriff zu ermöglichen

}

void ReadManualControl(ManualControl* shm, sem_t* sem) {

    sem_wait(sem); //Sprerren des Shared Memory Bereichs, um gleichzeitigen Zugriff zu verhindern

    //Lese die Daten aus dem Shared Memory Bereich
    manual_v = shm->Manual_linear_x;
    manual_w = shm->Manual_angular_z;
    
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

int opensemaphore() {



    //Shared Memory und Semaphore initialisieren
    //-------------------------------------------------------
    
    mutex_odom = sem_open("/odom_sem", 0);   // 0 = existiert schon
    mutex_goal = sem_open("/goal_sem", 0);   // 0 = existiert schon
    mutex_manual = sem_open("/manual_sem", 0); // 0 = existiert schon

    //Fehlermeldung, wenn sem_open fehlschlägt und return
    if (mutex_odom == SEM_FAILED) {
    perror("sem_open /odom_sem");
    return 1;
    }

    if (mutex_goal == SEM_FAILED) {
    perror("sem_open /goal_sem");
    return 1;
    }

    if (mutex_manual == SEM_FAILED) {
    perror("sem_open /manual_sem");
    return 1;
    }

    //Nummer SHM
    int fd_odom = shm_open("/odom", O_RDWR, 0666);
    int fd_goal = shm_open("/goal", O_RDWR, 0666);
    int fd_manual = shm_open("/manual", O_RDWR, 0666);

    //Fehlermeldung, wenn shm_open fehlschlägt und return
    if (fd_odom == -1) {
        perror("shm_open /odom");
        return 1;
    }

    if (fd_goal == -1) {
        perror("shm_open /goal");

        return 1;
    }   

        if (fd_manual == -1) {
        perror("shm_open /manual");

        return 1;
    }   

    //Mappe den Shared Memory Bereich in den Adressraum des Prozesses
    shm_odom = (OdomData*) mmap(NULL, sizeof(OdomData), PROT_READ , MAP_SHARED, fd_odom, 0);
    shm_goal = (GoalPose*) mmap(NULL, sizeof(GoalPose), PROT_READ , MAP_SHARED, fd_goal, 0);
    shm_manual = (ManualControl*) mmap(NULL, sizeof(ManualControl), PROT_READ , MAP_SHARED, fd_manual, 0);

    
    //Fehlermeldung, wenn mmap fehlschlägt und return
    if (shm_odom == MAP_FAILED) {
        perror("mmap /odom");
        close(fd_odom);
        close(fd_goal);
        return 1;
    }


    if (shm_goal == MAP_FAILED) {
        perror("mmap /goal");
        close(fd_odom);
        close(fd_goal);
        return 1;
    }   
    
    if (shm_manual == MAP_FAILED) {
        perror("mmap /manual");
        close(fd_odom);
        close(fd_goal);
        close(fd_manual);
        return 1;
    }


    close(fd_odom); //Schließe den File Descriptor, da er nicht mehr benötigt wird
    close(fd_goal); //Schließe den File Descriptor, da er nicht mehr benötigt wird
    close(fd_manual); //Schließe den File Descriptor, da er nicht mehr benötigt wird
    //-------------------------------------------------------

    return 0;

}

int openTCP() {


    // TCP/IP Verbindung zum Roboter aufbauen
    TCPsock = socket(AF_INET, SOCK_STREAM, 0);
    if (TCPsock < 0) { perror("socket"); return -1; } // return -1 bei Fehler

    // Roboter Adresse vorbereiten
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));                 //alles nullen
    addr.sin_family = AF_INET;                      // IPv4 
    addr.sin_port   = htons(ROBOT_PORT);            // htons: Little-Endian → Big-Endian
    inet_pton(AF_INET, ROBOT_IP, &addr.sin_addr);   //zu binäre Form konvertieren
    
   //Handshake
    if (connect(TCPsock, (sockaddr*)&addr, sizeof(addr)) < 0) { 
    perror("connect");
    close(TCPsock);
    return -1;
    }

    return 0;




}


int main() {   

    signal(SIGINT, on_signal);    // Ctrl+C
    signal(SIGTERM, on_signal);   // kill-Befehl
    
    //Initialisierung
    //-------------------------------------------------------
    if (opensemaphore() != 0) return 1;
    if (openTCP() != 0) return 1;


    //Schleife
    //-------------------------------------------------------
    while(!g_stop) {
    
    Readodometry(shm_odom, mutex_odom); //Lese die aktuelle Position und Orientierung des Roboters aus dem Shared Memory
    ReadGoalPose(shm_goal, mutex_goal); //Lese die aktuelle Zielposition und Orientierung des Roboters aus dem Shared Memory
    ReadManualControl(shm_manual, mutex_manual); //Lese die manuelle Steuerung des Roboters aus dem Shared Memory
    
    //manualControl(TCPsock); //Manuelle Steuerung des Roboters über die Konsole

    MotionResult cmd = MotionCommands(DeltaTarget(X, Y, THETA, X_GOAL, Y_GOAL, THETA_GOAL), kp, ka, kb, Vmax, Wmax);
    
    if (GOAL_VALID == 0) { cmd.v = 0; cmd.w = 0; } //Wenn keine gültigen Daten vorhanden sind, setze v und w auf 0

    if(manual_mode){

        //Manuelle Steuerung des Roboters über die Konsole
        sendCommands(TCPsock, manual_v, manual_w); //Befehle an Roboter senden Über TCP/IP

    }
    else{

        //Autonome Steuerung des Roboters über die berechneten Befehle
        sendCommands(TCPsock, cmd.v, cmd.w); //Befehle an Roboter senden Über TCP/IP

    }
    

    
    usleep(50000); //50ms warten

    }
    //-------------------------------------------------------



    //Cleanup
    //-------------------------------------------------------
    close(TCPsock);                        //Schließe die TCP/IP Verbindung zum Roboter
    munmap(shm_odom, sizeof(OdomData));      //Entferne die Zuordnung des Shared Memory Bereichs aus dem Adressraum des Prozesses
    munmap(shm_goal, sizeof(GoalPose)); //Entferne die Zuordnung des Shared Memory Bereichs aus dem Adressraum des Prozesses
    munmap(shm_manual, sizeof(ManualControl)); //Entferne die Zuordnung des Shared Memory Bereichs aus dem Adressraum des Prozesses
    sem_close(mutex_odom);                   //Freigeben des Shared Memory Bereichs, um anderen Prozessen Zugriff zu ermöglichen
    sem_close(mutex_goal);              //Freigeben des Shared Memory Bereichs, um anderen Prozessen Zugriff zu ermöglichen
    sem_close(mutex_manual);              //Freigeben des Shared Memory Bereichs, um anderen Prozessen Zugriff zu ermöglichen


    return 0;
}



