#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    struct sockaddr_in stSockAddr;
    int SocketFD;
    int n;
    char buffer[256];

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (SocketFD == -1) {
        perror("Cannot create socket");
        exit(EXIT_FAILURE);
    }

    memset(&stSockAddr, 0, sizeof(struct sockaddr_in));

    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(atoi(argv[1]));
    stSockAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(SocketFD,(const struct sockaddr *)&stSockAddr,sizeof(struct sockaddr_in)) == -1) {
        perror("Bind failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    if (listen(SocketFD, 10) == -1) {
        perror("Listen failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %s...\n", argv[1]);

    while (1) {
        int ConnectFD = accept(SocketFD, NULL, NULL);

        if (ConnectFD < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected.\n");

        while (1) {
            memset(buffer, 0, sizeof(buffer));

            n = read(ConnectFD, buffer, sizeof(buffer) - 1);

            if (n < 0) {
                perror("Error reading from socket");
                break;
            }

            if (n == 0) {
                printf("Client disconnected.\n");
                break;
            }

            buffer[n] = '\0';

            printf("Message from client: %s", buffer);

            if (strcmp(buffer, "END\n") == 0 ||
            strcmp(buffer, "END") == 0) {
                printf("Client requested to close the connection.\n");

                write(ConnectFD, "END\n", 4);
                break;
            }

            printf("Response: ");

            char response[256];

            if (fgets(response, sizeof(response), stdin) == NULL) {
                break;
            }

            n = write(ConnectFD,
            response,
            strlen(response));

            if (n < 0) {
                perror("Error writing to socket");
                break;
            }
        }

        shutdown(ConnectFD, SHUT_RDWR);
        close(ConnectFD);

        printf("Connection closed.\n");
    }

    close(SocketFD);

    return 0;
}
