#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <thread>

using namespace std;

bool is_running = true;

string zeroPad(int number, int size) {
    string str = to_string(number);
    if (str.length() >= (size_t)size) return str;
    return string(size - str.length(), '0') + str;
}

void ThreadReadServer(int socket_fd) {
    char buff[1000];
    while (is_running) {
        int n = read(socket_fd, buff, 1);
        if (n <= 0) {
            if (is_running) cout << "\n[Sistema] Desconectado del servidor.\n";
            is_running = false;
            break;
        }

        char action = buff[0];
        if (action == 'm' || action == 'b') {
            n = read(socket_fd, buff, 7);
            buff[n] = '\0';
            int nick_len = atoi(buff);
            n = read(socket_fd, buff, nick_len);
            buff[n] = '\0';
            string sender_nick = buff;

            n = read(socket_fd, buff, 11);
            buff[n] = '\0';
            int msg_len = atoi(buff);
            n = read(socket_fd, buff, msg_len);
            buff[n] = '\0';
            string msg = buff;

            string type = (action == 'm') ? "[Privado de " : "[Broadcast de ";
            cout << "\n" << type << sender_nick << "]: " << msg << "\nComando > " << flush;
        }
    }
}

int main(int argc, char* argv[]) {
    string ip = (argc >= 2) ? argv[1] : "127.0.0.1";
    int port = (argc >= 3) ? atoi(argv[2]) : 45000;

    int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd == -1) {
        perror("Error al crear socket");
        return 1;
    }

    struct sockaddr_in stSockAddr;
    memset(&stSockAddr, 0, sizeof(stSockAddr));
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &stSockAddr.sin_addr);

    if (connect(socket_fd, (struct sockaddr*)&stSockAddr, sizeof(stSockAddr)) == -1) {
        perror("Error de conexión");
        close(socket_fd);
        return 1;
    }

    string nickname;
    cout << "Ingrese su Nickname: ";
    cin >> nickname;
    cin.ignore();

    string reg_frame = "N" + zeroPad(nickname.size(), 7) + nickname;
    write(socket_fd, reg_frame.c_str(), reg_frame.size());

    thread reader(ThreadReadServer, socket_fd);

    cout << "\n--- Opciones ---" << endl;
    cout << "  Unicast:   M <destino> <mensaje>" << endl;
    cout << "  Broadcast: B <mensaje>" << endl;
    cout << "  Salir:     Q" << endl;

    string line;
    while (is_running) {
        cout << "Comando > ";
        if (!getline(cin, line) || line.empty()) continue;

        char option = line[0];
        if (option == 'Q') {
            string q_frame = "Q";
            write(socket_fd, q_frame.c_str(), q_frame.size());
            is_running = false;
            break;
        }
        else if (option == 'M') {
            size_t space1 = line.find(' ', 2);
            if (space1 != string::npos) {
                string dest = line.substr(2, space1 - 2);
                string msg = line.substr(space1 + 1);
                string frame = "M" + zeroPad(dest.size(), 7) + dest + zeroPad(msg.size(), 11) + msg;
                write(socket_fd, frame.c_str(), frame.size());
            } else {
                cout << "[Error] Formato invalido. Uso: M <destino> <mensaje>\n";
            }
        }
        else if (option == 'B') {
            if (line.size() > 2) {
                string msg = line.substr(2);
                string frame = "B" + zeroPad(msg.size(), 11) + msg;
                write(socket_fd, frame.c_str(), frame.size());
            } else {
                cout << "[Error] Formato invalido. Uso: B <mensaje>\n";
            }
        }
    }

    if (reader.joinable()) reader.join();
    close(socket_fd);
    return 0;
}
