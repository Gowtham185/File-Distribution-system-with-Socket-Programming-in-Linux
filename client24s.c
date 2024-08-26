#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>

//port number and server ip
#define PORT 30720
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"

void error(const char *msg) {
    perror(msg);
    exit(1);
}

// function to expand ~ to the user's home directory since all comannds starts with "~"
void expand_path(const char *input_path, char *expanded_path) {
    if (strncmp(input_path, "~smain", 6) == 0) {
        snprintf(expanded_path, BUFFER_SIZE, "/home/baskarg/smain%s", input_path + 6);
    } else if (input_path[0] == '~') {
        const char *home = "/home/baskarg/";
        if (!home) {
            home = getpwuid(getuid())->pw_dir;
        }
        snprintf(expanded_path, BUFFER_SIZE, "%s%s", home, input_path + 1);
    } else {
        strcpy(expanded_path, input_path);
    }
}
 //function to check if directory exists, if not create them
void ensure_directory_exists(const char *directory_path) {
    struct stat st = {0};

    if (stat(directory_path, &st) == -1) {
        if (mkdir(directory_path, 0700) != 0) {
            error("ERROR creating directory");
        }
    }

}
//function for "ufile" command
void upload_file(int sockfd, const char *filename, const char *destination_path) {
    char buffer[BUFFER_SIZE];
    char expanded_path[BUFFER_SIZE];

    expand_path(destination_path, expanded_path);

    // Ensure the smain directory exists
    ensure_directory_exists("/home/baskarg/smain");
    //preparing the upload command with file name and expanded path
    sprintf(buffer, "upload %s %s", filename, expanded_path);
    write(sockfd, buffer, strlen(buffer));

    // open the file to be uplaoded
    int file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) error("ERROR opening file");
    ssize_t n;
    while ((n = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(sockfd, buffer, n) < 0) error("ERROR writing to socket");
    }
    close(file_fd);
}

//function for "dfile" command
void download_file(int sockfd, const char *filename) {
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    char expanded_path[BUFFER_SIZE];
    FILE *fp;
    ssize_t bytes_read;

    expand_path(filename, expanded_path);

    // Send the download command to the smain with expanded path
    snprintf(command, sizeof(command), "download %s", expanded_path);
    if (write(sockfd, command, strlen(command)) < 0) {
        perror("ERROR writing to server");
        return;
    }

    // Extract just the file name from the full path
    const char *base_name = strrchr(filename, '/');
    if (base_name) {
        base_name++;  // Move past the last '/'
    } else {
 base_name = filename;
    }

    // Open the file for writing the downloaded content in the current working directory
    fp = fopen(base_name, "wb");
    if (fp == NULL) {
        perror("ERROR opening file for writing");
        return;
    }

    // Read the data from the server and write it to the file
    while ((bytes_read = read(sockfd, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, sizeof(char), bytes_read, fp);
    }

    if (bytes_read < 0) {
        perror("ERROR reading from server");
    } else {
        printf("Download of '%s' completed successfully.\n", base_name);
    }

    fclose(fp);
}

//function for "rmfile" command
void remove_file(int sockfd, const char *filename) {
    char buffer[BUFFER_SIZE];
    char expanded_path[BUFFER_SIZE];

    // Expand the path before using it
    expand_path(filename, expanded_path);

    sprintf(buffer, "remove %s", expanded_path);
    write(sockfd, buffer, strlen(buffer));
}

//function for "dtar" command
void download_tar(int sockfd, const char *filetype) {
    char buffer[BUFFER_SIZE];
    char tar_filename[BUFFER_SIZE];

    // Send the tar command to Smain
    sprintf(buffer, "tar %s", filetype);
    if (write(sockfd, buffer, strlen(buffer)) < 0) {
        perror("ERROR writing to socket");
        return;
 }

    // Read the filename of the tar file from Smain
    ssize_t n = read(sockfd, tar_filename, BUFFER_SIZE);
    tar_filename[n] = '\0';

    // Create a file with the received tar filename
    int file_fd = open(tar_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("ERROR opening file to save tar");
        return;
    }

    // Receive the tar file contents and write to the file
    while ((n = read(sockfd, buffer, BUFFER_SIZE)) > 0) {
        if (write(file_fd, buffer, n) < 0) {
            perror("ERROR writing to file");
            close(file_fd);
            return;
        }
    }
    close(file_fd);
}

//function for "display" command
void display_files(int sockfd, const char *pathname) {
    char buffer[BUFFER_SIZE];
    char expanded_path[BUFFER_SIZE];

    // Expanding the path before sending
    expand_path(pathname, expanded_path);

    sprintf(buffer, "display %s", expanded_path);
    write(sockfd, buffer, strlen(buffer));

    // Read and print the list of files received from smain
    while ((read(sockfd, buffer, BUFFER_SIZE)) > 0) {
        printf("%s", buffer);
    }
}

int main() {
    int sockfd;
    struct sockaddr_in serv_addr;
    char command[BUFFER_SIZE];
    char arg1[BUFFER_SIZE], arg2[BUFFER_SIZE];

    while (1) {
        //create the socket and connect to Smain for each command
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            error("ERROR opening socket");
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

        if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            error("ERROR connecting");
            close(sockfd);
            continue;
        }

        printf("client24s$ ");
        fgets(command, sizeof(command), stdin);

        if (sscanf(command, "ufile %s %s", arg1, arg2) == 2) {
            upload_file(sockfd, arg1, arg2);
        } else if (sscanf(command, "dfile %s", arg1) == 1) {
            download_file(sockfd, arg1);
        } else if (sscanf(command, "rmfile %s", arg1) == 1) {
            remove_file(sockfd, arg1);
        } else if (sscanf(command, "dtar %s", arg1) == 1) {
            download_tar(sockfd, arg1);
        } else if (sscanf(command, "display %s", arg1) == 1) {
            display_files(sockfd, arg1);
        } else {
            printf("Invalid command\n");
        }

        // Close the socket after each command
        close(sockfd);
    }

    return 0;
}