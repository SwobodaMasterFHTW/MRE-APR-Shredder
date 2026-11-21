// Ali Shamkhani

#ifndef STEUERUNG_H
#define STEUERUNG_H
#include"Fahrbefehl.h"
#include"Tastenstatus.h"


class Steuerung
{
    public:
        Steuerung();
        ~Steuerung();


        Fahrbefehl GetBefehl();


        void Stop();
        void Aktualisieren(const Tastenstatus& a);


    private:

        Fahrbefehl Befehl;

};

#endif // STEUERUNG_H
