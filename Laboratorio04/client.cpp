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
#include <mutex>
#include <cctype>

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

const char JUEGO = 'T';
const char CMD_JUGAR    = 'P';
const char CMD_OBSERVAR = 'V';
const char CMD_NEUTRAL  = 'N';
const char CMD_MOVER    = 'M';
const char CMD_FICHA_TURNO = 't';
const char CMD_TABLERO     = 'T';
const char CMD_VICTORIA    = 'W';
const char CMD_DERROTA     = 'O';

string tramaJuego(char cmd, const string& datos = "") {
    return string(1, JUEGO) + string(1, cmd) + datos;
}

void enviarJuego(int fd, char cmd, const string& datos = "") {
    string t = tramaJuego(cmd, datos);
    write(fd, t.data(), t.size());
}

enum EstadoJuego { J_NEUTRAL, J_UNIENDOSE, J_JUGANDO, J_OBSERVANDO };

mutex mtx_juego;
EstadoJuego estado_juego = J_NEUTRAL;
char mi_ficha = ' ';
char turno = ' ';
char tablero[9] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
bool tablero_final = false;

void limpiarTablero() {
    memset(tablero, ' ', 9);
    turno = ' ';
    tablero_final = false;
}

void dibujarTablero() {
    cout << "\n--- Estado del Tablero ---\n";
    for (int i = 0; i < 9; i += 3) {
        for (int j = 0; j < 3; ++j) {
            char c = tablero[i + j];
            if (c == ' ') cout << (i + j + 1);
            else cout << c;
            if (j < 2) cout << " | ";
        }
        cout << "\n";
        if (i < 6) cout << "---+---+---\n";
    }
    cout << "--------------------------\n";
}

char revisarTablero(const char* b) {
    static const int gana[8][3] = {
        {0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}
    };
    for (const auto& g : gana)
        if (b[g[0]] != ' ' && b[g[0]] == b[g[1]] && b[g[1]] == b[g[2]]) return b[g[0]];
    for (int i = 0; i < 9; ++i)
        if (b[i] == ' ') return ' ';
    return 'D';
}

void juegoTurnoFicha(char f) {
    lock_guard<mutex> lock(mtx_juego);
    if (estado_juego == J_UNIENDOSE) {
        mi_ficha = f;
        estado_juego = J_JUGANDO;
        limpiarTablero();
        cout << "\n[Juego] Te asignaron la ficha " << f << ". Esperando al rival...";
    } else if (estado_juego == J_JUGANDO) {
        turno = f;
        if (f == mi_ficha) cout << "\n[Juego] Es tu turno. Escribe el número de la casilla (1-9).";
        else               cout << "\n[Juego] Turno del rival.";
    } else if (estado_juego == J_OBSERVANDO) {
        turno = f;
        cout << "\n[Juego] Turno de " << f << ".";
    } else {
        return;
    }
    cout << "\nComando > " << flush;
}

void juegoTablero(const char* t) {
    lock_guard<mutex> lock(mtx_juego);
    if (estado_juego != J_JUGANDO && estado_juego != J_OBSERVANDO) return;
    memcpy(tablero, t, 9);
    dibujarTablero();
    char s = revisarTablero(tablero);
    tablero_final = (s == 'X' || s == 'O');
    if (s == 'D') {
        cout << "[Juego] Empate: el tablero está lleno.\n";
        if (estado_juego == J_JUGANDO) estado_juego = J_NEUTRAL;
    } else if (tablero_final && estado_juego == J_OBSERVANDO) {
        cout << "[Juego] Fin de la partida: ganó " << s << ".\n";
    }
    cout << "Comando > " << flush;
}

void juegoVictoria() {
    lock_guard<mutex> lock(mtx_juego);
    if (estado_juego != J_JUGANDO) return;
    cout << (tablero_final ? "[Juego] ¡FELICIDADES! ¡Ganaste la partida! (You Win)\n"
                           : "[Juego] El rival abandonó. ¡Ganaste! (You Win)\n");
    estado_juego = J_NEUTRAL;
    cout << "Comando > " << flush;
}

void juegoDerrota() {
    lock_guard<mutex> lock(mtx_juego);
    if (estado_juego != J_JUGANDO) return;
    cout << "[Juego] HAS PERDIDO. (You Lost)\n";
    estado_juego = J_NEUTRAL;
    cout << "Comando > " << flush;
}

void comandoJugar(int socket_fd) {
    lock_guard<mutex> lock(mtx_juego);
    if (estado_juego != J_NEUTRAL) {
        cout << "Ya participas en el juego. Usa N para volver a neutral.\n";
        return;
    }
    estado_juego = J_UNIENDOSE;
    enviarJuego(socket_fd, CMD_JUGAR);
    cout << "Solicitud enviada (T + P). Si ya hay dos jugadores no habrá respuesta; usa N para cancelar.\n";
}

