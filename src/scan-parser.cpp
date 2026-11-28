
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <cmath>

#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;


// --------------------------------------------------
// Einstellungen
// --------------------------------------------------

constexpr int PORT = 9997;
constexpr int BUFFER_SIZE = 1024;
constexpr int MAX_MEASUREMENTS = 360;

const std::string IP = "192.168.100.50";

const std::string START_MARKER = "---START---";
const std::string END_MARKER = "___END___";

const char* SHM_NAME = "/scan";
const char* SEM_NAME = "/scan_sem";


// --------------------------------------------------
// Scan-Datenstruktur
// --------------------------------------------------

struct ScanData
{
    uint32_t seq = 0;

    double angle_min = 0.0;
    double angle_max = 0.0;
    double angle_increment = 0.0;

    double range_min = 0.0;
    double range_max = 0.0;

    double ranges[MAX_MEASUREMENTS] = {};
    double intensities[MAX_MEASUREMENTS] = {};

    int count = 0;

    char frame_id[32] = {};
};


// --------------------------------------------------
// Vollständige Nachrichten aus TCP-Puffer extrahieren
// --------------------------------------------------

void extractMessages(
    std::string& buffer,
    std::vector<std::string>& messages)
{
    while (true)
    {
        size_t start = buffer.find(START_MARKER);

        if (start == std::string::npos)
        {
            if (buffer.size() >= START_MARKER.size())
            {
                buffer.erase(
                    0,
                    buffer.size() - START_MARKER.size() + 1
                );
            }

            return;
        }

        if (start > 0)
        {
            buffer.erase(0, start);
        }

        size_t end = buffer.find(
            END_MARKER,
            START_MARKER.size()
        );

        if (end == std::string::npos)
        {
            return;
        }

        size_t json_start = START_MARKER.size();

        std::string message = buffer.substr(
            json_start,
            end - json_start
        );

        messages.push_back(message);

        buffer.erase(
            0,
            end + END_MARKER.size()
        );
    }
}


// --------------------------------------------------
// JSON-Daten in ScanData umwandeln
// --------------------------------------------------

