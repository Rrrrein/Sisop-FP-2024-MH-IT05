#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX 1024
#define PORT 8080
#define SA struct sockaddr
#define USERS_FILE_PATH "/Users/rrrreins/sisop/funproject/DiscorIT_dir/users.csv"
#define CHANNELS_FILE_PATH "/Users/rrrreins/sisop/funproject/DiscorIT_dir/channels.csv"

void *handle_client(void *arg);
void register_user(int connfd, const char *username, const char *password);
void login_user(int connfd, const char *username, const char *password);
void create_channel(int connfd, const char *channel_name, const char *key);
void list_channels(int connfd);
int join_channel(int client_socket, const char *username, const char *channel, const char *key);
int get_next_user_id(FILE *file);

int main() {
    int sockfd, connfd;
    socklen_t len;
    struct sockaddr_in servaddr, cli;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket creation failed");
        exit(1);
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    // Bind socket
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        perror("socket bind failed");
        exit(1);
    }

    // Listen for connections
    if ((listen(sockfd, 5)) != 0) {
        perror("listen failed");
        exit(1);
    }
    printf("Server listening on port %d\n", PORT);

    while (1) {
        len = sizeof(cli);
        connfd = accept(sockfd, (SA*)&cli, &len);
        if (connfd < 0) {
            perror("server accept failed");
            exit(1);
        }
        printf("Client connected\n");

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, (void *)&connfd);
    }

    close(sockfd);
    return 0;
}

void *handle_client(void *arg) {
    int connfd = *(int *)arg;
    char buffer[MAX];
    int n;
    char username[MAX] = "";

    while ((n = read(connfd, buffer, sizeof(buffer))) > 0) {
        buffer[n] = '\0';
        printf("Client: %s\n", buffer);

        // Parse command and handle request
        char *command = strtok(buffer, " ");
        if (strcmp(command, "REGISTER") == 0) {
            char *username = strtok(NULL, " ");
            char *password = strtok(NULL, " ");
            register_user(connfd, username, password);
        } else if (strcmp(command, "LOGIN") == 0) {
            strcpy(username, strtok(NULL, " "));
            char *password = strtok(NULL, " ");
            login_user(connfd, username, password);
        } else if (strcmp(command, "CREATE") == 0 && strcmp(strtok(NULL, " "), "CHANNEL") == 0) {
            char *channel_name = strtok(NULL, " ");
            char *key = strtok(NULL, " ");
            create_channel(connfd, channel_name, key);
        } else if (strcmp(command, "LIST") == 0 && strcmp(strtok(NULL, " "), "CHANNELS") == 0) {
            list_channels(connfd);
        } else if (strcmp(command, "JOIN") == 0 && strcmp(strtok(NULL, " "), "CHANNEL") == 0) {
            char *channel_name = strtok(NULL, " ");
            char *key = NULL; // initialize to NULL, it will be read from client later
            join_channel(connfd, username, channel_name, key);
        }
    }

    close(connfd);
    pthread_exit(NULL);
}

