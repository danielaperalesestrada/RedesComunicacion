#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>
#include <thread>

using namespace std;

bool is_running = true;

string zeroPad(size_t number, int size) {
    string str = to_string(number);
    if (str.length() >= (size_t)size) return str;
    return string(size - str.length(), '0') + str;
}

bool readExact(int fd, char* buffer, size_t size) {
    size_t total_read = 0;
    while (total_read < size) {
        ssize_t bytes_read = read(fd, buffer + total_read, size - total_read);
        if (bytes_read <= 0) return false;
        total_read += bytes_read;
    }
    return true;
}

void ThreadCliente(int socket_fd) {
    char buff[1000];

    while (is_running) {
        char action;
        if (!readExact(socket_fd, &action, 1)) break;

        // --- RESPUESTA m / b: Mensajes ---
        if (action == 'm' || action == 'b') {
            if (!readExact(socket_fd, buff, 7)) break;
            buff[7] = '\0';
            int nick_len = atoi(buff);

            string nickname(nick_len, '\0');
            if (!readExact(socket_fd, &nickname[0], nick_len)) break;

            if (!readExact(socket_fd, buff, 11)) break;
            buff[11] = '\0';
            int msg_len = atoi(buff);

            string msg(msg_len, '\0');
            if (!readExact(socket_fd, &msg[0], msg_len)) break;

            string tag = (action == 'm') ? "[Privado de " : "[Broadcast de ";
            cout << "\n" << tag << nickname << "]: " << msg << "\nComando > " << flush;
        }
        // --- RESPUESTA l: Lista de Clientes ---
        else if (action == 'l') {
            if (!readExact(socket_fd, buff, 17)) break;
            buff[17] = '\0';
            int list_len = atoi(buff);

            string lista(list_len, '\0');
            if (!readExact(socket_fd, &lista[0], list_len)) break;

            cout << "\n[Usuarios Conectados]: " << lista << "\nComando > " << flush;
        }
        // --- RESPUESTA E: Error del Servidor ---
        else if (action == 'E') {
            if (!readExact(socket_fd, buff, 11)) break;
            buff[11] = '\0';
            int err_len = atoi(buff);

            string err_msg(err_len, '\0');
            if (!readExact(socket_fd, &err_msg[0], err_len)) break;

            cout << "\n[Error del Servidor]: " << err_msg << "\nComando > " << flush;
        }
        // --- RESPUESTA f: Recepción de Archivo ---
        else if (action == 'f') {
            // 1. Origen Nickname (13B)
            if (!readExact(socket_fd, buff, 13)) break;
            buff[13] = '\0';
            int orig_len = atoi(buff);

            string orig_nick(orig_len, '\0');
            if (!readExact(socket_fd, &orig_nick[0], orig_len)) break;

            // 2. Filename (13B)
            if (!readExact(socket_fd, buff, 13)) break;
            buff[13] = '\0';
            int file_len = atoi(buff);

            string filename(file_len, '\0');
            if (!readExact(socket_fd, &filename[0], file_len)) break;

            // 3. File content (25B)
            if (!readExact(socket_fd, buff, 25)) break;
            buff[25] = '\0';
            size_t content_len = atol(buff);

            string content(content_len, '\0');
            if (!readExact(socket_fd, &content[0], content_len)) break;

            // Guardar el archivo localmente
            string out_name = "recibido_" + filename;
            ofstream outfile(out_name, ios::binary);
            outfile.write(content.c_str(), content.size());
            outfile.close();

            cout << "\n[Archivo Recibido de " << orig_nick << "]: Guardado como '" << out_name << "'\nComando > " << flush;
        }
    }
}