bool parseScan(
    const std::string& message,
    ScanData& scan)
{
    try
    {
        json data = json::parse(message);

        scan.seq = data["header"]["seq"].get<uint32_t>();

        scan.angle_min =
            data["angle_min"].get<double>();

        scan.angle_max =
            data["angle_max"].get<double>();

        scan.angle_increment =
            data["angle_increment"].get<double>();

        scan.range_min =
            data["range_min"].get<double>();

        scan.range_max =
            data["range_max"].get<double>();

        std::string frame =
            data["header"]["frame_id"].get<std::string>();

        std::strncpy(
            scan.frame_id,
            frame.c_str(),
            sizeof(scan.frame_id) - 1
        );

        const auto& ranges = data["ranges"];
        const auto& intensities = data["intensities"];

        if (!ranges.is_array() || !intensities.is_array())
        {
            std::cerr << "Fehler: ranges oder intensities ist kein Array.\n";
            return false;
        }

        if (ranges.size() != intensities.size())
        {
            std::cerr << "Fehler: ranges und intensities haben unterschiedliche Groessen.\n";
            return false;
        }

        if (ranges.size() > MAX_MEASUREMENTS)
        {
            std::cerr << "Fehler: Zu viele Messwerte fuer ScanData.\n";
            return false;
        }

        scan.count = 0;

        for (size_t i = 0; i < ranges.size(); ++i)
        {
            double range = ranges[i].get<double>();
            double intensity = intensities[i].get<double>();

            scan.ranges[i] = range;
            scan.intensities[i] = intensity;

            scan.count++;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "JSON-Fehler: " << e.what() << "\n";
        return false;
    }
}


// --------------------------------------------------
// Hauptprogramm
// --------------------------------------------------

int main()
{
    // TCP-Verbindung herstellen

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        perror("Socket konnte nicht erstellt werden");
        return 1;
    }

    sockaddr_in server{};

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (inet_pton(AF_INET, IP.c_str(), &server.sin_addr) <= 0)
    {
        std::cerr << "Ungueltige IP-Adresse.\n";
        close(sock);
        return 1;
    }

    std::cout << "Verbinde mit " << IP << ":" << PORT << " ...\n";

    if (connect(
        sock,
        reinterpret_cast<sockaddr*>(&server),
        sizeof(server)) < 0)
    {
        perror("TCP-Verbindung fehlgeschlagen");
        close(sock);
        return 1;
    }

    std::cout << "TCP-Verbindung erfolgreich!\n";


    // Shared Memory erstellen

    int shm_fd = shm_open(
        SHM_NAME,
        O_CREAT | O_RDWR,
        0666
    );

    if (shm_fd < 0)
    {
        perror("Shared Memory konnte nicht erstellt werden");
        close(sock);
        return 1;
    }

    if (ftruncate(shm_fd, sizeof(ScanData)) < 0)
    {
        perror("ftruncate fehlgeschlagen");
        close(shm_fd);
        close(sock);
        return 1;
    }

    ScanData* current_scan = static_cast<ScanData*>(
        mmap(
            nullptr,
            sizeof(ScanData),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            shm_fd,
            0
        )
    );

    if (current_scan == MAP_FAILED)
    {
        perror("Shared Memory konnte nicht eingebunden werden");
        close(sock);
        close(shm_fd);
        return 1;
    }

    *current_scan = ScanData{};


    // Semaphore erstellen

    sem_t* semaphore = sem_open(
        SEM_NAME,
        O_CREAT,
        0666,
        1
    );

    if (semaphore == SEM_FAILED)
    {
        perror("Semaphore konnte nicht erstellt werden");
        munmap(current_scan, sizeof(ScanData));
        close(shm_fd);
        close(sock);
        return 1;
    }


    // Empfang vorbereiten

    char buffer[BUFFER_SIZE];

    std::string tcp_buffer;

    std::vector<std::string> messages;

    uint64_t scan_counter = 0;

    uint32_t last_seq = 0;
    bool first_scan = true;


    std::cout << "\nWarte auf LiDAR-Daten ...\n\n";


    // Endlosschleife: TCP-Daten empfangen

    while (true)
    {
        ssize_t bytes_received = read(
            sock,
            buffer,
            BUFFER_SIZE
        );

        if (bytes_received < 0)
        {
            if (errno == EINTR)
                continue;

            perror("Fehler beim Lesen der TCP-Daten");
            break;
        }

        if (bytes_received == 0)
        {
            std::cout << "TCP-Verbindung wurde beendet.\n";
            break;
        }

        // Empfangene Bytes an den Puffer anhängen
        tcp_buffer.append(buffer, bytes_received);


        // Vollständige Nachrichten extrahieren
        extractMessages(tcp_buffer, messages);


        // Alle vollständigen Nachrichten verarbeiten
        for (const std::string& message : messages)
        {
            ScanData new_scan{};

            if (!parseScan(message, new_scan))
            {
                std::cerr << "Scan konnte nicht verarbeitet werden.\n";
                continue;
            }


            // Doppelte Scans erkennen und lautlos ignorieren

            if (!first_scan && new_scan.seq == last_seq)
            {
                continue;
            }

            first_scan = false;
            last_seq = new_scan.seq;

            scan_counter++;


            // Scan in Shared Memory schreiben

            while (sem_wait(semaphore) < 0)
            {
                if (errno == EINTR)
                    continue;

                perror("sem_wait fehlgeschlagen");
                break;
            }

            *current_scan = new_scan;

            sem_post(semaphore);


            // Ausgabe

            std::cout << "\n====================================\n";

            /*std::cout << "Scan erfolgreich verarbeitet: "
                      //<< scan_counter << "\n";

            std::cout << "Sequence number: "
                      << new_scan.seq << "\n";

            std::cout << "Frame ID: "
                      << new_scan.frame_id << "\n";

            std::cout << "Number of measurements: "
                      << new_scan.count << "\n";

            std::cout << "Angle min: "
                      << new_scan.angle_min << "\n";

            std::cout << "Angle max: "
                      << new_scan.angle_max << "\n";

            std::cout << "Angle increment: "
                      << new_scan.angle_increment << "\n";

            std::cout << "Range min: "
                      << new_scan.range_min << "\n";

            std::cout << "Range max: "
                      << new_scan.range_max << "\n"; */


            // ALLE Messwerte ausgeben

            std::cout << "\nAlle Messwerte:\n";

            for (int i = 0; i < new_scan.count; ++i)
            {
                double angle =
                    new_scan.angle_min +
                    i * new_scan.angle_increment;

                double angle_deg = angle * 180.0 / M_PI;


                std::cout
                    //<< "Measurement " << i
                    << " Winkel: " << angle_deg << "°"
                    << " | Entfernung: " << new_scan.ranges[i] << " m"
                    << " | Intensitaet: " << new_scan.intensities[i]
                    << "\n";
            }
        }    

        // Verarbeitete Nachrichten entfernen
        messages.clear();
    }


    // Ressourcen freigeben

    sem_close(semaphore);

    munmap(current_scan, sizeof(ScanData));

    close(shm_fd);

    close(sock);

    std::cout << "Scan Parser beendet.\n";

    return 0;
}