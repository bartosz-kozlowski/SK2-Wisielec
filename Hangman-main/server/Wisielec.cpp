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
#include <random>
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
int playersInGame = 0; //liczba graczy w rozgrywce
int efd; //epoll descriptor
chrono::steady_clock::time_point firstCorrectGuessTime;
bool firstGuessMade = false;
int answerDelay = 500; //po jakim czasie moze przyjsc kolejna odpowiedz zeby uznac ze gracze odpowiedzieli w tym samymm czasie
mt19937 rng(random_device{}());
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
    string formattedMessage = to_string(flag) + ";" + message + "&";
    for (const auto &pair : players) {
        if (pair.first != excludeFd) {
            if(players[pair.first].playerStatus != 2  and (flag == 0 or flag == 1 or flag == 2 or flag == 3  or flag == 4 or flag == 6)){
                int n = send(pair.first, formattedMessage.c_str(), formattedMessage.size(), 0);
                if (n == -1 || n < static_cast<int>(formattedMessage.size())) {
                    perror("send");
                    close(pair.first);
                    epoll_ctl(efd, EPOLL_CTL_DEL, pair.first, nullptr);
                    nicknames.erase(players[pair.first].nickname);
                    if (players[pair.first].playerStatus != 2) {
                        activePlayers--;
                    }
                    players.erase(pair.first);
                    //jezeli nie ma juz graczy aktywnych zresetuj ustawienia gry
                    if (activePlayers == 0 or players.size() == 0 or playersInGame == 0) {
                        firstGuessMade = false;
                        countdown = 90*1000;
                        startGame = false;
                    }
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
void resetGame();
// usuwanie gracza - na skutek rozlaczenia sie
void deletePlayer(int clientFd) {
    shutdown(clientFd, SHUT_RDWR);
    close(clientFd);
    epoll_ctl(efd, EPOLL_CTL_DEL, clientFd, nullptr);
    nicknames.erase(players[clientFd].nickname);
    broadcast("Gracz " + players[clientFd].nickname + " rozłączył się.\n", clientFd, 0);
    // jezeli gracz ktory sie rozlaczyl byl w rozgrywce lub na nia oczekiwal zmniejszam liczbe aktywnych (gotowych do rozgrywki) graczy
    if (players[clientFd].playerStatus != 2) {
        activePlayers--;
    }
    if(players[clientFd].playerStatus == 1) {
        playersInGame--;
    }
    players.erase(clientFd);
    //jezeli nie ma juz graczy aktywnych zresetuj ustawienia gry
    if (activePlayers == 0 || playersInGame == 0) {
        countdown = 90 * 1000;
        startGame = false;
        firstGuessMade = false;

        if (activePlayers >= 2) {
            resetGame();
            lastTime = std::chrono::steady_clock::now();
            startGame = true;
        }

        createScoreBoard();
        return;
    }
    createScoreBoard();
}

// Funkcja resetujaca haslo dla gracza - ustawia "-" w miejsce liter hasla
void resetPassword(int fd) {
    players[fd].password = string(pass.size(), '-');
    players[fd].hangman = 0;
    string message = "2;" +topic +"\n"+ players[fd].password + "\n&";
    int n = send(fd, message.c_str(), message.size(), 0);
    if (n == -1 || n < static_cast<int>(message.size())) {
        perror("send");
        deletePlayer(fd);
    }
}

// Funkcja losujaca haslo dla pokoju, ustawia rowniez kategorie dla hasla
void setNewPassword() {
    int num = uniform_int_distribution<>(0, topicsNumber - 1)(rng);
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
        uniform_int_distribution<> dist(0, passwords.size() - 1);
        int passwordIndex = dist(rng);
        pass = passwords[passwordIndex];
        // bonus za odgadniecie hasla na poczatku jest rowne dlugosci hasla
        // wraz z odgadywaniem hasla bonus zmniejsza sie o polowe
        bonus = pass.size();
    } else {
        cerr << "Brak haseł w pliku: " << topic << ".txt" << endl;
    }
}

// zresetowanie ustawien gry - przed rozpoczeciem nowej rundy
// wybieramy nowe haslo oraz kategorie (funkcja setNewPassword)
// zresetowanie czasu gry (countdown = 90 * 1000)
// zresetowanie wynikow graczy - stanu wisielca, ustawienie statusu gracza jako aktywny, usuniecie uzytych liter
void resetGame() {
    //wybieranie nowego hasla i kategorii
    playersInGame = 0;
    setNewPassword();
    lastTime = chrono::steady_clock::now();
    countdown = 90 * 1000;
    firstGuessMade = false;
    for (auto &pair : players) {
        if (!pair.second.nickname.empty() and pair.second.playerStatus != 2) {
            pair.second.hangman = 0;
            pair.second.playerStatus = 1;
            for (int i = 0; i < 26; ++i) {
                pair.second.usedChars[i] = false;
            }
            resetPassword(pair.first);
            string message = "3;" + to_string(players[pair.first].hangman) + "&";
            int n = send(pair.first, message.c_str(), message.size(), 0);
            if (n == -1 || n < static_cast<int>(message.size())) {
                perror("send");
                deletePlayer(pair.first);
            }
            playersInGame++;
        }
    }
    broadcast(to_string(countdown / 1000), -1, 4);
    createScoreBoard();
}
// gracz wrocil do menu
void playerLeft(int clientFd) {
    broadcast("Gracz " + players[clientFd].nickname + " opuścił grę.\n", clientFd, 0);
    if (players[clientFd].playerStatus != 2) {
        activePlayers--;
    }
    //cout<<"playerStatus "<<players[clientFd].playerStatus<<" "<<playersInGame<<"\n";
    if (players[clientFd].playerStatus == 1) {
        playersInGame--;
    }

    players[clientFd].playerStatus = 2; //oznaczenie ze dany gracz znajduje sie w menu
    players[clientFd].time = -1;
    players[clientFd].hangman = 0;
    players[clientFd].score = 0;

    if (activePlayers == 0 or playersInGame == 0) {
        countdown = 90*1000;
        startGame = false;
        firstGuessMade = false;
        string message;
        message = "Witaj w poczekalni, oczekujemy aż dołączy jeszcze jeden gracz!\n";
        broadcast(message, -1, 6);

        // sprawdzamy czy mamy warunki do startu
        if (activePlayers >= 2) {
            resetGame();
            lastTime = chrono::steady_clock::now();
            startGame = true;
        }
        createScoreBoard();
        return;
    }

    createScoreBoard();
}
//funkcja wywolywana gdy uplynal czas
void timeEnded() {
    string message;
    for (const auto &pair : players) {
        //sprawdzamy czy dany gracz odgadnal haslo
        //jezeli gracz nie zdazyl odgadnac hasla przed uplynieciem czasu oznaczamy go jako przegranego
        if (!(pair.second.password == pass) and players[pair.first].playerStatus == 1) {
            int clinetFd = pair.first;
            message = "0; Koniec czasu! Zostałeś powieszony x.x\n&";
            int n = send(clinetFd, message.c_str(), message.size(), 0);
            if (n == -1 || n < static_cast<int>(message.size())) {
                perror("send");
                deletePlayer(clinetFd);
            }
        }
    }
    createScoreBoard();
    //zatrzymanie gry
    //cout << activePlayers << endl;
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
    //cout << "CZY WSZYSCY PRZEGRALI?" << endl;
    for (const auto &pair : players) {
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
    timeEnded();
}


void updatePassword(char ans, int fd) {
    bool guessedCorrectly = false;
    ans = toupper(ans);
    int n = 0;

    // Sprawdzenie, czy litera była już użyta
    if (players[fd].usedChars[ans - 'A']) {
        string message = "0; sprawdzałeś/aś już tę literę!\n&";
        n = send(fd, message.c_str(), message.size(), 0);
        if (n == -1 || n < static_cast<int>(message.size())) {
            perror("send");
            deletePlayer(fd);
        }
        return;
    }

    // Oznaczenie litery jako użytej
    players[fd].usedChars[ans - 'A'] = true;

    // Sprawdzenie poprawności zgadywanej litery
    for (size_t i = 0; i < pass.size(); i++) {
        if (ans == pass[i]) {
            players[fd].password[i] = ans;
            players[fd].score++;
            guessedCorrectly = true;
        }
    }

    createScoreBoard();

    // Obsługa błędnej litery
    if (!guessedCorrectly) {
        players[fd].hangman++;
        string message = "3;" + to_string(players[fd].hangman) + "&";
        n = send(fd, message.c_str(), message.size(), 0);
        if (n == -1 || n < static_cast<int>(message.size())) {
            perror("send updatePassword:");
            deletePlayer(fd);
        }
        createScoreBoard();

        if (players[fd].hangman == 7) {
            message = "0; Zostałeś powieszony x.x\n&";
            n = send(fd, message.c_str(), message.size(), 0);
            if (n == -1 || n < static_cast<int>(message.size())) {
                perror("send updatePassword:");
                deletePlayer(fd);
            }
            broadcast("Gracz " + players[fd].nickname + " został powieszony!\n", fd, 0);
            createScoreBoard();
            checkLosers();
            return;
        }
    }

    // Sprawdzenie, czy gracz odgadł całe hasło
    if (players[fd].password == pass) {
        auto currentTime = chrono::steady_clock::now();
        string message = "0;Odgadłeś hasło, gratulacje :D\n&";
        int n = send(fd, message.c_str(), message.size(), 0);
        if (n == -1 || n < static_cast<int>(message.size())) {
            perror("send updatePassword:");
            deletePlayer(fd);
        }
        if (!firstGuessMade) {
            firstCorrectGuessTime = currentTime;
            firstGuessMade = true;
            players[fd].score += bonus;
        } else {
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(currentTime - firstCorrectGuessTime).count();
            //cout<<"elapsed "<<elapsed<<" ms "<<answerDelay<<"ms \n";
            if (elapsed <= answerDelay) {
                players[fd].score += bonus;
            } else {
                // Zmniejszenie bonusu dla kolejnych graczy
                bonus = bonus / 2;
                players[fd].score += bonus;
                firstCorrectGuessTime = currentTime;
            }
        }

        // Aktualizacja rankingu
        createScoreBoard();
        broadcast("Gracz " + players[fd].nickname + " odgadł hasło, gratulacje!\n", fd, 0);

        // Skrócenie czasu rundy, jeśli hasło zostało odgadnięte
        if (countdown > 20 * 1000) {
            countdown = 20 * 1000;
        }

        checkLosers();
    }

    // Rozesłanie statusu hasła do gracza
    string message = "2;" + topic + "\n" + players[fd].password + "\n&";
    n = send(fd, message.c_str(), message.size(), 0);
    if (n == -1 || n < static_cast<int>(message.size())) {
        perror("send updatePassword:");
        deletePlayer(fd);
    }
}


// Obsługa wejścia klienta
void handleClientInput(int clientFd, const string &input) {
    //obsluga wpisania nicku przez klienta
    if (players[clientFd].nickname.empty()) {
        if (nicknames.find(input) != nicknames.end()) {
            const char *notAvailable = "01;Nick zajęty. Wybierz inny: \n&";
            int n = send(clientFd, notAvailable, strlen(notAvailable), 0);
            if (n == -1 || n < static_cast<int>(strlen(notAvailable))) {
                perror("send handleClientInput: ");
                deletePlayer(clientFd);
            }
        } else {
            players[clientFd].nickname = input;
            nicknames.insert(input);

            const char *acceptedNick = "01;Nick zaakceptowany! Miłej gry.\n&";
            int n = send(clientFd, acceptedNick, strlen(acceptedNick), 0);
            if (n == -1 || n < static_cast<int>(strlen(acceptedNick))) {
                perror("send handleClientInput: ");
                deletePlayer(clientFd);
            }
            //przeniesienie gracza do menu
            players[clientFd].playerStatus = 2;
            //odsylam do klienta jego wasny nick zatwierdzony przez serwer
            string message = "02;"+input+"&";
            n = send(clientFd, message.c_str(), message.size(), 0);
            if (n == -1 || n < static_cast<int>(message.size())) {
                perror("send handleClientInput: ");
                deletePlayer(clientFd);
            }
        }
    } else {
        //klient dolaczyl do pokoju
        if (input == "USER JOIN") {
            players[clientFd].time =  time(nullptr);
            string message;
            //zmieniam status gracza na obecny w pokoju
            players[clientFd].playerStatus = 0;
            //zwiekszam liczbe graczy gotowych do rozpoczecia
            broadcast("Gracz " + players[clientFd].nickname + " dołączył do gry.\n", clientFd, 0);
            activePlayers ++;
            if (!startGame and activePlayers >=2) {
                resetGame();
                lastTime = chrono::steady_clock::now();
                startGame = true;
            }else if (startGame) {
                message = "6;Witaj aktualnie trwa gra, poczekaj aż się zakończy.\n&";
            }else {
                message = "6;Witaj w poczekalni, oczekujemy na rozpoczęcie gry.\n&";
            }
            int n = send(clientFd, message.c_str(), message.size(), 0);
            if (n == -1 || n < static_cast<int>(message.size())) {
                perror("send handleClientInput: ");
                deletePlayer(clientFd);
            }
            createScoreBoard();
        }else if (input == "USER LEFT") {
            playerLeft(clientFd);
        }else {
            //oblsuzenie wyslanego przez gracza znaku
            if (startGame and players[clientFd].playerStatus == 1) {
                updatePassword(input[0], clientFd);
            }else {
                if (!startGame) {
                    string message = "0;Gra nierozpoczęta.\n&";
                    int n = send(clientFd, message.c_str(), message.size(), 0);
                    if (n == -1 || n < static_cast<int>(message.size())) {
                        perror("send handleClientInput: ");
                        deletePlayer(clientFd);
                    }
                }else if (!players[clientFd].playerStatus){
                    string message = "0;Poczekaj na zakończenie aktualnej rozgrywki.\n&";
                    int n = send(clientFd, message.c_str(), message.size(), 0);
                    if (n == -1 || n < static_cast<int>(message.size())) {
                        perror("send handleClientInput: ");
                        deletePlayer(clientFd);
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
        shutdown(serverFd, SHUT_RDWR);
        close(serverFd);
        return 1;
    }

    if (listen(serverFd, SOMAXCONN) == -1) {
        perror("listen");
        shutdown(serverFd, SHUT_RDWR);
        close(serverFd);
        return 1;
    }

    int efd = epoll_create1(0);
    if (efd == -1) {
        perror("epoll_create1");
        shutdown(serverFd, SHUT_RDWR);
        close(serverFd);
        return 1;
    }

    epoll_event event{}, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = serverFd;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, serverFd, &event) == -1) {
        perror("epoll_ctl");
        shutdown(serverFd, SHUT_RDWR);
        close(serverFd);
        close(efd);
        return 1;
    }

    cout << ":) Serwer działa na porcie " << PORT << endl;


    while (true) {
        if (startGame) {
            auto currentTime = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(currentTime - lastTime).count();
            if (elapsed >= 1000) { // Aktualizacja co sekundę
                int secondsToSubtract = elapsed / 1000;
                countdown = max(0, countdown - secondsToSubtract * 1000);
                lastTime += chrono::milliseconds(secondsToSubtract * 1000);
                if (countdown <= 0) {
                    timeEnded();
                } else {
                    broadcast(to_string(countdown / 1000), -1, 4);
                }
            }
        }

        int numEvents = epoll_wait(efd, events, MAX_EVENTS, 1000);
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
                event.events = EPOLLIN;
                event.data.fd = clientFd;

                if (epoll_ctl(efd, EPOLL_CTL_ADD, clientFd, &event) == -1) {
                    perror("epoll_ctl: client");
                    shutdown(clientFd, SHUT_RDWR);
                    close(clientFd);
                    continue;
                }

                players[clientFd] = Player{};
                const char *welcome = "01;Witaj! Wprowadź swój nick!: &";
                int n = send(clientFd, welcome, strlen(welcome), 0);
                if (n == -1 || n < static_cast<int>(strlen(welcome))) {
                    perror("send handleClientInput: ");
                    deletePlayer(clientFd);
                }
                createScoreBoard();
            } else {
                int clientFd = events[i].data.fd;
                char buffer[BUFFER_SIZE];
                memset(buffer, 0, BUFFER_SIZE);
                int bytesRead = recv(clientFd, buffer, BUFFER_SIZE - 1,0);
                if (bytesRead < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("recv error");
                        deletePlayer(clientFd);
                    }
                    continue;
                }
                if (bytesRead == 0) {
                    deletePlayer(clientFd);
                    createScoreBoard();
                } else {
                    static unordered_map<int, string> messageBuffers;

                    messageBuffers[clientFd] += string(buffer, bytesRead);

                    size_t semicolonPos;
                    while ((semicolonPos = messageBuffers[clientFd].find(';')) != string::npos) {
                        string message = messageBuffers[clientFd].substr(0, semicolonPos);
                        //cout<<message<<endl;
                        messageBuffers[clientFd].erase(0, semicolonPos + 1);

                        handleClientInput(clientFd, message);
                    }
                }
            }
        }
    }

    shutdown(serverFd, SHUT_RDWR);
    close(serverFd);
    close(efd);
    return 1;
}
int main(){
    runMainLoop();
}
