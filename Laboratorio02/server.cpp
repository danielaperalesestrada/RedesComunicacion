#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <mutex>

using namespace std;

map<string, int> ListOfCli;
mutex map_mutex;

string zeroPad(int number, int size) {
    string str = to_string(number);
    if (str.length() >= (size_t)size) return str;
    return string(size - str.length(), '0') + str;
}

void ThreadReadClient(int S) {
    string nickname;
    char buff[1000];
    int n, tamano;

    read(S, buff, 1);
    while (buff[0] != 'N') {
        read(S, buff, 1);
    }
    n = read(S, buff, 7);
    buff[n] = '\0';
    tamano = atoi(buff);
    n = read(S, buff, tamano);
    buff[n] = '\0';
    nickname = buff;

    {
        lock_guard<mutex> lock(map_mutex);
        ListOfCli[nickname] = S;
    }
    cout << "Cliente registrado: " << nickname << " (FD: " << S << ")\n";

    for (;;) {
        n = read(S, buff, 1);
        if (n <= 0) break;

        char action = buff[0];

        if (action == 'M') { // Mensaje Unicast
            n = read(S, buff, 7);
            buff[n] = '\0';
            tamano = atoi(buff);
            n = read(S, buff, tamano);
            buff[n] = '\0';
            string destination = buff;

            n = read(S, buff, 11);
            buff[n] = '\0';
            tamano = atoi(buff);
            n = read(S, buff, tamano);
            buff[n] = '\0';
            string msg = buff;

            string payload = "m" + zeroPad(nickname.size(), 7) + nickname + zeroPad(msg.size(), 11) + msg;

            lock_guard<mutex> lock(map_mutex);
            if (ListOfCli.count(destination)) {
                write(ListOfCli[destination], payload.c_str(), payload.size());
            }
        }
        else if (action == 'B') { // Mensaje Broadcast
            n = read(S, buff, 11);
            buff[n] = '\0';
            tamano = atoi(buff);
            n = read(S, buff, tamano);
            buff[n] = '\0';
            string msg = buff;

            string payload = "b" + zeroPad(nickname.size(), 7) + nickname + zeroPad(msg.size(), 11) + msg;

            lock_guard<mutex> lock(map_mutex);
            for (auto const& [nick, socket_fd] : ListOfCli) {
                if (socket_fd != S) { // No autoreenviar al emisor
                    write(socket_fd, payload.c_str(), payload.size());
                }
            }
        }
        else if (action == 'Q') { // Desconexión
            break;
        }
    }

    {
        lock_guard<mutex> lock(map_mutex);
        ListOfCli.erase(nickname);
    }
    cout << "Cliente desconectado: " << nickname << "\n";
    close(S);
}

int main(int argc, char* argv[]) {
    int port = (argc >= 2) ? atoi(argv[1]) : 45000;

    int ServerSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ServerSocket == -1) {
        perror("Error creando el socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in stSockAddr;
    memset(&stSockAddr, 0, sizeof(stSockAddr));
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(port);
    stSockAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(ServerSocket, (const struct sockaddr *)&stSockAddr, sizeof(stSockAddr)) == -1) {
        perror("Error bind failed");
        close(ServerSocket);
        exit(EXIT_FAILURE);
    }

    if (listen(ServerSocket, 10) == -1) {
        perror("Error listen failed");
        close(ServerSocket);
        exit(EXIT_FAILURE);
    }

    cout << "Servidor escuchando en puerto " << port << "...\n";

    for (;;) {
        int ClientSocket = accept(ServerSocket, NULL, NULL);
        if (ClientSocket < 0) continue;
        thread(ThreadReadClient, ClientSocket).detach();
    }

    close(ServerSocket);
    return 0;
}
