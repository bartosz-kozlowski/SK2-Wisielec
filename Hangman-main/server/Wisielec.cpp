#include <algorithm>
#include <chrono>
#include <iostream>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <fstream>
#include <math.h>
#define MAX_EVENTS 10
#define BUFFER_SIZE 1024
#define PORT 1234

using namespace std;
const int topicsNumber=3; //liczba tematow do wyboru
string topics[topicsNumber] = {"PKP", "CELEBRYCI", "SWIETA"}; //tematy
string topic = ""; //temat hasla
string pass = ""; //haslo
int bonus=0; //ilosc punktow mozliwych do zdobycia
int countdown = 90 * 1000; //czas ktory pozostal do konca rundy - w milisekundach
bool startGame = false; //czy trwa aktualnie jakas gra
auto lastTime = chrono::steady_clock::now(); //ustawienie pomiaru czasu
int activePlayers = 0; //liczba aktywnych graczy - w pokoju
// Struktura gracza
struct Player {
    string nickname;
    int score = 0; //wynik gracza - liczony od momentu wejscia do pokoju
    int hangman = 0; //status wisielca gdzie 0 - pierwszy poziom wisielca 7- ostatni poziom wisielca = przegrana gracza
    int playerStatus = 2; //status gracza 0 - oznacza ze gracz jest w pokoju ale nie bierze udzialu w rozgrywce(dolaczyl po jej rozpoczeciu) 1- oznacza ze gracz jest w pokoju i bierze udzial w rozgrywce 2 - oznacza ze gracz znajduje sie w menu
    string password = ""; //status zgadnietego hasla
    time_t time = -1; // Czas spędzony w pokoju (domyślnie 0 sekund)
    bool usedChars[26]={false}; //uzyte przez gracza znaki w danej rundzie
};

unordered_map<int, Player> players; // Mapa deskryptorów do graczy
unordered_set<string> nicknames;   // Zbiór zajętych nicków

//ustawienie gwiazda w tryb nieblokujacy
void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
    }
}

// Funkcja do rozsyłania wiadomości do wszystkich klientów, mozna wylaczyc wskazanego gracza,wywolujac funkcje z excludeFd = -1 mozna rozeslac wiadomosc do wszystkich podlaczonych graczy
// Flaga pozwala na ustawienie typu wiadomosci
// Flaga 0 i 01 - komunikaty wysylane do klientow 1 - przeslanie rankingu 2 - przeslanie hasla
// 3 - przeslanie stanu wisielca 4 - przeslanie czasu 6 - przeniesienie uzytkownika do poczekalni
// 02 - odeslanie do klienta nicku zaakceptowanego przez serwer
void broadcast(const string &message, int excludeFd, int flag) {
    //wiadomosc jest w formacie flag;message;
    string formattedMessage = to_string(flag) + ";" + message + ";";
    for (const auto &pair : players) {
        if (pair.first != excludeFd) {
            int n = send(pair.first, formattedMessage.c_str(), formattedMessage.size(), 0);
            if (n==-1) {
                perror("send");
            }
        }
    }
}
// Funkcja do tworzenia tablicy wyników i wysyłania jej do graczy
// Wysylamy informacje o nicku, liczbie zdobytych punktow,
// statusie wisielca (na podstawie tej liczby wyswietlamy odpowiednia grafike)
// oraz status gracza w rozgrywce
// Tablica wynikow zawiera tylko i wylacznie graczy ktorzy dolaczyli do pkoju (playerStatus = 1 lub playerStatus = 2_
void createScoreBoard() {
    string scoreBoard;
    for (const auto &pair : players) {
        if (!pair.second.nickname.empty() and pair.second.playerStatus != 2) {
            scoreBoard += pair.second.nickname + ":" + to_string(pair.second.score) + ":" +
            to_string(pair.second.hangman) + ":"+
                to_string(pair.second.playerStatus)+"\n";
        }
    }
    if (!scoreBoard.empty()) {
        broadcast(scoreBoard, -1, 1);
    }
}

// Funkcja resetujaca haslo dla gracza - ustawia "-" w miejsce liter hasla
void resetPassword(int fd) {
    players[fd].password = string(pass.size(), '-');
    players[fd].hangman = 0;
    string message = "2;" +topic +"\n"+ players[fd].password + "\n;";
    send(fd, message.c_str(), message.size(), 0);
}
// Funkcja losujaca haslo dla pokoju, ustawia rowniez kategorie dla hasla
void setNewPassword() {
    srand(time(nullptr));

    int num = rand() % topicsNumber;
    topic = topics[num];

    ifstream file("topics/" + topic + ".txt");
    if (!file) {
        cerr << "Nie można otworzyć pliku: " << topic << ".txt" << endl;
        return;
    }

    vector<string> passwords;
    string line;
    while (getline(file, line)) {
        passwords.push_back(line);
    }
    file.close();

    if (!passwords.empty()) {
        int passwordIndex = rand() % passwords.size();
        pass = passwords[passwordIndex];
        // bonus za odgadniecie hasla na poczatku jest rowne dlugosci hasla
        // wraz z odgadywaniem hasla bonus zmniejsza sie o polowe
        bonus = pass.size();
    } else {
        cerr << "Brak haseł w pliku: " << topic << ".txt" << endl;
        pass = "";
    }
}

