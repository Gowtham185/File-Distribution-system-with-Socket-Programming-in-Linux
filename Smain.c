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
#include <dirent.h>
#include <libgen.h>

//defining port number for smain, stext and spdf servers and defining the directories
#define PORT 30720
#define SM_MAIN_DIR "/home/baskarg/smain"
#define SPDF_DIR "/home/baskarg/spdf"
#define STEXT_DIR "/home/baskarg/stext" 
#define BUFFER_SIZE 1024
#define SPDF_PORT 42424
#define STEXT_PORT 32323

void error(const char *msg) {
    perror(msg);
    exit(1);
}

//function to send command to stext and spdf servers if the command has file type .txt or .pdf
void send_to_server(const char *server_ip, int server_port, const char *command) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
 // creating a connecting socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) error("ERROR opening socket");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        error("ERROR connecting to server");

    write(sock, command, strlen(command));
    read(sock, buffer, BUFFER_SIZE);
    close(sock);
}
//connection socket function for the download function
int connect_to_server(const char *server_ip, int server_port) {
    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("ERROR opening socket");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("ERROR invalid server IP address");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR connecting to server");
        close(sock);
        return -1;
    }

    return sock;
}

//function to check if the directory used in the command exists if not then create the directory
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
            mkdir(tmp, 0700);
 *p = '/';
        }
    }
    mkdir(tmp, 0700);
}

//function for "ufile" command
void handle_upload(int client_sock, const char *filename, const char *destination_path) {
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    char file_type[5];
    char server_ip[] = "127.0.0.1"; //server ip is set to this for all servers since we are running it on the same machine
    int server_port = 0;
    char *relative_path;

    //file extension extraction
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return;

    strncpy(file_type, dot, sizeof(file_type) - 1);
    file_type[sizeof(file_type) - 1] = '\0';

    // calculate the relative path from Smain directory
    if (strncmp(destination_path, SM_MAIN_DIR, strlen(SM_MAIN_DIR)) == 0) {
        relative_path = destination_path + strlen(SM_MAIN_DIR);
    } else {
        relative_path = "";  // fallback in case of an unexpected path
    }

    if (strcmp(file_type, ".c") == 0) {
        //checking the directory exists, including any intermediate folders
        ensure_directory_exists(destination_path);

        //save file locally in Smain
        snprintf(buffer, sizeof(buffer), "cp %s %s/%s", filename, destination_path, filename);
        system(buffer);
    } else if (strcmp(file_type, ".pdf") == 0) {
        server_port = SPDF_PORT; //sending the command to send_to_server with port number as text port
        snprintf(command, sizeof(command), "upload %s %s%s", filename, SPDF_DIR, relative_path);
        send_to_server(server_ip, server_port, command);
    } else if (strcmp(file_type, ".txt") == 0) {
        server_port = STEXT_PORT;//sending pdf port as port number
        snprintf(command, sizeof(command), "upload %s %s%s", filename, STEXT_DIR, relative_path);
        send_to_server(server_ip, server_port, command);
    }

 write(client_sock, "Upload successful\n", 18);
}

//function for "dfile" command
void handle_download(int client_sock, const char *filename) {
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    char file_type[5];
    char server_ip[] = "127.0.0.1";
    int server_port = 0;
    int server_sock;
    ssize_t bytes_read;
    char *relative_path;

    // extract file extension
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        write(client_sock, "ERROR: Invalid file type\n", 25);
        return;
    }

    strncpy(file_type, dot, sizeof(file_type) - 1);
    file_type[sizeof(file_type) - 1] = '\0';

    //relative path from Smain directory
    if (strncmp(filename, SM_MAIN_DIR, strlen(SM_MAIN_DIR)) == 0) {
        relative_path = filename + strlen(SM_MAIN_DIR);
    } else {
        relative_path = filename;  // fallback to the original path if not in Smain directory
    }

    if (strcmp(file_type, ".c") == 0) {
        // handling .c file locally
        FILE *fp = fopen(filename, "rb");
        if (fp == NULL) {
            perror("ERROR opening file for reading");
            write(client_sock, "ERROR opening file\n", 19);
            return;
        }

        // send the file content to the client since client should not know about the existence of stxt and spdf
        while ((bytes_read = fread(buffer, sizeof(char), sizeof(buffer), fp)) > 0) {
            if (write(client_sock, buffer, bytes_read) != bytes_read) {
                perror("ERROR writing to client");
                fclose(fp);
                return;
}
        }

        fclose(fp);

    } else if (strcmp(file_type, ".pdf") == 0) {
        server_port = SPDF_PORT;
        snprintf(command, sizeof(command), "download %s%s", SPDF_DIR, relative_path);
        server_sock = connect_to_server(server_ip, server_port);
        if (server_sock < 0) {
            write(client_sock, "ERROR connecting to Spdf server\n", 32);
            return;
        }
        write(server_sock, command, strlen(command));
        while ((bytes_read = read(server_sock, buffer, sizeof(buffer))) > 0) {
            if (write(client_sock, buffer, bytes_read) != bytes_read) {
                perror("ERROR writing to client");
                close(server_sock);
                return;
            }
        }
        close(server_sock);

    } else if (strcmp(file_type, ".txt") == 0) {
        server_port = STEXT_PORT;
        snprintf(command, sizeof(command), "download %s%s", STEXT_DIR, relative_path);
        server_sock = connect_to_server(server_ip, server_port);
        if (server_sock < 0) {
            write(client_sock, "ERROR connecting to Stext server\n", 32);
            return;
        }
        write(server_sock, command, strlen(command));
        while ((bytes_read = read(server_sock, buffer, sizeof(buffer))) > 0) {
            if (write(client_sock, buffer, bytes_read) != bytes_read) {
                perror("ERROR writing to client");
                close(server_sock);
                return;
            }
        }
        close(server_sock);
    }
}

