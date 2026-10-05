#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <cctype>

using namespace std;

map<string, int> clientes;
mutex clientes_mutex;

string zeroPad(size_t number, int size) {
    string str = to_string(number);
    if (str.length() >= (size_t)size) return str;
    return string(size - str.length(), '0') + str;
}

string crearTramaRespuestaMensaje(char tipo, const string& emisor, const string& msg) {
    return string(1, tipo) + zeroPad(emisor.size(), 7) + emisor + zeroPad(msg.size(), 11) + msg;
}

string crearTramaLista(const string& lista) {
    return "l" + zeroPad(lista.size(), 17) + lista;
}

string crearTramaError(const string& err_msg) {
    return "E" + zeroPad(err_msg.size(), 11) + err_msg;
}

string crearTramaArchivo(char tipo, const string& emisor, const string& filename, const string& content) {
    return string(1, tipo) +
           zeroPad(emisor.size(), 13) + emisor +
           zeroPad(filename.size(), 13) + filename +
           zeroPad(content.size(), 25) + content;
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

enum RolJuego { NEUTRAL, JUGADOR, OBSERVADOR };

struct InfoJuego {
    RolJuego rol = NEUTRAL;
    char ficha = ' ';
};

map<int, InfoJuego> juego_info;
char tablero[9] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
char turno_actual = 'X';

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

bool enviarBytes(int fd, const char* buf, size_t n) {
    size_t enviados = 0;
    while (enviados < n) {
        ssize_t r = send(fd, buf + enviados, n - enviados, MSG_NOSIGNAL);
        if (r <= 0) return false;
        enviados += r;
    }
    return true;
}

void reiniciarTablero() {
    memset(tablero, ' ', 9);
    turno_actual = 'X';
}

bool tableroVacio() {
    for (int i = 0; i < 9; ++i)
        if (tablero[i] != ' ') return false;
    return true;
}

char revisarTablero() {
    static const int gana[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
        {0, 4, 8}, {2, 4, 6}
    };
    for (const auto& g : gana)
        if (tablero[g[0]] != ' ' && tablero[g[0]] == tablero[g[1]] && tablero[g[1]] == tablero[g[2]])
            return tablero[g[0]];
    for (int i = 0; i < 9; ++i)
        if (tablero[i] == ' ') return ' ';
    return 'D';
}

int jugadorConFicha(char ficha) {
    for (auto const& [fd, info] : juego_info)
        if (info.rol == JUGADOR && info.ficha == ficha) return fd;
    return -1;
}

bool partidaLista() { return jugadorConFicha('X') != -1 && jugadorConFicha('O') != -1; }

void enviarTrama(int fd, const string& trama) {
    enviarBytes(fd, trama.data(), trama.size());
}

void enviarTablero(int fd) {
    enviarTrama(fd, tramaJuego(CMD_TABLERO, string(tablero, 9)));
}

void enviarTurno(int fd) {
    enviarTrama(fd, tramaJuego(CMD_FICHA_TURNO, string(1, turno_actual)));
}

void enviarComando(int fd, char cmd) {
    enviarTrama(fd, tramaJuego(cmd));
}

void difundirTablero() {
    for (auto const& [fd, info] : juego_info)
        if (info.rol != NEUTRAL) enviarTablero(fd);
}

void difundirTurno() {
    for (auto const& [fd, info] : juego_info)
        if (info.rol != NEUTRAL) enviarTurno(fd);
}

void terminarPartida() {
    for (auto& [fd, info] : juego_info) {
        if (info.rol == JUGADOR) { info.rol = NEUTRAL; info.ficha = ' '; }
    }
    reiniciarTablero();
}

void jugadorSalio(char ficha, const string& nick) {
    if (!tableroVacio()) {
        int otro = jugadorConFicha(ficha == 'X' ? 'O' : 'X');
        if (otro != -1) {
            enviarComando(otro, CMD_VICTORIA);
            cout << "[JUEGO] " << nick << " abandono: gana " << (ficha == 'X' ? 'O' : 'X') << "\n";
        }
        terminarPartida();
    } else {
        reiniciarTablero();
    }
}

string etiquetaJuego(int fd) {
    auto it = juego_info.find(fd);
    if (it == juego_info.end()) return "neutral";
    switch (it->second.rol) {
        case JUGADOR:    return string("jugador ") + it->second.ficha;
        case OBSERVADOR: return "viewer";
        default:         return "neutral";
    }
}

void juegoPedirJugar(int fd, const string& nick) {
    InfoJuego& yo = juego_info[fd];
    cout << "[JUEGO] " << nick << " -> T+P (jugar)\n";
    if (yo.rol == JUGADOR) return;
    char f = (jugadorConFicha('X') == -1) ? 'X' : (jugadorConFicha('O') == -1 ? 'O' : ' ');
    if (f == ' ') {
        cout << "  mesa llena, se ignora\n";
        return;
    }
    yo.rol = JUGADOR;
    yo.ficha = f;
    enviarTrama(fd, tramaJuego(CMD_FICHA_TURNO, string(1, f)));
    cout << "  " << nick << " juega con " << f << "\n";
    if (partidaLista()) {
        reiniciarTablero();
        cout << "[JUEGO] Empieza la partida\n";
        difundirTablero();
        difundirTurno();
    }
}

void juegoPedirObservar(int fd, const string& nick) {
    InfoJuego& yo = juego_info[fd];
    cout << "[JUEGO] " << nick << " -> T+V (observador)\n";
    if (yo.rol == JUGADOR) {
        char f = yo.ficha;
        yo.rol = OBSERVADOR; yo.ficha = ' ';
        jugadorSalio(f, nick);
    } else {
        yo.rol = OBSERVADOR;
    }
    enviarTablero(fd);
    if (partidaLista()) enviarTurno(fd);
}

void juegoPedirNeutral(int fd, const string& nick) {
    InfoJuego& yo = juego_info[fd];
    cout << "[JUEGO] " << nick << " -> T+N (neutral)\n";
    if (yo.rol == JUGADOR) {
        char f = yo.ficha;
        yo.rol = NEUTRAL; yo.ficha = ' ';
        jugadorSalio(f, nick);
    } else {
        yo.rol = NEUTRAL;
    }
}

void juegoMovimiento(int fd, const string& nick, unsigned char pos) {
    InfoJuego& yo = juego_info[fd];
    if (yo.rol != JUGADOR || !partidaLista()) return;
    bool ok = (yo.ficha == turno_actual) && pos >= 1 && pos <= 9 && tablero[pos - 1] == ' ';
    if (!ok) {
        enviarTurno(fd);
        return;
    }
    tablero[pos - 1] = yo.ficha;
    cout << "[JUEGO] " << nick << " (" << yo.ficha << ") juega en " << (int)pos << "\n";
    difundirTablero();

    char estado = revisarTablero();
    if (estado == ' ') {
        turno_actual = (turno_actual == 'X') ? 'O' : 'X';
        difundirTurno();
        return;
    }
    if (estado == 'X' || estado == 'O') {
        int ganador  = jugadorConFicha(estado);
        int perdedor = jugadorConFicha(estado == 'X' ? 'O' : 'X');
        if (ganador  != -1) enviarComando(ganador,  CMD_VICTORIA);
        if (perdedor != -1) enviarComando(perdedor, CMD_DERROTA);
        cout << "[JUEGO] Gana " << estado << "\n";
    } else {
        cout << "[JUEGO] Empate (sin mensaje: los clientes lo deducen)\n";
    }
    terminarPartida();
}

int iniciarServidor(const string& ip, int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == -1) {
        perror("Error creando el socket del servidor");
        return -1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
        cerr << "Dirección IP inválida: " << ip << "\n";
        close(server_fd);
        return -1;
    }

    if (bind(server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Error en bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 10) == -1) {
        perror("Error en listen");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

string registrarClienteServidor(int client_fd) {
    char action;
    if (!readExact(client_fd, &action, 1)) return "";
    while (action != 'N') {
        if (!readExact(client_fd, &action, 1)) return "";
    }

    char buff[100];
    if (!readExact(client_fd, buff, 7)) return "";
    buff[7] = '\0';
    int tamano_nick = atoi(buff);

    string nickname(tamano_nick, '\0');
    if (!readExact(client_fd, &nickname[0], tamano_nick)) return "";

    {
        lock_guard<mutex> lock(clientes_mutex);
        clientes[nickname] = client_fd;
        juego_info[client_fd] = InfoJuego();
    }

    cout << "[+ Conectado] Cliente: " << nickname << " (FD: " << client_fd << ")\n";
    return nickname;
}

void desconectarCliente(int client_fd, const string& nickname) {
    {
        lock_guard<mutex> lock(clientes_mutex);
        if (!nickname.empty()) {
            clientes.erase(nickname);
            cout << "[- Desconectado] Cliente: " << nickname << "\n";
        }
        auto it = juego_info.find(client_fd);
        if (it != juego_info.end()) {
            InfoJuego info = it->second;
            juego_info.erase(it);
            if (info.rol == JUGADOR) jugadorSalio(info.ficha, nickname);
        }
    }
    close(client_fd);
}

void AtenderCliente(int client_fd) {
    string emisor_nick = registrarClienteServidor(client_fd);
    if (emisor_nick.empty()) {
        close(client_fd);
        return;
    }

    char buff[1000];

    while (true) {
        char action;
        if (!readExact(client_fd, &action, 1)) break;

        if (action == 'M') {
            if (!readExact(client_fd, buff, 7)) break;
            buff[7] = '\0';
            int dest_len = atoi(buff);

            string dest_nick(dest_len, '\0');
            if (!readExact(client_fd, &dest_nick[0], dest_len)) break;

            if (!readExact(client_fd, buff, 11)) break;
            buff[11] = '\0';
            int msg_len = atoi(buff);

            string msg(msg_len, '\0');
            if (!readExact(client_fd, &msg[0], msg_len)) break;

            lock_guard<mutex> lock(clientes_mutex);
            if (clientes.count(dest_nick)) {
                string payload = crearTramaRespuestaMensaje('m', emisor_nick, msg);
                write(clientes[dest_nick], payload.c_str(), payload.size());
            } else {
                string err = crearTramaError("Usuario '" + dest_nick + "' no existe.");
                write(client_fd, err.c_str(), err.size());
            }
        }
        else if (action == 'B') {
            if (!readExact(client_fd, buff, 11)) break;
            buff[11] = '\0';
            int msg_len = atoi(buff);

            string msg(msg_len, '\0');
            if (!readExact(client_fd, &msg[0], msg_len)) break;

            string payload = crearTramaRespuestaMensaje('b', emisor_nick, msg);

            lock_guard<mutex> lock(clientes_mutex);
            for (auto const& [nick, socket_fd] : clientes) {
                if (socket_fd != client_fd) {
                    write(socket_fd, payload.c_str(), payload.size());
                }
            }
        }
        else if (action == 'L') {
            string lista_csv = "";
            {
                lock_guard<mutex> lock(clientes_mutex);
                for (auto const& [nick, socket_fd] : clientes) {
                    if (!lista_csv.empty()) lista_csv += ", ";
                    lista_csv += nick + " (" + etiquetaJuego(socket_fd) + ")";
                }
            }
            string payload = crearTramaLista(lista_csv);
            write(client_fd, payload.c_str(), payload.size());
        }
        else if (action == 'F') {
            if (!readExact(client_fd, buff, 13)) break;
            buff[13] = '\0';
            int dest_len = atoi(buff);

            string dest_nick(dest_len, '\0');
            if (!readExact(client_fd, &dest_nick[0], dest_len)) break;

            if (!readExact(client_fd, buff, 13)) break;
            buff[13] = '\0';
            int filename_len = atoi(buff);

            string filename(filename_len, '\0');
            if (!readExact(client_fd, &filename[0], filename_len)) break;

            if (!readExact(client_fd, buff, 25)) break;
            buff[25] = '\0';
            size_t file_len = atol(buff);

            string content(file_len, '\0');
            if (!readExact(client_fd, &content[0], file_len)) break;

            lock_guard<mutex> lock(clientes_mutex);
            if (clientes.count(dest_nick)) {
                string payload = crearTramaArchivo('f', emisor_nick, filename, content);
                write(clientes[dest_nick], payload.c_str(), payload.size());
            } else {
                string err = crearTramaError("No se pudo enviar archivo: '" + dest_nick + "' no existe.");
                write(client_fd, err.c_str(), err.size());
            }
        }
        else if (action == JUEGO) {
            char cmd;
            if (!readExact(client_fd, &cmd, 1)) break;

            if (cmd == CMD_JUGAR) {
                lock_guard<mutex> lock(clientes_mutex);
                juegoPedirJugar(client_fd, emisor_nick);
            }
            else if (cmd == CMD_OBSERVAR) {
                lock_guard<mutex> lock(clientes_mutex);
                juegoPedirObservar(client_fd, emisor_nick);
            }
            else if (cmd == CMD_NEUTRAL) {
                lock_guard<mutex> lock(clientes_mutex);
                juegoPedirNeutral(client_fd, emisor_nick);
            }
            else if (cmd == CMD_MOVER) {
                char p;
                if (!readExact(client_fd, &p, 1)) break;
                lock_guard<mutex> lock(clientes_mutex);
                juegoMovimiento(client_fd, emisor_nick, (unsigned char)p);
            }
        }
        else if (action == 'Q') {
            break;
        }
    }

    desconectarCliente(client_fd, emisor_nick);
}

static bool soloDigitos(const char* s) {
    if (!*s) return false;
    for (; *s; ++s) if (!isdigit((unsigned char)*s)) return false;
    return true;
}

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);

    string ip = "0.0.0.0";
    int port = 45000;
    if (argc == 2 && soloDigitos(argv[1])) {
        port = atoi(argv[1]);
    } else {
        if (argc >= 2) ip = argv[1];
        if (argc >= 3) port = atoi(argv[2]);
    }

    int server_fd = iniciarServidor(ip, port);
    if (server_fd == -1) return 1;

    cout << "Servidor corriendo en " << ip << ":" << port << "...\n" << flush;

    while (true) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
        thread(AtenderCliente, client_fd).detach();
    }

    close(server_fd);
    return 0;
}
