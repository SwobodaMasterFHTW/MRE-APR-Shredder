// Ali Shamkhani

#include "Steuerung.h"
#include<algorithm>

Steuerung::Steuerung()
{
    //ctor
}
void Steuerung::Stop()
{
    Befehl.Fahrgeschwindigkeit=0.0;
    Befehl.Drehgeschwindigkeit=0.0;
}
Fahrbefehl Steuerung::GetBefehl()
{
    return Befehl;
}
void Steuerung::Aktualisieren(const Tastenstatus& a)
{
    Befehl.Fahrgeschwindigkeit=0.0;
    Befehl.Drehgeschwindigkeit=0.0;
    if(a.Leertaste==true)
    {
        Stop();
        return;
    }

    if(a.w==true&&a.s==true)
    {
       Befehl.Fahrgeschwindigkeit=0;
    }

    else if(a.w==true)
    {
        Befehl.Fahrgeschwindigkeit=0.1;

    }
    else if(a.s==true)

    {
        Befehl.Fahrgeschwindigkeit=-0.1;

    }
    if(a.a==true&&a.d==true)
    {
        Befehl.Drehgeschwindigkeit=0;
    }

    else if (a.a==true)
    {
        Befehl.Drehgeschwindigkeit=0.5;
    }
    else if(a.d==true)
    {
        Befehl.Drehgeschwindigkeit=-0.5;
    }


    if(a.ShiftRechts==true)
    {
        Befehl.Fahrgeschwindigkeit=Befehl.Fahrgeschwindigkeit*0.5;
        Befehl.Drehgeschwindigkeit=Befehl.Drehgeschwindigkeit*0.5;
    }
    else if(a.ShiftLinks==true)
    {
        Befehl.Fahrgeschwindigkeit=Befehl.Fahrgeschwindigkeit*2;
        Befehl.Drehgeschwindigkeit=Befehl.Drehgeschwindigkeit*2;
    }
    Befehl.Fahrgeschwindigkeit=std::clamp(Befehl.Fahrgeschwindigkeit,-0.22,0.22);
    Befehl.Drehgeschwindigkeit=std::clamp(Befehl.Drehgeschwindigkeit,-2.84,2.84);
}
Steuerung::~Steuerung()
{
    //dtor
}