int join_channel(int client_socket, const char *username, const char *channel, const char *key) {
    FILE *file = fopen(CHANNELS_FILE_PATH, "r");
    if (!file) {
        perror("fopen");
        return 0;
    }

    char line[MAX];
    int channel_found = 0;
    int is_user = 0;
    int user_in_auth = 0;
    char user_role[MAX] = "";

    // Ambil role user dari file users.csv
    FILE *user_file = fopen(USERS_FILE_PATH, "r");
    if (!user_file) {
        perror("fopen");
        fclose(file);
        return 0;
    }

    char user_line[MAX];
    while (fgets(user_line, sizeof(user_line), user_file)) {
        char stored_username[MAX], stored_role[MAX];
        sscanf(user_line, "%*d,%[^,],%*[^,],%s", stored_username, stored_role);
        if (strcmp(stored_username, username) == 0) {
            strcpy(user_role, stored_role);
            break;
        }
    }
    fclose(user_file);

    // Case 1: Check if user is ROOT or ADMIN
    if (strcmp(user_role, "ROOT") == 0 || strcmp(user_role, "ADMIN") == 0) {
        channel_found = 1;

        // Add the user to auth.csv as ROOT or ADMIN if not already in auth.csv
        char auth_file_path[MAX];
        snprintf(auth_file_path, sizeof(auth_file_path), "/Users/rrrreins/sisop/funproject/DiscorIT_dir/%s/admin/auth.csv", channel);
        FILE *auth_file = fopen(auth_file_path, "r");
        if (!auth_file) {
            perror("fopen");
            fclose(file);
            return 0;
        }

        char auth_line[MAX];
        while (fgets(auth_line, sizeof(auth_line), auth_file)) {
            char stored_user[MAX];
            sscanf(auth_line, "%*d,%[^,],%*s", stored_user);
            if (strcmp(stored_user, username) == 0) {
                user_in_auth = 1;
                break;
            }
        }
        fclose(auth_file);

        if (!user_in_auth) {
            auth_file = fopen(auth_file_path, "a");
            if (!auth_file) {
                perror("fopen");
                fclose(file);
                return 0;
            }

            // Mengambil ID pengguna
            int user_id = 0;
            user_file = fopen(USERS_FILE_PATH, "r");
            if (user_file) {
                while (fgets(user_line, sizeof(user_line), user_file)) {
                    sscanf(user_line, "%d,%*[^,],%*[^,],%*s", &user_id);
                }
                fclose(user_file);
            }

            fprintf(auth_file, "%d,%s,%s\n", user_id, username, user_role);
            fclose(auth_file);
        }
    } else {
        // Case 2: Check if the user is already in auth.csv of the channel
        char auth_file_path[MAX];
        snprintf(auth_file_path, sizeof(auth_file_path), "/Users/rrrreins/sisop/funproject/DiscorIT_dir/%s/admin/auth.csv", channel);
        FILE *auth_file = fopen(auth_file_path, "r");
        if (!auth_file) {
            perror("fopen");
            fclose(file);
            return 0;
        }

        int user_found_in_auth = 0;
        char auth_line[MAX];
        while (fgets(auth_line, sizeof(auth_line), auth_file)) {
            char stored_user[MAX];
            sscanf(auth_line, "%*d,%[^,],%*s", stored_user);
            if (strcmp(stored_user, username) == 0) {
                user_found_in_auth = 1;
                break;
            }
        }
        fclose(auth_file);

        if (user_found_in_auth) {
            channel_found = 1;
        } else {
            // Check the key if user is not found in auth.csv
            while (fgets(line, sizeof(line), file)) {
                char stored_channel[MAX], stored_key[MAX];
                sscanf(line, "%*d,%[^,],%s", stored_channel, stored_key);

                if (strcmp(stored_channel, channel) == 0) {
                    channel_found = 1;
                    if (key == NULL || strcmp(stored_key, key) != 0) {
                        write(client_socket, "Key: ", strlen("Key: "));
                        char input_key[MAX];
                        int valread = read(client_socket, input_key, MAX);
                        input_key[valread] = '\0'; // Remove newline
                        if (strcmp(input_key, stored_key) != 0) {
                            write(client_socket, "Key salah\n", strlen("Key salah\n"));
                            fclose(file);
                            return 0;
                        }
                    }

                    // Add the user to auth.csv as a regular USER
                    auth_file = fopen(auth_file_path, "a");
                    if (!auth_file) {
                        perror("fopen");
                        fclose(file);
                        return 0;
                    }

                    // Mengambil ID pengguna
                    int user_id = 0;
                    user_file = fopen(USERS_FILE_PATH, "r");
                    if (user_file) {
                        while (fgets(user_line, sizeof(user_line), user_file)) {
                            sscanf(user_line, "%d,%*[^,],%*[^,],%*s", &user_id);
                        }
                        fclose(user_file);
                    }

                    fprintf(auth_file, "%d,%s,USER\n", user_id, username);
                    fclose(auth_file);
                    break;
                }
            }
        }
    }

    fclose(file);

    if (channel_found) {
        char msg[MAX];
        snprintf(msg, sizeof(msg), "[%s/%s] ", username, channel);
        write(client_socket, msg, strlen(msg));

        char activity[MAX];
        snprintf(activity, sizeof(activity), "%s bergabung dengan channel %s", username, channel);
        // log_activity(channel, activity); // Ensure you have this function defined
        return 1;
    } else {
        write(client_socket, "Channel tidak ditemukan\n", strlen("Channel tidak ditemukan\n"));
        return 0;
    }
}

