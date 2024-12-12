#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <fcntl.h>
#include <sys/epoll.h>

using namespace std;

#define BUFFER_SIZE 1024
#define MAX_EVENTS 10

void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void handleServerResponse(int socket, int epollFd) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int bytesReceived = recv(socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytesReceived <= 0) {
        std::cout << "Disconnected from server." << std::endl;
        close(socket);
        epoll_ctl(epollFd, EPOLL_CTL_DEL, socket, nullptr);
        exit(0);
    }
    write(1, buffer, bytesReceived);
    //cout << buffer << endl;
}

void handleUserInput(int socket) {
    std::string input;
    std::getline(std::cin, input);
    if (send(socket, input.c_str(), input.size(), 0) == -1) {
        perror("send");
    }
}

int main() {
    const char *serverIp = "127.0.0.1";
    const int serverPort = 1234;

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        perror("socket");
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) <= 0) {
        perror("inet_pton");
        close(clientSocket);
        return 1;
    }

    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("connect");
        close(clientSocket);
        return 1;
    }

    std::cout << "Connected to the server." << std::endl;

    setNonBlocking(clientSocket);

    int epollFd = epoll_create1(0);
    if (epollFd == -1) {
        perror("epoll_create1");
        close(clientSocket);
        return 1;
    }

    epoll_event event{}, events[MAX_EVENTS];
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = clientSocket;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientSocket, &event) == -1) {
        perror("epoll_ctl");
        close(clientSocket);
        close(epollFd);
        return 1;
    }

    int stdinFd = fileno(stdin);
    setNonBlocking(stdinFd);

    event.events = EPOLLIN | EPOLLET;
    event.data.fd = stdinFd;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, stdinFd, &event) == -1) {
        perror("epoll_ctl");
        close(clientSocket);
        close(epollFd);
        return 1;
    }

    bool nicknameAccepted = false;

    while (true) {
        int numEvents = epoll_wait(epollFd, events, MAX_EVENTS, -1);
        if (numEvents == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < numEvents; ++i) {
            if (events[i].data.fd == clientSocket) {
                handleServerResponse(clientSocket, epollFd);
            } else if (events[i].data.fd == stdinFd) {
                if (!nicknameAccepted) {
                    //cout << "Enter your nickname (klient): ";
                    cout << "Wbilem" << endl;
                    handleUserInput(clientSocket);
                    nicknameAccepted = true; // Assume the server will handle duplicates and respond accordingly
                } else {
                    handleUserInput(clientSocket);
                }
            }
        }
    }

    close(clientSocket);
    close(epollFd);
    return 0;
}
