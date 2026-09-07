#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

std::vector<int> clients;
std::mutex clients_mutex;

void broadcast_message(const std::string& message, int sender_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (int client_fd : clients) {
        // Broadcast a todos los demás clientes
        if (client_fd != sender_fd) {
            write(client_fd, message.c_str(), message.length());
        }
    }
}

void handle_client(int client_fd) {
    char buffer[256];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

        if (bytes_read <= 0 || strcmp(buffer, "END\n") == 0 || strcmp(buffer, "END") == 0) {
            std::cout << "Cliente desconectado (FD: " << client_fd << ").\n";
            break;
        }

        buffer[bytes_read] = '\0';
        std::cout << "[FD " << client_fd << "]: " << buffer;

        // Reenviar mensaje a los demás clientes
        broadcast_message(std::string(buffer), client_fd);
    }

    // Remover cliente de la lista al desconectarse
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(std::remove(clients.begin(), clients.end(), client_fd), clients.end());
    }
    close(client_fd);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Uso: " << argv[0] << " <puerto>\n";
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == -1) {
        perror("Error creando el socket");
        return 1;
    }

    sockaddr_in stSockAddr;
    memset(&stSockAddr, 0, sizeof(stSockAddr));
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(std::stoi(argv[1]));
    stSockAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&stSockAddr, sizeof(stSockAddr)) == -1) {
        perror("Bind falló");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) == -1) {
        perror("Listen falló");
        close(server_fd);
        return 1;
    }

    std::cout << "Servidor escuchando en el puerto " << argv[1] << "...\n";

    while (true) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(client_fd);
        }

        std::cout << "Nuevo cliente conectado (FD: " << client_fd << ").\n";
        
        // Se lanza un hilo por cada cliente conectado
        std::thread(handle_client, client_fd).detach();
    }

    close(server_fd);
    return 0;
}