void comandoObservar(int socket_fd) {
    lock_guard<mutex> lock(mtx_juego);
    if (estado_juego != J_NEUTRAL) {
        cout << "Ya participas en el juego. Usa N para volver a neutral.\n";
        return;
    }
    estado_juego = J_OBSERVANDO;
    limpiarTablero();
    enviarJuego(socket_fd, CMD_OBSERVAR);
    cout << "Ahora eres observador (T + V).\n";
}

void comandoNeutral(int socket_fd) {
    lock_guard<mutex> lock(mtx_juego);
    if (estado_juego == J_JUGANDO) cout << "Abandonaste la partida.\n";
    estado_juego = J_NEUTRAL;
    enviarJuego(socket_fd, CMD_NEUTRAL);
    cout << "Modo neutral: ni juegas ni observas (el chat sigue activo).\n";
}

void comandoJugada(int socket_fd, int casilla) {
    lock_guard<mutex> lock(mtx_juego);
    if (estado_juego != J_JUGANDO) { cout << "No estás jugando. Usa P para pedir jugar.\n"; return; }
    if (turno != mi_ficha)         { cout << "No es tu turno.\n"; return; }
    if (tablero[casilla - 1] != ' ') { cout << "Casilla ocupada, elige otra.\n"; return; }
    turno = ' ';
    enviarJuego(socket_fd, CMD_MOVER, string(1, (char)casilla));
}

void ThreadCliente(int socket_fd) {
    char buff[1000];

    while (is_running) {
        char action;
        if (!readExact(socket_fd, &action, 1)) break;

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
        else if (action == 'l') {
            if (!readExact(socket_fd, buff, 17)) break;
            buff[17] = '\0';
            int list_len = atoi(buff);

            string lista(list_len, '\0');
            if (!readExact(socket_fd, &lista[0], list_len)) break;

            cout << "\n[Usuarios Conectados]: " << lista << "\nComando > " << flush;
        }
        else if (action == 'E') {
            if (!readExact(socket_fd, buff, 11)) break;
            buff[11] = '\0';
            int err_len = atoi(buff);

            string err_msg(err_len, '\0');
            if (!readExact(socket_fd, &err_msg[0], err_len)) break;

            cout << "\n[Error del Servidor]: " << err_msg << "\nComando > " << flush;
        }
        else if (action == 'f') {
            if (!readExact(socket_fd, buff, 13)) break;
            buff[13] = '\0';
            int orig_len = atoi(buff);

            string orig_nick(orig_len, '\0');
            if (!readExact(socket_fd, &orig_nick[0], orig_len)) break;

            if (!readExact(socket_fd, buff, 13)) break;
            buff[13] = '\0';
            int file_len = atoi(buff);

            string filename(file_len, '\0');
            if (!readExact(socket_fd, &filename[0], file_len)) break;

            if (!readExact(socket_fd, buff, 25)) break;
            buff[25] = '\0';
            size_t content_len = atol(buff);

            string content(content_len, '\0');
            if (!readExact(socket_fd, &content[0], content_len)) break;

            string out_name = "recibido_" + filename;
            ofstream outfile(out_name, ios::binary);
            outfile.write(content.c_str(), content.size());
            outfile.close();

            cout << "\n[Archivo Recibido de " << orig_nick << "]: Guardado como '" << out_name << "'\nComando > " << flush;
        }
        else if (action == JUEGO) {
            char cmd;
            if (!readExact(socket_fd, &cmd, 1)) break;
            if (cmd == CMD_FICHA_TURNO) {
                char f;
                if (!readExact(socket_fd, &f, 1)) break;
                juegoTurnoFicha(f);
            } else if (cmd == CMD_TABLERO) {
                char t[9];
                if (!readExact(socket_fd, t, 9)) break;
                juegoTablero(t);
            } else if (cmd == CMD_VICTORIA) {
                juegoVictoria();
            } else if (cmd == CMD_DERROTA) {
                juegoDerrota();
            }
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
    cout << "Juego:     P (jugar) | V (observar) | N (neutral)" << endl;
    cout << "           1-9 (jugada: escribe solo el número de la casilla)" << endl;
    cout << "Salir:     Q" << endl;
    cout << "------------------------------------------------\n" << endl;

    string line;
    while (is_running) {
        cout << "Comando > " << flush;
        if (!getline(cin, line)) {
            write(socket_fd, "Q", 1);
            is_running = false;
            break;
        }
        if (line.empty()) continue;

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
        else if (option == 'P') {
            comandoJugar(socket_fd);
        }
        else if (option == 'V') {
            comandoObservar(socket_fd);
        }
        else if (option == 'N') {
            comandoNeutral(socket_fd);
        }
        else if (line.size() == 1 && option >= '1' && option <= '9') {
            comandoJugada(socket_fd, option - '0');
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

    shutdown(socket_fd, SHUT_RDWR);
    if (reader.joinable()) reader.join();
    close(socket_fd);

    return 0;
}