// zresetowanie ustawien gry - przed rozpoczeciem nowej rundy
// wybieramy nowe haslo oraz kategorie (funkcja setNewPassword)
// zresetowanie czasu gry (countdown = 90 * 1000)
// zresetowanie wynikow graczy - stanu wisielca, ustawienie statusu gracza jako aktywny, usuniecie uzytych liter
void resetGame() {
    //wybieranie nowego hasla i kategorii
    setNewPassword();
    countdown = 90 * 1000;
    for (auto &pair : players) {
        if (!pair.second.nickname.empty() and pair.second.playerStatus != 2) {
            pair.second.hangman = 0;
            pair.second.playerStatus = 1;
            for (int i = 0; i < 26; ++i) {
                pair.second.usedChars[i] = false;
            }
            resetPassword(pair.first);
            string message = "3;" + to_string(players[pair.first].hangman) + ";";
            send(pair.first, message.c_str(), message.size(), 0);
        }
    }
    createScoreBoard();
}
// usuwanie gracza - na skutek rozlaczenia sie
void deletePlayer(int epollFd, int clientFd) {
    close(clientFd);
    epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
    nicknames.erase(players[clientFd].nickname);
    broadcast("Gracz " + players[clientFd].nickname + " rozłączył się.\n", clientFd, 0);
    // jezeli gracz ktory sie rozlaczyl byl w rozgrywce lub na nia oczekiwal zmniejszam liczbe aktywnych (gotowych do rozgrywki) graczy
    if (players[clientFd].playerStatus != 2) {
        activePlayers--;
    }
    players.erase(clientFd);
    //jezeli nie ma juz graczy aktywnych zresetuj ustawienia gry
    if (activePlayers == 0 or players.size() == 0) {
        countdown = 90*1000;
        startGame = false;
        return;
    }
    createScoreBoard();

}
// gracz wrocil do menu
void playerLeft(int clientFd) {
    broadcast("Gracz " + players[clientFd].nickname + " opuścił grę.\n", clientFd, 0);
    if (players[clientFd].playerStatus != 2) {
        activePlayers--;
    }
    //jezeli w pokoju nie ma aktywnych graczy to przywroc ustawienia poczatkowe
    if (activePlayers == 0) {
        countdown = 90*1000;
        startGame = false;
    }
    players[clientFd].playerStatus = 2; //oznaczenie ze dany gracz znajduje sie w menu
    players[clientFd].time = -1;
    players[clientFd].hangman = 0;
    players[clientFd].score = 0;

    createScoreBoard();
}
//funkcja wywolywana gdy uplynal czas
void timeEnded() {
    string message;
    for (const auto &pair : players) {
        //sprawdzamy czy dany gracz odgadnal haslo
        //jezeli gracz nie zdazyl odgadnac hasla przed uplynieciem czasu oznaczamy go jako przegranego
        if (!(pair.second.password == pass)) {
            int clinetFd = pair.first;
            message = "0; Koniec czasu! Zostałeś powieszony x.x\n;";
            send(clinetFd, message.c_str(), message.size(), 0);
            players[clinetFd].hangman = 7;
            message = "3;" + to_string(players[clinetFd].hangman) + ";";
            send(clinetFd, message.c_str(), message.size(), 0);
            //wyslanie do klienta poprawnego hasla
            message = "2;"+topic+"\n" + pass + "\n;";
            send(clinetFd, message.c_str(), message.size(), 0);

        }
    }
    createScoreBoard();
    //zatrzymanie gry
    cout << activePlayers << endl;
    if (activePlayers >= 2){
        resetGame();
        lastTime = chrono::steady_clock::now();
        startGame = true;
        }else {
            startGame = false;
            //przekierowanie graczy do poczekalni
            message = "Witaj w poczekalni, oczekujemy aż dołączy jeszcze jeden gracz!\n";
            broadcast(message, -1, 6);
        }
}
//Funkcja sprawdzajaca czy wszyscy gracze przegrali/wygrali
void checkLosers() {
    cout << "CZY WSZYSCY PRZEGRALI?" << endl;
    for (const auto &pair : players) {
        /*cout << "GRACZ: " << pair.second.playerStatus << " " << pair.second.nickname
             << ", Wisielec: " << pair.second.hangman
             << ", Hasło zgadnięte: "<<" "<<pair.second.password<<" "
             << (pair.second.password == pass)<<" "<<pair.second.password.size()
        <<" "<<pass.size() << endl;
        */
        //sprawdzenie czy dany gracz bierze udzial w rundzie
        if (pair.second.playerStatus == 1) {
            bool check = (pair.second.password == pass); //zmienna inforomujaca ze gracz odgadl haslo
            if (pair.second.hangman < 7 && !check) {
                // Ktoś jeszcze nie został powieszony i ma nieodgadniete hasło
                //cout << "Gra trwa, przynajmniej jeden gracz nie przegrał." << endl;
                return;
            }
        }
    }
   // cout << "Wszyscy gracze przegrali. Resetowanie gry." << endl;
    timeEnded();
   // startGame = false;
}