void create_channel(int connfd, const char *channel_name, const char *key) {
    char path[MAX];
    snprintf(path, sizeof(path), "/Users/rrrreins/sisop/funproject/DiscorIT_dir/%s", channel_name);

    if (mkdir(path, 0777) == -1) {
        perror("Failed to create channel directory");
        write(connfd, "CREATE CHANNEL FAILED", strlen("CREATE CHANNEL FAILED"));
        return;
    }

    char admin_path[MAX];
    snprintf(admin_path, sizeof(admin_path), "%s/admin", path);
    mkdir(admin_path, 0777);

    char auth_file[MAX], user_log[MAX];
    snprintf(auth_file, sizeof(auth_file), "%s/auth.csv", admin_path);
    snprintf(user_log, sizeof(user_log), "%s/user.log", admin_path);

    FILE *file;
    file = fopen(auth_file, "w"); fclose(file);
    file = fopen(user_log, "w"); fclose(file);

    // Write to channels.csv
    file = fopen(CHANNELS_FILE_PATH, "a+");
    if (file == NULL) {
        perror("Failed to open channels.csv");
        return;
    }

    int id = get_next_user_id(file);

    fprintf(file, "%d,%s,%s\n", id, channel_name, key);
    fclose(file);

    write(connfd, "CREATE CHANNEL SUCCESS", strlen("CREATE CHANNEL SUCCESS"));
}

void register_user(int connfd, const char *username, const char *password) {
    FILE *file = fopen(USERS_FILE_PATH, "a+");
    if (file == NULL) {
        perror("Failed to open users.csv");
        exit(1);
    }

    // Check if username already exists
    rewind(file);
    char line[MAX];
    while (fgets(line, sizeof(line), file)) {
        char stored_username[MAX];
        sscanf(line, "%*d,%[^,],%*[^,],%*s", stored_username);
        if (strcmp(stored_username, username) == 0) {
            write(connfd, "REGISTER FAILED", strlen("REGISTER FAILED"));
            fclose(file);
            return;
        }
    }

    // Get next user ID
    rewind(file);
    int id = get_next_user_id(file);

    // Determine user role
    const char *role = (id == 1) ? "ROOT" : "USER";

    // Register the new user
    fprintf(file, "%d,%s,%s,%s\n", id, username, password, role);
    write(connfd, "REGISTER SUCCESS", strlen("REGISTER SUCCESS"));

    fclose(file);
}

void login_user(int connfd, const char *username, const char *password) {
    FILE *file = fopen(USERS_FILE_PATH, "r");
    if (file == NULL) {
        perror("Failed to open users.csv");
        exit(1);
    }

    char line[MAX];
    int login_success = 0;  // Flag to check if login is successful
    while (fgets(line, sizeof(line), file)) {
        char stored_username[MAX], stored_password[MAX];
        sscanf(line, "%*d,%[^,],%[^,],%*s", stored_username, stored_password);

        if (strcmp(stored_username, username) == 0 && strcmp(stored_password, password) == 0) {
            write(connfd, "LOGIN SUCCESS", strlen("LOGIN SUCCESS"));
            login_success = 1;
            break;
        }
    }

    if (!login_success) {
        write(connfd, "LOGIN FAILED", strlen("LOGIN FAILED"));
    }

    fclose(file);
}

void list_channels(int connfd) {
    FILE *file = fopen(CHANNELS_FILE_PATH, "r");
    if (file == NULL) {
        perror("Failed to open channels.csv");
        write(connfd, "LIST CHANNELS FAILED", strlen("LIST CHANNELS FAILED"));
        return;
    }

    char line[MAX];
    char channels[MAX] = "";

    while (fgets(line, sizeof(line), file)) {
        char stored_channel[MAX];
        sscanf(line, "%*d,%[^,],%*s", stored_channel);
        strcat(channels, stored_channel);
        strcat(channels, " ");
    }

    fclose(file);

    write(connfd, channels, strlen(channels));
}

int get_next_user_id(FILE *file) {
    int id = 0;
    char line[MAX];

    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%d,", &id);
    }

    return id + 1;
}
