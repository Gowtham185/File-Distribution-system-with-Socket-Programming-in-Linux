#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

//defining port and directory
#define PORT 42424
#define SPDF_DIR "/home/baskarg/spdf"
#define BUFFER_SIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(1);
}

//  checking if the directory exists
void ensure_directory_exists(const char *directory_path) {
    char tmp[BUFFER_SIZE];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", directory_path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0700); // Create directory if it doesn't exist
            *p = '/';
        }
    }
    mkdir(tmp, 0700); // Create the final directory
}

// Function for "ufile" command
void handle_upload(int client_sock, const char *filename, const char *destination_path) {
 char buffer[BUFFER_SIZE];

    // Ensuring the directory exists
    ensure_directory_exists(destination_path);

    // Saving file locally in Spdf
    snprintf(buffer, sizeof(buffer), "cp %s %s/%s", filename, destination_path, filename);
    system(buffer);

    write(client_sock, "Upload successful\n", 18);
}

// Function for "dfile" command
void handle_download(int client_sock, const char *filename) {
    char buffer[BUFFER_SIZE];
    FILE *fp;
    ssize_t bytes_read;

    printf("Downloading file: %s\n", filename);

    // Open the file for reading
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("ERROR opening file for reading");
        write(client_sock, "ERROR opening file\n", 19);
        return;
    }

    // Read and send the file content to the client
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (write(client_sock, buffer, bytes_read) != bytes_read) {
            perror("ERROR writing to client");
            fclose(fp);
            return;
        }
    }

    if (ferror(fp)) {
        perror("ERROR reading file");
    }

    fclose(fp);

}

// Function for "rmfile" command
void handle_remove(int client_sock, const char *filename) {
    char buffer[BUFFER_SIZE];

    snprintf(buffer, sizeof(buffer), "rm %s", filename);
    system(buffer);

    write(client_sock, "Remove successful\n", 18);
}

// Function for "dtar" command
void handle_tar(int client_sock, const char *file_type) {
    char buffer[BUFFER_SIZE];
    char tar_file_path[BUFFER_SIZE];
    char tar_filename[] = "pdffiles.tar";

    // Create the tar file in the ~/spdf directory
    snprintf(tar_file_path, sizeof(tar_file_path), "%s/%s", SPDF_DIR, tar_filename);
    snprintf(buffer, sizeof(buffer), "tar -cvf %s %s", tar_file_path, SPDF_DIR);
    system(buffer);

    // Send the tar filename to Smain
    write(client_sock, tar_filename, strlen(tar_filename) + 1);  // +1 to include null terminator

    // Open the created tar file to send to Smain
    FILE *fp = fopen(tar_file_path, "rb");
    if (fp == NULL) {
        perror("ERROR opening tar file");
        write(client_sock, "ERROR opening tar file\n", 24);
        return;
    }

    // Send the tar file to Smain
    ssize_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (write(client_sock, buffer, bytes_read) != bytes_read) {
            perror("ERROR writing to Smain");
            fclose(fp);
            return;
        }
    }

 fclose(fp);
}

// Function for "display" command
void handle_display(int client_sock, const char *pathname) {
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];

    // List .pdf files in the specified directory in Spdf
    snprintf(command, sizeof(command), "find %s -type f -name '*.pdf'", pathname);
    FILE *fp = popen(command, "r");
    if (fp == NULL) error("ERROR listing .pdf files");
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        write(client_sock, buffer, strlen(buffer));
    }
    pclose(fp);
}

int main() {
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    //creating listening socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = PORT;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    //binding port and ip address
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    //listening with queue size of 5
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) error("ERROR on accept");

        bzero(buffer, BUFFER_SIZE);
 n = read(newsockfd, buffer, BUFFER_SIZE - 1);
 if (n < 0) error("ERROR reading from socket");

        printf("Received command: %s\n", buffer);

        char command[10], argument1[256], argument2[256];
        sscanf(buffer, "%s %s %s", command, argument1, argument2);

        if (strcmp(command, "upload") == 0) {
            handle_upload(newsockfd, argument1, argument2);
        } else if (strcmp(command, "download") == 0) {
            handle_download(newsockfd, argument1);
        } else if (strcmp(command, "remove") == 0) {
            handle_remove(newsockfd, argument1);
        } else if (strcmp(command, "tar") == 0) {
            handle_tar(newsockfd, argument1);
        } else if (strcmp(command, "display") == 0) {
            handle_display(newsockfd, argument1);
        } else {
            write(newsockfd, "Invalid command\n", 16);
        }

        close(newsockfd);
    }

    close(sockfd);
    return 0;
}
