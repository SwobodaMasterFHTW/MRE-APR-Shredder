// Ali Shamkhani
// Manual Control for TurtleBot3 – schreibt in /manual Shared Memory

#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <exception>
#include <csignal>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>

#include "Tastenstatus.h"
#include "Tastatur.h"
#include "Steuerung.h"
#include "Fahrbefehl.h"
#include "Betriebsmodus.h"
#include "shared_data.h"

static volatile sig_atomic_t g_stop = 0;
static void on_signal(int) { g_stop = 1; }

int main()
{
    try
    {
        // --- Signal-Handler ---
        signal(SIGINT, on_signal);
        signal(SIGTERM, on_signal);

        // --- SHM + Semaphore anlegen (Ali ist Writer!) ---
        shm_unlink(SHM_MANUAL_NAME);          // alte Reste weg (falls vorhanden)
        sem_unlink(SEM_MANUAL_NAME);

        int fd = shm_open(SHM_MANUAL_NAME, O_CREAT | O_RDWR, 0666);
        if (fd < 0) {
            perror("shm_open /manual");
            return 1;
        }
        if (ftruncate(fd, sizeof(ManualControl)) < 0) {
            perror("ftruncate");
            close(fd);
            return 1;
        }

        ManualControl* shm = (ManualControl*)mmap(
            nullptr, sizeof(ManualControl),
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (shm == MAP_FAILED) {
            perror("mmap /manual");
            close(fd);
            return 1;
        }
        close(fd);

        // Initialwerte
        shm->Manual_linear_x = 0.0;
        shm->Manual_angular_z = 0.0;

        sem_t* mutex = sem_open(SEM_MANUAL_NAME, O_CREAT, 0666, 1);
        if (mutex == SEM_FAILED) {
            perror("sem_open /manual_sem");
            munmap(shm, sizeof(ManualControl));
            return 1;
        }

        std::cout << "[Manual] /manual angelegt. Tastatur bereit.\n";

        // --- Alis bestehende Klassen ---
        Tastatur Tast;
        Steuerung Steu;
        Tastenstatus Status;
        Betriebsmodus Modus = Betriebsmodus::Manuel;

        while (!g_stop)
        {
            Tast.Aktualisieren();
            Status = Tast.GetStatus();

            if (Status.q) {
                Steu.Stop();
                break;
            }

            if (Status.m) Modus = Betriebsmodus::Manuel;
            else if (Status.h) Modus = Betriebsmodus::Halbautonom;
            else if (Status.f) Modus = Betriebsmodus::Fullautonom;

            Fahrbefehl Befehl{};
            switch (Modus)
            {
                case Betriebsmodus::Manuel:
                    Steu.Aktualisieren(Status);
                    Befehl = Steu.GetBefehl();
                    break;
                case Betriebsmodus::Halbautonom:
                case Betriebsmodus::Fullautonom:
                    Steu.Stop();
                    Befehl = Steu.GetBefehl();
                    break;
            }

            // Sicherheitscheck (wie bei Ali)
            if (!std::isfinite(Befehl.Fahrgeschwindigkeit) ||
                !std::isfinite(Befehl.Drehgeschwindigkeit) ||
                std::abs(Befehl.Fahrgeschwindigkeit) > 0.22 ||
                std::abs(Befehl.Drehgeschwindigkeit) > 2.84)
            {
                Steu.Stop();
                std::cerr << "[Manual] FEHLER: Ungueltige Fahrwerte. Stopp.\n";
                break;
            }

            // --- In SHM schreiben (unter Semaphore) ---
            sem_wait(mutex);
            shm->Manual_linear_x  = Befehl.Fahrgeschwindigkeit;
            shm->Manual_angular_z = Befehl.Drehgeschwindigkeit;
            sem_post(mutex);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // --- Beim Beenden: Stop-Werte schreiben ---
        sem_wait(mutex);
        shm->Manual_linear_x  = 0.0;
        shm->Manual_angular_z = 0.0;
        sem_post(mutex);

        // --- Cleanup ---
        sem_close(mutex);
        sem_unlink(SEM_MANUAL_NAME);
        munmap(shm, sizeof(ManualControl));
        shm_unlink(SHM_MANUAL_NAME);

        std::cout << "[Manual] Beendet.\n";
        return 0;
    }
    catch (const std::exception& Fehler)
    {
        std::cerr << "[Manual] FEHLER: " << Fehler.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "[Manual] FEHLER: Unbekannter Programmfehler.\n";
        return 1;
    }
}