// Funkcja aktualizująca hasło na podstawie zgadniętej litery
void updatePassword(char ans, int fd) {
    bool guessedCorrectly = false;
    ans=toupper(ans);
    //sprawdzamy czy gracz zgadywal juz dana litere
    if (players[fd].usedChars[ans-'A']) {
        string message = "0; sprawdzałeś/aś już tę literę!\n;";
        send(fd, message.c_str(), message.size(), 0);
        return;
    }
    //oznaczamy litere jako uzyta
    players[fd].usedChars[ans-'A'] = 1;
    for (size_t i = 0; i < pass.size(); i++) {
        if (ans == pass[i]) {
            players[fd].password[i] = ans;
            players[fd].score++;
            guessedCorrectly = true;
            createScoreBoard();
        }
    }
    //jezeli gracz nie zgadl dodajemy pietro wisielca
    if (!guessedCorrectly) {
        players[fd].hangman++;
        string message = "3;" + to_string(players[fd].hangman) + ";";
        int n = send(fd, message.c_str(), message.size(), 0);
        if (n==-1) {
            perror("send updatePassword:");
        }
        createScoreBoard();
        if (players[fd].hangman == 7) {
            message = "0; Zostałeś powieszony x.x\n;";
            send(fd, message.c_str(), message.size(), 0);
            if (n==-1) {
                perror("send updatePassword:");
            }
            broadcast("Gracz " + players[fd].nickname + " został powieszony!\n", fd, 0);
            createScoreBoard();
            //sprawdzamy czy sa gracze ktorzy nadal odgaduja haslo
            checkLosers();
            return;
        }
    }
    //sprawdzamy czy gracz odgadl cale haslo
    if (players[fd].password == pass) {
        string message = "0;Odgadłeś hasło, gratulacje :D\n;";
        int n = send(fd, message.c_str(), message.size(), 0);
        if (n==-1) {
            perror("send updatePassword:");
        }
        //za odgadniecie hasla gracz dostaje bonusowe punkty
        players[fd].score += bonus;
        //aktualziacja rankingu
        createScoreBoard();
        broadcast("Gracz " + players[fd].nickname + " odgadł hasło, gratulacje!\n", fd, 0);
        //jezeli czas pozostaly jest wiekszy niz 20 sekund to go zmniejszamy
        if (countdown > 20 * 1000) {
            countdown = 20 * 1000;
        }
        //sprawdzamy czy sa jeszcze gracze ktorzy odgaduja haslo
        checkLosers();
        //po odgadnieciu hasla przez gracza bonus zostaje zmniejszony
        bonus = bonus/2;
    }
    //rozeslanie statusu hasla do gracza
    string message = "2;"+topic+"\n" + players[fd].password + "\n;";
    send(fd, message.c_str(), message.size(), 0);
}