//function for "rmfile" command
void handle_remove(int client_sock, const char *filename) {
    char buffer[BUFFER_SIZE];
 char command[BUFFER_SIZE];
    char file_type[5];
    char server_ip[] = "127.0.0.1";
    int server_port = 0;
    char *relative_path;

    //extract file extension
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return;

    strncpy(file_type, dot, sizeof(file_type) - 1);
    file_type[sizeof(file_type) - 1] = '\0';

    //relative path from Smain directory
    if (strncmp(filename, SM_MAIN_DIR, strlen(SM_MAIN_DIR)) == 0) {
        relative_path = filename + strlen(SM_MAIN_DIR);
    } else {
        relative_path = "";  // fallback in case of an unexpected path
    }

    if (strcmp(file_type, ".c") == 0) {
        // Remove file locally in Smain
        snprintf(buffer, sizeof(buffer), "rm %s", filename);
        system(buffer);
    } else if (strcmp(file_type, ".pdf") == 0) {
        server_port = SPDF_PORT;
        snprintf(command, sizeof(command), "remove %s%s", SPDF_DIR, relative_path);
        send_to_server(server_ip, server_port, command);
    } else if (strcmp(file_type, ".txt") == 0) {
        server_port = STEXT_PORT;
        snprintf(command, sizeof(command), "remove %s%s", STEXT_DIR, relative_path);
        send_to_server(server_ip, server_port, command);
    }

    write(client_sock, "Remove successful\n", 18);
}

void handle_tar(int client_sock, const char *file_type) {
    char buffer[BUFFER_SIZE];
    char tar_filename[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    char server_ip[] = "127.0.0.1";
    int server_port = 0;

    // Initialize tar_filename to ensure it has a known state
    memset(tar_filename, 0, sizeof(tar_filename));

    // if .txt or pdf forwarding the command to the respective servers
    if (strcmp(file_type, ".txt") == 0) {
        server_port = STEXT_PORT;
        snprintf(command, sizeof(command), "tar .txt");
        request_tar_from_server(server_ip, server_port, command, tar_filename);
    } else if (strcmp(file_type, ".pdf") == 0) {
        server_port = SPDF_PORT;
        snprintf(command, sizeof(command), "tar .pdf");
        request_tar_from_server(server_ip, server_port, command, tar_filename);
    } else if (strcmp(file_type, ".c") == 0) {
        //tar filename for .c files
        snprintf(tar_filename, sizeof(tar_filename), "cfiles.tar");
        // creating the tar file
        snprintf(command, sizeof(command), "tar -cvf /home/baskarg/smain/%s /home/baskarg/smain/*.c", tar_filename);
        system(command);
    }

    //full path for the tar file
    char full_tar_path[BUFFER_SIZE];
    snprintf(full_tar_path, sizeof(full_tar_path), "/home/baskarg/smain/%s", tar_filename);

    // opening the tar file using the full path
    FILE *fp = fopen(full_tar_path, "rb");
    
    // forwarding the tar filename to the client
    write(client_sock, tar_filename, strlen(tar_filename) + 1);

    // forwarding the tar file to the client
    ssize_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (write(client_sock, buffer, bytes_read) != bytes_read) {
            perror("ERROR writing to client");
            fclose(fp);
            return;
        }
    }
    fclose(fp);
}