int conectarServidor(const string& ip, int port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd == -1) return -1;

    struct sockaddr_in stSock;
    memset(&stSock, 0, sizeof(stSock));
    stSock.sin_family = AF_INET;
    stSock.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &stSock.sin_addr);

    if (connect(socket_fd, (struct sockaddr*)&stSock, sizeof(stSock)) == -1) {
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

void registrarNickname(int socket_fd) {
    string nickname;
    cout << "Ingresa tu nickname: ";
    cin >> nickname;
    cin.ignore();

    string frame = "N" + zeroPad(nickname.size(), 7) + nickname;
    write(socket_fd, frame.c_str(), frame.size());
}

void procesarComandos(int socket_fd) {
    cout << "\n------------------- OPCIONES -------------------" << endl;
    cout << "Unicast:   M <destino> <mensaje>" << endl;
    cout << "Broadcast: B <mensaje>" << endl;
    cout << "Lista:     L" << endl;
    cout << "Archivo:   F <destino> <ruta_archivo>" << endl;
    cout << "Salir:     Q" << endl;
    cout << "------------------------------------------------\n" << endl;

    string line;
    while (is_running) {
        cout << "Comando > " << flush;
        if (!getline(cin, line) || line.empty()) continue;

        char option = line[0];

        if (option == 'Q') {
            string frame = "Q";
            write(socket_fd, frame.c_str(), frame.size());
            is_running = false;
            break;
        }
        else if (option == 'L') {
            string frame = "L";
            write(socket_fd, frame.c_str(), frame.size());
        }
        else if (option == 'M') {
            size_t space1 = line.find(' ', 2);
            if (space1 != string::npos) {
                string dest = line.substr(2, space1 - 2);
                string msg = line.substr(space1 + 1);
                string frame = "M" + zeroPad(dest.size(), 7) + dest + zeroPad(msg.size(), 11) + msg;
                write(socket_fd, frame.c_str(), frame.size());
            } else {
                cout << "Formato incorrecto. Uso: M <destino> <mensaje>\n";
            }
        }
        else if (option == 'B') {
            if (line.size() > 2) {
                string msg = line.substr(2);
                string frame = "B" + zeroPad(msg.size(), 11) + msg;
                write(socket_fd, frame.c_str(), frame.size());
            } else {
                cout << "Formato incorrecto. Uso: B <mensaje>\n";
            }
        }
        else if (option == 'F') {
            // Formato esperable: F <destino> <filepath>
            size_t space1 = line.find(' ', 2);
            if (space1 != string::npos) {
                string dest = line.substr(2, space1 - 2);
                string filepath = line.substr(space1 + 1);

                ifstream file(filepath, ios::binary | ios::ate);
                if (!file.is_open()) {
                    cout << "Error: No se pudo abrir el archivo local '" << filepath << "'\n";
                    continue;
                }

                streamsize size = file.tellg();
                file.seekg(0, ios::beg);

                string content(size, '\0');
                if (file.read(&content[0], size)) {
                    // Extraer solo el nombre sin rutas (ej: ./docs/foto.png -> foto.png)
                    size_t last_slash = filepath.find_last_of("/\\");
                    string filename = (last_slash == string::npos) ? filepath : filepath.substr(last_slash + 1);

                    string frame = "F" + 
                                   zeroPad(dest.size(), 13) + dest + 
                                   zeroPad(filename.size(), 13) + filename + 
                                   zeroPad(content.size(), 25) + content;
                    
                    write(socket_fd, frame.c_str(), frame.size());
                    cout << "Archivo '" << filename << "' enviado hacia " << dest << endl;
                }
                file.close();
            } else {
                cout << "Formato incorrecto. Uso: F <destino> <ruta_archivo>\n";
            }
        }
    }
}

int main(int argc, char* argv[]) {
    string ip = (argc >= 2) ? argv[1] : "127.0.0.1";
    int port = (argc >= 3) ? atoi(argv[2]) : 45000;

    int socket_fd = conectarServidor(ip, port);
    if (socket_fd == -1) {
        cerr << "No se pudo conectar al servidor " << ip << ":" << port << endl;
        return 1;
    }

    registrarNickname(socket_fd);

    thread reader(ThreadCliente, socket_fd);
    procesarComandos(socket_fd);

    if (reader.joinable()) reader.join();
    close(socket_fd);

    return 0;
}
