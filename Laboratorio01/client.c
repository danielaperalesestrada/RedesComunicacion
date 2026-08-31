#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    struct sockaddr_in stSockAddr;
    int SocketFD;
    int Res;
    int n;
    char buffer[256];

    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (SocketFD == -1) {
        perror("Cannot create socket");
        exit(EXIT_FAILURE);
    }

    memset(&stSockAddr, 0, sizeof(struct sockaddr_in));

    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(atoi(argv[2])); // Convierte  el puerto a entero y luego a formato de red
    Res = inet_pton(AF_INET, argv[1], &stSockAddr.sin_addr); // Convierte la dirección IP de cadena a binario

    /*
    stSockAddr.sin_port = htons(45000);
    Res = inet_pton(AF_INET, "127.0.0.1", &stSockAddr.sin_addr);
    */

    if (Res <= 0) {
        perror("Invalid IP address");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    if (connect(SocketFD,(const struct sockaddr *)&stSockAddr,sizeof(struct sockaddr_in)) == -1) {
        perror("Connect failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    printf("Connected to %s:%s\n", argv[1], argv[2]);

    while (1) {
        printf("Data to be sent: ");

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }

        n = write(SocketFD, buffer, strlen(buffer));

        if (n < 0) {
            perror("Error writing to socket");
            break;
        }

        if (strcmp(buffer, "END\n") == 0 ||
        strcmp(buffer, "END") == 0) {
            printf("Closing connection...\n");
            break;
        }

        memset(buffer, 0, sizeof(buffer));

        n = read(SocketFD, buffer, sizeof(buffer) - 1);

        if (n < 0) {
            perror("Error reading from socket");
            break;
        }

        if (n == 0) {
            printf("Server disconnected.\n");
            break;
        }

        buffer[n] = '\0';

        printf("Server response: %s", buffer);

        if (strcmp(buffer, "END\n") == 0 ||
        strcmp(buffer, "END") == 0) {
            printf("Server closed the connection.\n");
            break;
        }
    }

    shutdown(SocketFD, SHUT_RDWR);
    close(SocketFD);

    return 0;
}
