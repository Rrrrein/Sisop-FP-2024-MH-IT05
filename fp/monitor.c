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

int login_user(int sockfd, const char *username, const char *password);
void display_chat(int sockfd, const char *channel, const char *room);

int main(int argc, char *argv[]) {
    if (argc != 6 || strcmp(argv[1], "LOGIN") != 0 || strcmp(argv[3], "-p") != 0) {
        fprintf(stderr, "Usage: %s LOGIN <username> -p <password> -channel <channel> -room <room>\n", argv[0]);
        exit(1);
    }

    const char *username = argv[2];
    const char *password = argv[4];
    const char *channel = argv[5];
    const char *room = argv[6];

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

    if (login_user(sockfd, username, password)) {
        display_chat(sockfd, channel, room);
    } else {
        fprintf(stderr, "Login failed\n");
    }

    close(sockfd);
    return 0;
}

int login_user(int sockfd, const char *username, const char *password) {
    char buffer[MAX];
    snprintf(buffer, sizeof(buffer), "LOGIN %s %s", username, password);
    write(sockfd, buffer, strlen(buffer));
    read(sockfd, buffer, sizeof(buffer));
    return strcmp(buffer, "LOGIN SUCCESS") == 0;
}

void display_chat(int sockfd, const char *channel, const char *room) {
    char buffer[MAX];
    snprintf(buffer, sizeof(buffer), "DISPLAY CHAT %s %s", channel, room);
    write(sockfd, buffer, strlen(buffer));

    while (1) {
        int n = read(sockfd, buffer, sizeof(buffer));
        if (n > 0) {
            buffer[n] = '\0';
            printf("%s\n", buffer);
        }
    }
}

void generate_salt(unsigned char *salt) {
    if (RAND_bytes(salt, SALT_SIZE) != 1) {
        fprintf(stderr, "Failed to generate salt\n");
        exit(1);
    }
}

void hash_password(const char *password, const unsigned char *salt, unsigned char *hashed_password) {
    if (PKCS5_PBKDF2_HMAC(password, strlen(password), salt, SALT_SIZE, ITERATIONS, EVP_sha256(), HASH_SIZE, hashed_password) != 1) {
        fprintf(stderr, "Failed to hash password\n");
        exit(1);
    }
    
}

int verify_password(const char *password, const unsigned char *salt, const unsigned char *hashed_password) {
    unsigned char hashed_input[HASH_SIZE];
    hash_password(password, salt, hashed_input);
    return memcmp(hashed_password, hashed_input, HASH_SIZE) == 0;
}
