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

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024
#define PORT 1234

using namespace std;

// Struktura gracza
struct Player {
    string nickname;
    int score = 0;
    bool isInGame = false;
};

unordered_map<int, Player> players; // Mapa deskryptorów do graczy
unordered_set<string> nicknames; // Zbiór zajętych nicków

void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void broadcast(const string &message, int excludeFd = -1) {
    for (const auto &pair : players) {
        if (pair.first != excludeFd) {
            send(pair.first, message.c_str(), message.size(), 0);
        }
    }
}

void handleClientInput(int clientFd, const string &input) {
    if (players[clientFd].nickname.empty()) { // Oczekiwanie na nick
        if (nicknames.find(input) != nicknames.end()) {
            const char * notAvailable =  "Nick zajęty. Wybierz inny: ";
            send(clientFd, notAvailable, strlen(notAvailable), 0);
        } else {
            players[clientFd].nickname = input;
            nicknames.insert(input);
            const char* acceptedNick = "Nick zaakceptowany! Milej gry.\n";
            send(clientFd, acceptedNick, strlen(acceptedNick), 0);
            broadcast("Gracz " + input + " dolaczyl do gry.\n", clientFd);
        }
    } else {
        broadcast(players[clientFd].nickname + ": " + input + "\n", clientFd);
    }
}

int main() {
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1) {
        perror("socket");
        return 1;
    }

    setNonBlocking(serverFd);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
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

    cout << "Serwer dziala na porcie " << PORT << endl;

    while (true) {
        int numEvents = epoll_wait(epollFd, events, MAX_EVENTS, -1);
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
                const char * powitanie = "Witaj! Wprowadz swoj nick!: ";
                send(clientFd, powitanie, strlen(powitanie), 0);
            } else {
                int clientFd = events[i].data.fd;
                char buffer[BUFFER_SIZE];
                memset(buffer, 0, BUFFER_SIZE);
                int bytesRead = recv(clientFd, buffer, BUFFER_SIZE - 1, 0);

                if (bytesRead < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("recv error");
                        close(clientFd);
                        epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
                        players.erase(clientFd);
                    }
                    continue;
                }

                if (bytesRead == 0) {
                    close(clientFd);
                    epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
                    nicknames.erase(players[clientFd].nickname);
                    broadcast("Gracz " + players[clientFd].nickname + " opuscil gre.\n");
                    players.erase(clientFd);
                } else {
                    string input(buffer, bytesRead);
                    handleClientInput(clientFd, input);
                }
            }
        }
    }

    close(serverFd);
    close(epollFd);
    return 0;
}
