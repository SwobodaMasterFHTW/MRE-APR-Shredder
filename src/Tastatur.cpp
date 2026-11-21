// Ali Shamkhani

#include "Tastatur.h"

// Ubuntu/Linux: Tastenstatus direkt aus dem Linux-Eingabegeraet lesen.
// So bleiben W/S/A/D, linkes/rechtes Shift, Leertaste und Q erhalten.
// Linux-Eingabegeraete benoetigen passende Leserechte auf /dev/input/event*.
// Es sind keine externen C++-Bibliotheken erforderlich.

#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace
{
    bool HatTaste(int Datei, int Taste)
    {
        unsigned char Tasten[(KEY_MAX + 8) / 8]{};
        if (ioctl(Datei, EVIOCGBIT(EV_KEY, sizeof(Tasten)), Tasten) < 0)
            return false;
        return (Tasten[Taste / 8] & (1U << (Taste % 8))) != 0;
    }

    // Ein Keyboard suchen, das die benoetigten Bewegungstasten besitzt.
    int TastaturOeffnen()
    {
        const char* Vorgabe = std::getenv("RC_KEYBOARD_DEVICE");
        if (Vorgabe != nullptr && Vorgabe[0] != '\0')
        {
            const int Datei = open(Vorgabe, O_RDONLY | O_NONBLOCK);
            if (Datei < 0)
                throw std::runtime_error(std::string("Tastatur nicht lesbar: ") + Vorgabe + " (" + std::strerror(errno) + ")");
            return Datei;
        }

        bool GefundenOhneRechte = false;
        for (int Nummer = 0; Nummer < 128; ++Nummer)
        {
            const std::string Pfad = "/dev/input/event" + std::to_string(Nummer);
            int Datei = open(Pfad.c_str(), O_RDONLY | O_NONBLOCK);
            if (Datei < 0)
            {
                if (errno == EACCES || errno == EPERM)
                    GefundenOhneRechte = true;
                continue;
            }

            if (HatTaste(Datei, KEY_W) && HatTaste(Datei, KEY_A) &&
                HatTaste(Datei, KEY_S) && HatTaste(Datei, KEY_D) &&
                HatTaste(Datei, KEY_LEFTSHIFT) && HatTaste(Datei, KEY_RIGHTSHIFT))
            {
                return Datei;
            }
            close(Datei);
        }

        if (GefundenOhneRechte)
            throw std::runtime_error("Kein lesbares Tastaturgeraet gefunden. Zugriffsrechte auf /dev/input/event* pruefen.");
        throw std::runtime_error("Kein passendes Tastaturgeraet in /dev/input/event* gefunden.");
    }
}

Tastatur::Tastatur() : Datei(TastaturOeffnen())
{
}

Tastatur::~Tastatur()
{
    if (Datei >= 0)
        close(Datei);
}

Tastenstatus Tastatur::GetStatus()
{
    return Status;
}

void Tastatur::Aktualisieren()
{
    // Alle seit dem letzten Aufruf eingetroffenen Tastenereignisse verarbeiten.
    input_event Ereignis{};
    while (true)
    {
        const ssize_t Anzahl = read(Datei, &Ereignis, sizeof(Ereignis));
        if (Anzahl == static_cast<ssize_t>(sizeof(Ereignis)))
        {
            if (Ereignis.type != EV_KEY)
                continue;

            // 0 = losgelassen, 1 = gedrueckt, 2 = automatische Wiederholung.
            const bool Gedrueckt = Ereignis.value != 0;
            switch (Ereignis.code)
            {
                case KEY_W: Status.w = Gedrueckt; break;
                case KEY_S: Status.s = Gedrueckt; break;
                case KEY_A: Status.a = Gedrueckt; break;
                case KEY_D: Status.d = Gedrueckt; break;
                case KEY_E: Status.e = Gedrueckt; break;
                case KEY_Q: Status.q = Gedrueckt; break;
                case KEY_M: Status.m = Gedrueckt; break;
                case KEY_H: Status.h = Gedrueckt; break;
                case KEY_F: Status.f = Gedrueckt; break;
                case KEY_SPACE: Status.Leertaste = Gedrueckt; break;
                case KEY_LEFTSHIFT: Status.ShiftLinks = Gedrueckt; break;
                case KEY_RIGHTSHIFT: Status.ShiftRechts = Gedrueckt; break;
            }
        }
        else if (Anzahl < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            break; // Aktuell keine weiteren Tastenzustaende verfuegbar.
        }
        else
        {
            // Nach Geraetefehler keine alte Bewegung beibehalten.
            Status = Tastenstatus{};
            throw std::runtime_error("Tastaturgeraet getrennt oder Lesefehler.");
        }
    }
}
