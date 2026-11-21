// Ali Shamkhani

#ifndef TASTATUR_H
#define TASTATUR_H
#include"Tastenstatus.h"



class Tastatur
{
    public:
        Tastatur();
        ~Tastatur();

        void Aktualisieren();

        Tastenstatus GetStatus();

    private:
       Tastenstatus Status;
       int Datei{-1}; // Linux-Tastaturgeraet
};

#endif // TASTATUR_H