// Obsługa wejścia klienta
void handleClientInput(int clientFd, const string &input) {
    //obsluga wpisania nicku przez klienta
    if (players[clientFd].nickname.empty()) {
        if (nicknames.find(input) != nicknames.end()) {
            const char *notAvailable = "01;Nick zajęty. Wybierz inny: \n;";
            int n = send(clientFd, notAvailable, strlen(notAvailable), 0);
            if (n== -1) {
                perror("send handleClientInput: ");
            }
        } else {
            players[clientFd].nickname = input;
            nicknames.insert(input);

            const char *acceptedNick = "01;Nick zaakceptowany! Miłej gry.\n;";
            int n = send(clientFd, acceptedNick, strlen(acceptedNick), 0);
            if (n== -1) {
                perror("send handleClientInput: ");
            }
            //przeniesienie gracza do menu
            players[clientFd].playerStatus = 2;
            //odsylam do klienta jego wasny nick zatwierdzony przez serwer
            string message = "02;"+input+";";
            n = send(clientFd, message.c_str(), message.size(), 0);
            if (n== -1) {
                perror("send handleClientInput: ");
            }
        }
    } else {
        //klient dolaczyl do pokoju
        if (input == "USER JOIN;") {
            players[clientFd].time =  time(nullptr);
            string message;
            //zmieniam status gracza na obecny w pokoju
            players[clientFd].playerStatus = 0;
            //zwiekszam liczbe graczy gotowych do rozpoczecia
            activePlayers ++;
            broadcast("Gracz " + players[clientFd].nickname + " dołączył do gry.\n", clientFd, 0);
            if (!startGame and activePlayers >=2) {
                resetGame();
                lastTime = chrono::steady_clock::now();
                startGame = true;
            }else if (startGame) {
                message = "6;Witaj aktualnie trwa gra, poczekaj aż się zakończy.\n;";
            }else {
                message = "6;Witaj w poczekalni, oczekujemy na rozpoczęcie gry.\n;";
            }
            int n = send(clientFd, message.c_str(), message.size(), 0);
            if (n==-1) {
                perror("send handleClientInput: ");
            }
            createScoreBoard();
        }else if (input == "USER LEFT;") {
            playerLeft(clientFd);
        }else {
            //oblsuzenie wyslanego przez gracza znaku
            if (startGame and players[clientFd].playerStatus == 1) {
                updatePassword(input[0], clientFd);
            }else {
                if (!startGame) {
                    string message = "0;Gra nierozpoczęta.\n;";
                    int n = send(clientFd, message.c_str(), message.size(), 0);
                    if (n==-1) {
                        perror("send handleClientInput: ");
                    }
                }else if (!players[clientFd].playerStatus){
                    string message = "0;Poczekaj na zakończenie aktualnej rozgrywki.\n;";
                    int n = send(clientFd, message.c_str(), message.size(), 0);
                    if (n==-1) {
                        perror("send handleClientInput: ");
                    }
                }
            }
        }
    }
}

//glowna petla serwera
int runMainLoop() {
    //utworzenie gniazda TCP
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1) {
        perror("socket");
        return 1;
    }

    setNonBlocking(serverFd);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; //gniazdo nasluchuje na
    serverAddr.sin_port = htons(PORT);

    const int one = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    if (bind(serverFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("bind");
        close(serverFd);
        return 1;
    }

    if (listen(serverFd, SOMAXCONN) == -1) {
        perror("listen");
        close(serverFd);
        return 1;
    }

    int epollFd = epoll_create1(0);
    if (epollFd == -1) {
        perror("epoll_create1");
        close(serverFd);
        return 1;
    }

    epoll_event event{}, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = serverFd;

    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &event) == -1) {
        perror("epoll_ctl");
        close(serverFd);
        close(epollFd);
        return 1;
    }

    cout << "Serwer działa na porcie " << PORT << endl;


    while (true) {
        if (startGame) {
            auto currentTime = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(currentTime - lastTime).count();
            if (elapsed >= 1000) { // Aktualizacja co sekundę
                countdown -= elapsed;
                lastTime = currentTime;
                if (countdown <= 0) {
                    timeEnded();
                } else {
                    int conv = round(countdown / 1000);
                    broadcast(to_string(conv), -1, 4);
                }
            }
        }

        int numEvents = epoll_wait(epollFd, events, MAX_EVENTS, 1000); // Wait 1 mili second
        if (numEvents == -1) {
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < numEvents; ++i) {
            if (events[i].data.fd == serverFd) {
                sockaddr_in clientAddr;
                socklen_t clientLen = sizeof(clientAddr);
                int clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &clientLen);
                if (clientFd == -1) {
                    perror("accept");
                    continue;
                }

                setNonBlocking(clientFd);
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = clientFd;

                if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &event) == -1) {
                    perror("epoll_ctl: client");
                    close(clientFd);
                    continue;
                }

                players[clientFd] = Player{};
                const char *welcome = "01;Witaj! Wprowadź swój nick!: ;";
                send(clientFd, welcome, strlen(welcome), 0);
                createScoreBoard();
            } else {
                int clientFd = events[i].data.fd;
                char buffer[BUFFER_SIZE];
                memset(buffer, 0, BUFFER_SIZE);
                int bytesRead = recv(clientFd, buffer, BUFFER_SIZE - 1,0);
                if (bytesRead < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("recv error");
                        deletePlayer(epollFd,clientFd);
                    }
                    continue;
                }
                if (bytesRead == 0) {
                    deletePlayer(epollFd, clientFd);
                    createScoreBoard();
                } else {
                    string input(buffer, bytesRead - 1);
                    handleClientInput(clientFd, input);
                }
            }
        }
    }

    close(serverFd);
    close(epollFd);
    return 1;
}
