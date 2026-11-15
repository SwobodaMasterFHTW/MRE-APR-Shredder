#include <iostream>
#include <cmath>
#include <algorithm>

using namespace std;

int X = 0; //X position Roboter
int Y = 0; //Y position Roboter
int THETA = 0; //Winkel Roboter

int X_GOAL = 0; //X position Ziel
int Y_GOAL = 0; //Y position Ziel
int THETA_GOAL = 0; //Winkel Ziel

int Vmax = 1.0; //Maximale Geschwindigkeit
int Wmax = 1.0; //Maximale Winkelgeschwindigkeit

int kp = 0.3; //Proportionaler Faktor für lineare Geschwindigkeit
int ka = 1.0; //Proportionaler Faktor für Winkelgeschwindigkeit
int kb = 0.3; //Proportionaler Faktor für Winkelgeschwindigkeit




int main() {
    
    //Port daten lesen

    MotionCommands(DeltaTarget(X, Y, THETA, X_GOAL, Y_GOAL, THETA_GOAL), kp, ka, kb, Vmax, Wmax);




    return 0;
}

int DeltaTarget(int x, int y, int theta, int x_goal, int y_goal, int theta_goal) {

    int delta_x = x_goal - x;
    int delta_y = y_goal - y;
    int delta_theta = theta_goal - theta;

    int p = sqrt(delta_x * delta_x + delta_y * delta_y);
    int alpha = - theta + atan2(delta_y, delta_x) ;
    int beta = - theta - alpha;

    return p, alpha, beta;

}

int MotionCommands (int p, int alpha, int beta, int kp, int ka, int kb, int vmax, int wmax) {

    double v = kp * p;
    double w = ka * alpha + kb * beta;

    // Ziel erreicht?
    if (p < 0.10) { v = 0; w = 0; }

    v = clamp(v, -vmax, vmax);
    w = clamp(w, -wmax, wmax);

    return v,w;
}

int sendCommands(int v, int w) {
    //Hier werden die Befehle an den Roboter gesendet

    

    return 0;
}

