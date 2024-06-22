#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <openssl/rand.h>
#include <openssl/evp.h>

#define MAX 1024
#define PORT 8080
#define SA struct sockaddr
#define SALT_SIZE 16
#define HASH_SIZE 32
#define ITERATIONS 10000

const char *USERS_FILE_PATH = "/Users/rrrreins/sisop/funproject/DiscorIT_dir/users.csv";
const char *CHANNELS_FILE_PATH = "/Users/rrrreins/sisop/funproject/DiscorIT_dir/channels.csv";

void *handle_client(void *arg);
void register_user(int connfd, const char *username, const char *password);
void login_user(int connfd, const char *username, const char *password);
void create_channel(int connfd, const char *username, const char *channel_name, const char *key);
void list_channels(int connfd, const char *username);
void edit_channel(int connfd, const char *username, const char *old_channel, const char *new_channel);
void delete_channel(int connfd, const char *username, const char *channel_name);
void generate_salt(unsigned char *salt);
void hash_password(const char *password, const unsigned char *salt, unsigned char *hashed_password);
int get_next_user_id(FILE *file);
void log_user_activity(const char *username, const char *activity, const char *channel_name);

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

    while ((n = read(connfd, buffer, sizeof(buffer))) > 0) {
        buffer[n] = '\0';
        printf("Client: %s\n", buffer);

        // Parse command and handle request
        char *command = strtok(buffer, " ");
        char *username = strtok(NULL, " ");
        if (strcmp(command, "REGISTER") == 0) {
            char *password = strtok(NULL, " ");
            register_user(connfd, username, password);
        } else if (strcmp(command, "LOGIN") == 0) {
            char *password = strtok(NULL, " ");
            login_user(connfd, username, password);
        } else if (strcmp(command, "CREATE") == 0 && strcmp(strtok(NULL, " "), "CHANNEL") == 0) {
            char *channel_name = strtok(NULL, " ");
            char *key = strtok(NULL, " ");
            create_channel(connfd, username, channel_name, key);
        } else if (strcmp(command, "LIST") == 0 && strcmp(strtok(NULL, " "), "CHANNELS") == 0) {
            list_channels(connfd, username);
        } else if (strcmp(command, "EDIT") == 0 && strcmp(strtok(NULL, " "), "CHANNEL") == 0) {
            char *old_channel = strtok(NULL, " ");
            strtok(NULL, " "); // Skip "TO"
            char *new_channel = strtok(NULL, " ");
            edit_channel(connfd, username, old_channel, new_channel);
        } else if (strcmp(command, "DEL") == 0 && strcmp(strtok(NULL, " "), "CHANNEL") == 0) {
            char *channel_name = strtok(NULL, " ");
            delete_channel(connfd, username, channel_name);
        }
    }

    close(connfd);
    pthread_exit(NULL);
}

void create_channel(int connfd, const char *username, const char *channel_name, const char *key) {
    char path[MAX];
    snprintf(path, sizeof(path), "/Users/rrrreins/sisop/funproject/DiscorIT_dir/%s", channel_name);

    if (mkdir(path, 0777) == -1) {
        perror("Failed to create channel directory");
        write(connfd, "CREATE CHANNEL FAILED", strlen("CREATE CHANNEL FAILED"));
        return;
    }

    char admin_path[MAX], room1_path[MAX], room2_path[MAX], room3_path[MAX];
    snprintf(admin_path, sizeof(admin_path), "%s/admin", path);
    snprintf(room1_path, sizeof(room1_path), "%s/room1", path);
    snprintf(room2_path, sizeof(room2_path), "%s/room2", path);
    snprintf(room3_path, sizeof(room3_path), "%s/room3", path);

    mkdir(admin_path, 0777);
    mkdir(room1_path, 0777);
    mkdir(room2_path, 0777);
    mkdir(room3_path, 0777);

    char auth_file[MAX], user_log[MAX], chat1_file[MAX], chat2_file[MAX], chat3_file[MAX];
    snprintf(auth_file, sizeof(auth_file), "%s/auth.csv", admin_path);
    snprintf(user_log, sizeof(user_log), "%s/user.log", admin_path);
    snprintf(chat1_file, sizeof(chat1_file), "%s/chat.csv", room1_path);
    snprintf(chat2_file, sizeof(chat2_file), "%s/chat.csv", room2_path);
    snprintf(chat3_file, sizeof(chat3_file), "%s/chat.csv", room3_path);

    FILE *file;
    file = fopen(auth_file, "w"); fclose(file);
    file = fopen(user_log, "w"); fclose(file);
    file = fopen(chat1_file, "w"); fclose(file);
    file = fopen(chat2_file, "w"); fclose(file);
    file = fopen(chat3_file, "w"); fclose(file);

    // Log channel creation in channels.csv
    file = fopen(CHANNELS_FILE_PATH, "a");
    if (file != NULL) {
        fprintf(file, "%s,%s\n", channel_name, key);
        fclose(file);
    }

    // Log user activity in users.log
    char activity[MAX];
    snprintf(activity, sizeof(activity), "CREATE CHANNEL %s -k %s", channel_name, key);
    log_user_activity(username, activity, channel_name);

    write(connfd, "CREATE CHANNEL SUCCESS", strlen("CREATE CHANNEL SUCCESS"));
}

