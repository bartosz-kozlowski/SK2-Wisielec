#include "Wisielec.h"

int main(int argc, char *argv[]) {
    int port = PORT;
    const int maxPort = 65535;

    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
            if (port <= 0 || port > maxPort) {
                throw std::out_of_range("Port poza zakresem");
            }
        } catch (const std::exception &e) {
            std::cerr << "Błąd: Nieprawidłowy numer portu. Używam domyślnego portu " << PORT << ".\n";
            port = PORT;
        }
    }

    while (port <= maxPort) {
        int result = runMainLoop(port);
        if (result == 0) {
            return 0; // Serwer został uruchomiony pomyślnie
        } else if (result == -2) {
            std::cerr << "Port " << port << " zajęty. Próba na następnym porcie...\n";
            ++port;
        } else {
            std::cerr << "Nieoczekiwany błąd. Kończę działanie.\n";
            return 1; // Inny błąd
        }
    }

    std::cerr << "Nie znaleziono dostępnego portu. Serwer nie może zostać uruchomiony.\n";
    return 1;
}
