#ifndef WISIELEC_H
#define WISIELEC_H
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

const int topicsNumber = 3;
extern string topics[topicsNumber];
extern string topic;
extern string pass;
extern int bonus;
extern bool isLeader;
extern int countdown;
extern bool startGame;
extern std::chrono::steady_clock::time_point lastTime;
extern int activePlayers;

// Struktura gracza
struct Player {
    string nickname;
    int score = 0;
    int hangman = 0;
    int playerStatus = 2;
    bool isLeader = false;
    string password = "";
    time_t time = -1; // Czas spędzony w pokoju (domyślnie 0 sekund)
    bool usedChars[26] = {false};
};

extern unordered_map<int, Player> players;
extern unordered_set<string> nicknames;

void setNonBlocking(int fd);
void broadcast(const string &message, int excludeFd, int flag);
void createScoreBoard();
void chooseLeader();
void resetPassword(int fd);
void setNewPassword();
void resetGame();
void deletePlayer(int epollFd, int clientFd);
void playerLeft(int clientFd);
void timeEnded();
void checkLosers();
void updatePassword(char ans, int fd);
void handleClientInput(int clientFd, const string &input);
int runMainLoop(int port);
#endif // GAME_SERVER_H