//function to forward the commands and filename if the commands is .txt or .pdf
void request_tar_from_server(const char *server_ip, int server_port, const char *command, char *tar_filename) {
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    ssize_t n;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("ERROR opening socket");
        return;
    }

    // Setup server address
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        close(sockfd);
        return;
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        return;
    }

    // Send the tar command to the server
    if (write(sockfd, command, strlen(command)) < 0) {
        perror("ERROR writing to socket");
        close(sockfd);
        return;
    }

    // Receive the tar filename from the server
    n = read(sockfd, tar_filename, BUFFER_SIZE);
    if (n <= 0) {
        perror("ERROR reading tar filename from server");
        close(sockfd);
        return;
    }
    tar_filename[n] = '\0'; 


    //receiveing the tar file contents from stext or spdf server
    int file_fd = open(tar_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("ERROR opening file to save tar");
        close(sockfd);
        return;
    }

    // Receive the tar file contents and write to the file
    while ((n = read(sockfd, buffer, BUFFER_SIZE)) > 0) {
        if (write(file_fd, buffer, n) < 0) {
            perror("ERROR writing to file");
            close(file_fd);
            close(sockfd);
            return;
        }
    }

    close(file_fd);
    close(sockfd);
}


// Function to request file lists from Spdf and Stext
void request_file_list(int server_sock, const char *subdir, char *file_list) {
    char buffer[BUFFER_SIZE];
    char *filename;

    // Send display command to the server
    snprintf(buffer, sizeof(buffer), "display %s", subdir);
    write(server_sock, buffer, strlen(buffer));

    // Receive the list of files from the server
    int n;
    while ((n = read(server_sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';

        // Extract and append just the filename, not the full path
 filename = basename(buffer);  // basename extracts the filename from the full path
strcat(file_list, filename);

        if (n > 0 && buffer[n-1] != '\n') {
            strcat(file_list, "\n");
        }
    }
}

// Function for the "display" command
void handle_display(int client_sock, const char *pathname) {
    char file_list[BUFFER_SIZE];
    char server_ip[] = "127.0.0.1";
    char subdir[BUFFER_SIZE];

     // Initialize the file list
    memset(file_list, 0, sizeof(file_list));
    //order is .c , .pdf and .txt
    // Add .c files from the local directory in Smain
    DIR *dir = opendir(pathname);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".c") != NULL) {
                strcat(file_list, entry->d_name);
                strcat(file_list, "\n");
            }
        }
        closedir(dir);
    } else {
        perror("ERROR opening directory in Smain");
    }

    // Extract subdirectory from pathname
    const char *relative_path = pathname + strlen("/home/baskarg/smain");

     // Update the subdir for Spdf server
    snprintf(subdir, sizeof(subdir), "%s%s", SPDF_DIR, relative_path);

    // Request file list from Spdf server
    int spdf_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (spdf_sock < 0) {
        perror("ERROR opening socket");
        return;
    }

 struct sockaddr_in spdf_server_addr;
    memset(&spdf_server_addr, 0, sizeof(spdf_server_addr));
    spdf_server_addr.sin_family = AF_INET;
    spdf_server_addr.sin_port = htons(SPDF_PORT);
    inet_pton(AF_INET, server_ip, &spdf_server_addr.sin_addr);

    if (connect(spdf_sock, (struct sockaddr *) &spdf_server_addr, sizeof(spdf_server_addr)) < 0) {
        perror("ERROR connecting to Spdf server");
        close(spdf_sock);
        return;
    }

    request_file_list(spdf_sock, subdir, file_list);
    close(spdf_sock);


    snprintf(subdir, sizeof(subdir), "%s%s", STEXT_DIR, relative_path);
    // Request file list from Stext server
    int stxt_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (stxt_sock < 0) {
        perror("ERROR opening socket");
        return;
    }

    struct sockaddr_in stxt_server_addr;
    memset(&stxt_server_addr, 0, sizeof(stxt_server_addr));
    stxt_server_addr.sin_family = AF_INET;
    stxt_server_addr.sin_port = htons(STEXT_PORT);
    inet_pton(AF_INET, server_ip, &stxt_server_addr.sin_addr);

    if (connect(stxt_sock, (struct sockaddr *) &stxt_server_addr, sizeof(stxt_server_addr)) < 0) {
        perror("ERROR connecting to Stext server");
        close(stxt_sock);
        return;
    }

    request_file_list(stxt_sock, subdir, file_list);
    close(stxt_sock);

    // Send the consolidated list to the client
    write(client_sock, file_list, strlen(file_list));
}

int main() {
    int sockfd, newsockfd, portno;
socklen_t clilen;
    char buffer[BUFFER_SIZE];
struct sockaddr_in serv_addr, cli_addr;
    int n;
    //listening socket creation
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
 portno = PORT;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    //binding ip and port to socket
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

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