void list_channels(int connfd, const char *username) {
    FILE *file = fopen(CHANNELS_FILE_PATH, "r");
    if (file == NULL) {
        perror("Failed to open channels.csv");
        write(connfd, "LIST CHANNELS FAILED", strlen("LIST CHANNELS FAILED"));
        return;
    }

    char line[MAX];
    while (fgets(line, sizeof(line), file)) {
        write(connfd, line, strlen(line));
    }

    fclose(file);
    log_user_activity(username, "LIST CHANNELS", NULL);
}

void edit_channel(int connfd, const char *username, const char *old_channel, const char *new_channel) {
    FILE *file = fopen(CHANNELS_FILE_PATH, "r+");
    if (file == NULL) {
        perror("Failed to open channels.csv");
        write(connfd, "EDIT CHANNEL FAILED", strlen("EDIT CHANNEL FAILED"));
        return;
    }

    char line[MAX];
    int found = 0;
    long pos;
    while ((pos = ftell(file)) != -1 && fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        if (strcmp(token, old_channel) == 0) {
            found = 1;
            fseek(file, pos, SEEK_SET);
            fprintf(file, "%s,", new_channel);
            break;
        }
    }

    fclose(file);

    if (found) {
        char old_path[MAX], new_path[MAX];
        snprintf(old_path, sizeof(old_path), "/Users/rrrreins/sisop/funproject/DiscorIT_dir/%s", old_channel);
        snprintf(new_path, sizeof(new_path), "/Users/rrrreins/sisop/funproject/DiscorIT_dir/%s", new_channel);
        rename(old_path, new_path);
        char activity[MAX];
        snprintf(activity, sizeof(activity), "EDIT CHANNEL %s TO %s", old_channel, new_channel);
        log_user_activity(username, activity, new_channel);
        write(connfd, "EDIT CHANNEL SUCCESS", strlen("EDIT CHANNEL SUCCESS"));
    } else {
        write(connfd, "EDIT CHANNEL FAILED", strlen("EDIT CHANNEL FAILED"));
    }
}

void delete_channel(int connfd, const char *username, const char *channel_name) {
    FILE *file = fopen(CHANNELS_FILE_PATH, "r");
    FILE *temp_file = fopen("/Users/rrrreins/sisop/funproject/DiscorIT_dir/temp.csv", "w");
    if (file == NULL || temp_file == NULL) {
        perror("Failed to open channels.csv or temp.csv");
        write(connfd, "DELETE CHANNEL FAILED", strlen("DELETE CHANNEL FAILED"));
        return;
    }

    char line[MAX];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        if (strcmp(token, channel_name) != 0) {
            fputs(line, temp_file);
        } else {
            found = 1;
        }
    }

    fclose(file);
    fclose(temp_file);

    remove(CHANNELS_FILE_PATH);
    rename("/Users/rrrreins/sisop/funproject/DiscorIT_dir/temp.csv", CHANNELS_FILE_PATH);

    if (found) {
        char path[MAX];
        snprintf(path, sizeof(path), "/Users/rrrreins/sisop/funproject/DiscorIT_dir/%s", channel_name);
        // Remove the directory and its contents
        char command[MAX];
        snprintf(command, sizeof(command), "rm -rf %s", path);
        system(command);
        char activity[MAX];
        snprintf(activity, sizeof(activity), "DEL CHANNEL %s", channel_name);
        log_user_activity(username, activity, NULL);
        write(connfd, "DELETE CHANNEL SUCCESS", strlen("DELETE CHANNEL SUCCESS"));
    } else {
        write(connfd, "DELETE CHANNEL FAILED", strlen("DELETE CHANNEL FAILED"));
    }
}

