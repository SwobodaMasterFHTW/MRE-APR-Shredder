// Ali Shamkhani

#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <exception>
#include "Tastenstatus.h"
#include "Tastatur.h"
#include "Steuerung.h"
#include "Fahrbefehl.h"
#include "Betriebsmodus.h"

int main()
{
    try
    {
    Tastatur Tast;
    Steuerung Steu;
    Tastenstatus Status;
    Betriebsmodus Modus = Betriebsmodus::Manuel;

    while (true)
    {
        // Tastenzustaende einlesen: WASD, Shift links/rechts, Leertaste, Q.
        Tast.Aktualisieren();
        Status = Tast.GetStatus();

        // Q beendet die Schleife; davor die Geschwindigkeiten null setzen.
        if (Status.q)
        {
            Steu.Stop();
            break;
        }

        // Betriebsmodus aus der Originalversion beibehalten.
        if (Status.m) Modus = Betriebsmodus::Manuel;
        else if (Status.h) Modus = Betriebsmodus::Halbautonom;
        else if (Status.f) Modus = Betriebsmodus::Fullautonom;

        Fahrbefehl Befehl{};
        switch (Modus)
        {
            case Betriebsmodus::Manuel:
                // Die originale Bewegungsberechnung bleibt unveraendert.
                Steu.Aktualisieren(Status);
                Befehl = Steu.GetBefehl();
                break;
            case Betriebsmodus::Halbautonom:
            case Betriebsmodus::Fullautonom:
                // Noch keine Algorithmen integriert: nur Stoppbefehle.
                Steu.Stop();
                Befehl = Steu.GetBefehl();
                break;
        }

        // Nur ein echter Fehler im Steuerungsbaustein wird gemeldet.
        // Im Normalbetrieb erfolgt keine Konsolenausgabe.
        if (!std::isfinite(Befehl.Fahrgeschwindigkeit) ||
            !std::isfinite(Befehl.Drehgeschwindigkeit) ||
            std::abs(Befehl.Fahrgeschwindigkeit) > 0.22 ||
            std::abs(Befehl.Drehgeschwindigkeit) > 2.84)
        {
            Steu.Stop();
            std::cerr << "[Steuereinheit/Steuerung] FEHLER: "
                         "Ungueltige Fahrwerte. Steuerung gestoppt.\n";
            return 1;
        }

        // SCHNITTSTELLE ZUR GRUPPE:
        // Befehl.Fahrgeschwindigkeit  [m/s]
        // Befehl.Drehgeschwindigkeit [rad/s]
        // Hier den Befehl an den vorhandenen Motor-/TCP-Baustein uebergeben.
        // Es gibt ABSICHTLICH keine Ausgabe und keine Netzwerkverbindung.
        (void)Befehl;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
    }
    catch (const std::exception& Fehler)
    {
        // Beispielsweise: Tastaturgeraet unter Ubuntu nicht lesbar.
        std::cerr << "[Steuereinheit/Tastatur] FEHLER: "
                  << Fehler.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "[Steuereinheit] FEHLER: Unbekannter Programmfehler.\n";
        return 1;
    }
}
