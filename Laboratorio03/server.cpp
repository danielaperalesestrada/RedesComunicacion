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
#include <vector>

using namespace std;

map<string, int> clientes; // nickname -> socket_fd
mutex clientes_mutex;

string zeroPad(size_t number, int size) {
    string str = to_string(number);
    if (str.length() >= (size_t)size) return str;
    return string(size - str.length(), '0') + str;
}

// Tramas de respuesta según la pizarra
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

// Lectura exacta de N bytes por socket
bool readExact(int fd, char* buffer, size_t size) {
    size_t total_read = 0;
    while (total_read < size) {
        ssize_t bytes_read = read(fd, buffer + total_read, size - total_read);
        if (bytes_read <= 0) return false;
        total_read += bytes_read;
    }
    return true;
}

int iniciarServidor(int port) {
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
    serv_addr.sin_addr.s_addr = INADDR_ANY;

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
    }

    cout << "[+ Conectado] Cliente: " << nickname << " (FD: " << client_fd << ")\n";
    return nickname;
}

void desconectarCliente(int client_fd, const string& nickname) {
    if (!nickname.empty()) {
        lock_guard<mutex> lock(clientes_mutex);
        clientes.erase(nickname);
        cout << "[- Desconectado] Cliente: " << nickname << "\n";
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

        // --- COMANDO M: Unicast ---
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
        // --- COMANDO B: Broadcast ---
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
        // --- COMANDO L: Solicitud de Lista ---
        else if (action == 'L') {
            string lista_csv = "";
            {
                lock_guard<mutex> lock(clientes_mutex);
                for (auto const& [nick, socket_fd] : clientes) {
                    if (!lista_csv.empty()) lista_csv += ", ";
                    lista_csv += nick;
                }
            }
            string payload = crearTramaLista(lista_csv);
            write(client_fd, payload.c_str(), payload.size());
        }
        // --- COMANDO F: Envío de Archivo ---
        else if (action == 'F') {
            // Read dest nickname (13B)
            if (!readExact(client_fd, buff, 13)) break;
            buff[13] = '\0';
            int dest_len = atoi(buff);

            string dest_nick(dest_len, '\0');
            if (!readExact(client_fd, &dest_nick[0], dest_len)) break;

            // Read filename (13B)
            if (!readExact(client_fd, buff, 13)) break;
            buff[13] = '\0';
            int filename_len = atoi(buff);

            string filename(filename_len, '\0');
            if (!readExact(client_fd, &filename[0], filename_len)) break;

            // Read file content size (25B)
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
        else if (action == 'Q') {
            break;
        }
    }

    desconectarCliente(client_fd, emisor_nick);
}

int main(int argc, char* argv[]) {
    int port = (argc >= 2) ? atoi(argv[1]) : 45000;

    int server_fd = iniciarServidor(port);
    if (server_fd == -1) return 1;

    cout << "Servidor corriendo en puerto " << port << "...\n";

    while (true) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
        thread(AtenderCliente, client_fd).detach();
    }

    close(server_fd);
    return 0;
}