void register_user(int connfd, const char *username, const char *password) {
    printf("Registering user: %s\n", username); // Debug log
    unsigned char salt[SALT_SIZE];
    unsigned char hashed_password[HASH_SIZE];

    generate_salt(salt);
    hash_password(password, salt, hashed_password);

    FILE *file = fopen(USERS_FILE_PATH, "a+");
    if (file == NULL) {
        perror("Failed to open users.csv");
        exit(1);
    }

    // Check if username already exists
    rewind(file);
    char line[MAX];
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        token = strtok(NULL, ",");
        printf("Checking existing username: %s\n", token); // Debug log
        if (strcmp(token, username) == 0) {
            write(connfd, "REGISTER FAILED", strlen("REGISTER FAILED"));
            printf("REGISTER FAILED: %s already exists\n", username); // Debug log
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
    fprintf(file, "%d,%s,", id, username);
    for (int i = 0; i < SALT_SIZE; i++) {
        fprintf(file, "%02x", salt[i]);
    }
    fprintf(file, ",");
    for (int i = 0; i < HASH_SIZE; i++) {
        fprintf(file, "%02x", hashed_password[i]);
    }
    fprintf(file, ",%s\n", role);
    write(connfd, "REGISTER SUCCESS", strlen("REGISTER SUCCESS"));
    printf("REGISTER SUCCESS: %s registered successfully\n", username); // Debug log

    fclose(file);
}

void login_user(int connfd, const char *username, const char *password) {
    printf("Logging in user: %s\n", username); // Debug log
    unsigned char salt[SALT_SIZE];
    unsigned char stored_hashed_password[HASH_SIZE];
    unsigned char hashed_password[HASH_SIZE];

    FILE *file = fopen(USERS_FILE_PATH, "r");
    if (file == NULL) {
        perror("Failed to open users.csv");
        exit(1);
    }

    char line[MAX];
    int login_success = 0;  // Flag to check if login is successful
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        int id = atoi(token);
        token = strtok(NULL, ",");
        char *stored_username = token;
        token = strtok(NULL, ",");
        printf("Checking username: %s\n", stored_username); // Debug log

        for (int i = 0; i < SALT_SIZE; i++) {
            unsigned int value;
            sscanf(token + 2 * i, "%02x", &value);
            salt[i] = (unsigned char)value;
        }
        token = strtok(NULL, ",");

        for (int i = 0; i < HASH_SIZE; i++) {
            unsigned int value;
            sscanf(token + 2 * i, "%02x", &value);
            stored_hashed_password[i] = (unsigned char)value;
        }

        if (strcmp(stored_username, username) == 0) {
            hash_password(password, salt, hashed_password);
            if (memcmp(stored_hashed_password, hashed_password, HASH_SIZE) == 0) {
                write(connfd, "LOGIN SUCCESS", strlen("LOGIN SUCCESS"));
                login_success = 1;
                printf("LOGIN SUCCESS: %s logged in successfully\n", username); // Debug log
                break;
            } else {
                printf("Password mismatch for user: %s\n", username); // Debug log
            }
        }
    }

    if (!login_success) {
        write(connfd, "LOGIN FAILED", strlen("LOGIN FAILED"));
        printf("LOGIN FAILED: %s not found or password mismatch\n", username); // Debug log
    }

    fclose(file);
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

int get_next_user_id(FILE *file) {
    int id = 1;
    char line[MAX];
    while (fgets(line, sizeof(line), file)) {
        id++;
    }
    return id;
}

void log_user_activity(const char *username, const char *activity, const char *channel_name) {
    char user_log_path[MAX];
    if (channel_name != NULL) {
        snprintf(user_log_path, sizeof(user_log_path), "/Users/rrrreins/sisop/funproject/DiscorIT_dir/%s/admin/user.log", channel_name);
    } else {
        snprintf(user_log_path, sizeof(user_log_path), "/Users/rrrreins/sisop/funproject/DiscorIT_dir/users.log");
    }

    FILE *file = fopen(user_log_path, "a");
    if (file != NULL) {
        fprintf(file, "[%s] %s\n", username, activity);
        fclose(file);
    }
}
