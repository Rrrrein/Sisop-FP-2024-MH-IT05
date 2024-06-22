#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/rand.h>
#include <openssl/evp.h>

#define MAX 1024
#define PORT 8080
#define SA struct sockaddr
#define SALT_SIZE 16
#define HASH_SIZE 32
#define ITERATIONS 10000

// Function prototypes
int register_user(int sockfd, const char *username, const char *password);
int login_user(int sockfd, const char *username, const char *password);
void create_channel(int sockfd, const char *channel_name, const char *key);
void list_channels(int sockfd);
void join_channel(int sockfd, const char *channel_name, const char *key);
void join_room(int sockfd, const char *room_name);
void send_chat(int sockfd, const char *message);
void see_chat(int sockfd);
void edit_chat(int sockfd, int id, const char *new_message);
void del_chat(int sockfd, int id);

void generate_salt(unsigned char *salt);
void hash_password(const char *password, const unsigned char *salt, unsigned char *hashed_password);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [<args>]\n", argv[0]);
        exit(1);
    }

    int sockfd;
    struct sockaddr_in servaddr;

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket creation failed");
        exit(1);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    // Connect to server
    if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) != 0) {
        perror("connection with the server failed");
        exit(1);
    }

    if (strcmp(argv[1], "REGISTER") == 0) {
        if (argc != 5 || strcmp(argv[3], "-p") != 0) {
            fprintf(stderr, "Usage: %s REGISTER <username> -p <password>\n", argv[0]);
            exit(1);
        }
        if (register_user(sockfd, argv[2], argv[4])) {
            printf("%s berhasil register\n", argv[2]);
        } else {
            printf("%s sudah terdaftar\n", argv[2]);
        }
    } else if (strcmp(argv[1], "LOGIN") == 0) {
        if (argc != 5 || strcmp(argv[3], "-p") != 0) {
            fprintf(stderr, "Usage: %s LOGIN <username> -p <password>\n", argv[0]);
            exit(1);
        }
        if (login_user(sockfd, argv[2], argv[4])) {
            printf("%s berhasil login\n", argv[2]);
            char command[MAX];
            printf("[%s] ", argv[2]);
            while (fgets(command, sizeof(command), stdin)) {
                command[strcspn(command, "\n")] = 0; // Remove trailing newline

                // Handle user commands here
                if (strncmp(command, "CREATE CHANNEL", 14) == 0) {
                    char *channel_name = strtok(command + 15, " ");
                    char *key = strtok(NULL, " ");
                    create_channel(sockfd, channel_name, key);
                } else if (strncmp(command, "LIST CHANNELS", 13) == 0) {
                    list_channels(sockfd);
                } else if (strncmp(command, "JOIN CHANNEL", 12) == 0) {
                    char *channel_name = strtok(command + 13, " ");
                    char *key = strtok(NULL, " ");
                    join_channel(sockfd, channel_name, key);
                }
                printf("[%s] ", argv[2]);
            }
        } else {
            printf("Login gagal\n");
        }
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        exit(1);
    }

    close(sockfd);
    return 0;
}

void create_channel(int sockfd, const char *channel_name, const char *key) {
    char buffer[MAX];
    snprintf(buffer, sizeof(buffer), "CREATE CHANNEL %s %s", channel_name, key);
    write(sockfd, buffer, strlen(buffer));
    int n = read(sockfd, buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';
    printf("Server response: %s\n", buffer);
}

void list_channels(int sockfd) {
    char buffer[MAX];
    snprintf(buffer, sizeof(buffer), "LIST CHANNELS");
    write(sockfd, buffer, strlen(buffer));
    int n = read(sockfd, buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';
    printf("Server response: %s\n", buffer);
}

void join_channel(int sockfd, const char *channel_name, const char *key) {
    char buffer[MAX];
    snprintf(buffer, sizeof(buffer), "JOIN CHANNEL %s %s", channel_name, key);
    write(sockfd, buffer, strlen(buffer));
    int n = read(sockfd, buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';
    printf("Server response: %s\n", buffer);
}

// Implement other functions (join_room, send_chat, etc.) here

int register_user(int sockfd, const char *username, const char *password) {
    char buffer[MAX];
    snprintf(buffer, sizeof(buffer), "REGISTER %s %s", username, password);
    write(sockfd, buffer, strlen(buffer));
    int n = read(sockfd, buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';
    return strcmp(buffer, "REGISTER SUCCESS") == 0;
}

int login_user(int sockfd, const char *username, const char *password) {
    char buffer[MAX];
    snprintf(buffer, sizeof(buffer), "LOGIN %s %s", username, password);
    write(sockfd, buffer, strlen(buffer));
    int n = read(sockfd, buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';
    return strcmp(buffer, "LOGIN SUCCESS") == 0;
}
