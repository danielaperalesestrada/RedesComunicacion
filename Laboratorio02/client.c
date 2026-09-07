#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

bool is_running = true;

// Hilo exclusivo para la lectura en segundo plano
void thread_read(int socket_fd) {
    char buffer[256];
    while (is_running) {
        memset(buffer, 0, sizeof(buffer));
        int n = read(socket_fd, buffer, sizeof(buffer) - 1);

        if (n <= 0) {
            if (is_running) {
                std::cout << "\nEl servidor se ha desconectado.\n";
            }
            is_running = false;
            break;
        }

        buffer[n] = '\0';
        std::cout << "\n[Broadcast]: " << buffer;
        std::cout << "Mensaje > " << std::flush;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Uso: " << argv[0] << " <IP> <puerto>\n";
        return 1;
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd == -1) {
        perror("Error creando el socket");
        return 1;
    }

    sockaddr_in stSockAddr;
    memset(&stSockAddr, 0, sizeof(stSockAddr));
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(std::stoi(argv[2]));
    inet_pton(AF_INET, argv[1], &stSockAddr.sin_addr);

    if (connect(socket_fd, (struct sockaddr*)&stSockAddr, sizeof(stSockAddr)) == -1) {
        perror("Conexión fallida");
        close(socket_fd);
        return 1;
    }

    std::cout << "Conectado al servidor " << argv[1] << ":" << argv[2] << "\n";

    // Hilo dedicado a escuchar respuestas en broadcast
    std::thread reader(thread_read, socket_fd);

    std::string buffer;
    while (is_running) {
        std::cout << "Mensaje > ";
        if (!std::getline(std::cin, buffer)) break;

        if (buffer == "END") {
            write(socket_fd, "END\n", 4);
            is_running = false;
            break;
        }

        buffer += "\n";
        write(socket_fd, buffer.c_str(), buffer.length());
    }

    if (reader.joinable()) {
        reader.join();
    }

    close(socket_fd);
    return 0;
}
