#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
#endif

int get_base_path(char* buffer, int buffer_size) {
    #ifdef _WIN32
    int bytes = GetModuleFileNameA(NULL, buffer, buffer_size);
    #else
    int bytes = readlink("/proc/self/exe", buffer, buffer_size);
    #endif

    for (int i = bytes-1; i >= 0; --i) {
        if (buffer[i] == '/' ||
            buffer[i] == '\\') {
            buffer[i+1] = '\0';
            bytes = i+1;
            break;
        }
    }

    #ifdef _WIN32
    for (int i = 0; i < bytes; ++i) {
        if (buffer[i] == '\\') {
            buffer[i] = '/';
        }
    }
    #endif

    return bytes;
}

int close_socket(unsigned long long socket) {
    #ifdef _WIN32
    return closesocket(socket);
    #else
    return close(socket);
    #endif
}

void cleanup_server(unsigned long long server_fd) {
    close_socket(server_fd);
    #ifdef _WIN32
    WSACleanup();
    #endif
    exit(EXIT_FAILURE);
}

void send_chunk(unsigned long long socket, const char* data, size_t data_size) {
    char chunk_header[16] = {0};
    int chunk_header_size = snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", data_size);
    if (send(socket, chunk_header, chunk_header_size, 0) == -1) {
        perror("Failed to send size of a chunk");
        return;
    }
    if (send(socket, data, (int)data_size, 0) == -1) {
        perror("Failed to send image bytes");
        return;
    }

    const char chunk_end[] = "\r\n";
    if (send(socket, chunk_end, sizeof(chunk_end)-1, 0) == -1) {
        perror("Failed to send chunk end");
        return;
    }
}

void send_file_chunked(unsigned long long socket, const char* content_type, int content_type_size, const char* file_path) {
    FILE* file_fd = fopen(file_path, "rb");
    if (!file_fd) {
        perror("Failed to open file file");
        return;
    }

    char response_header[] = 
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Type: ";
    char response_header_tail[] = 
        "\r\n"
        "\r\n";
    
    if (send(socket, response_header, sizeof(response_header)-1, 0) == -1 ||
        send(socket, content_type, content_type_size, 0) == -1 ||
        send(socket, response_header_tail, sizeof(response_header_tail)-1, 0) == -1) {
        perror("Failed to send response header");
    } else {
        enum {file_buffer_size = 1024};
        char file_buffer[file_buffer_size] = {0};

        size_t read_bytes = {0};
        while ((read_bytes = fread(file_buffer, sizeof(char), sizeof(file_buffer), file_fd)) > 0) {
            send_chunk(socket, file_buffer, read_bytes);
        }

        const char last_chunk[] = "0\r\n\r\n";
        send_chunk(socket, last_chunk, sizeof(last_chunk) - 1);
        if (send(socket, last_chunk, sizeof(last_chunk)-1, 0) == -1) {
            perror("Failed to send file bytes");
        }
    }

    fclose(file_fd);
    return;
}

void handle_request(unsigned long long socket, const char* base_path, int base_path_size) {
    enum {buffer_size = 256};
    char request_buffer[buffer_size] = {0};

    const int recv_bytes = recv(socket, request_buffer, sizeof(request_buffer), 0);
    if (recv_bytes == -1) {
        perror("Failed to recv data");
        return;
    }

    enum {path_buffer_size = 256};
    char path_buffer[path_buffer_size] = {0};
    memcpy(path_buffer, base_path, base_path_size);

    const char get_request[] = "GET";

    if (strncmp(request_buffer, get_request, sizeof(get_request)-1) != 0) {
        const char response[] =
            "HTTP/1.1 404 OK\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<h1>Unexpected request type</h1>";
        send(socket, response, sizeof(response), 0);
        return;
    }
    
    const char root_path[] = "/ ";
    const char index_path[] = "/index.html";

    if (strncmp(request_buffer + sizeof(get_request), root_path, sizeof(root_path)-1) == 0 ||
        strncmp(request_buffer + sizeof(get_request), index_path, sizeof(index_path)-1) == 0) {
        const char response[] = "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<h1>Hello from index.html page!</h1>";
        send(socket, response, sizeof(response), 0);
        return;
    }

    const char text_path[] = "/text";
    
    if (strncmp(request_buffer + sizeof(get_request), text_path, sizeof(text_path)-1) == 0) {
        const char content_type[] = "text/plain";
        const char relative_path[] = "text/readme.txt";
        strcat(path_buffer, relative_path);
        send_file_chunked(socket, content_type, sizeof(content_type)-1, path_buffer);
        return;
    }

    const char img_path[] = "/img";
    
    if (strncmp(request_buffer + sizeof(get_request), img_path, sizeof(img_path)-1) == 0) {
        const char content_type[] = "image/png";
        const char relative_path[] = "img/grassland_preview.png";
        strcat(path_buffer, relative_path);
        send_file_chunked(socket, content_type, sizeof(content_type)-1, path_buffer);
        return;
    }

    const char book_path[] = "/book";

    if (strncmp(request_buffer + sizeof(get_request), book_path, sizeof(book_path)-1) == 0) {
        const char content_type[] = "application/pdf";
        const char relative_path[] = "book/Demidovich-Sb_Zad_po_Matanu.pdf";
        strcat(path_buffer, relative_path);
        send_file_chunked(socket, content_type, sizeof(content_type)-1, path_buffer);
        return;
    }

    const char json_path[] = "/json";

    if (strncmp(request_buffer + sizeof(get_request), json_path, sizeof(json_path)-1) == 0) {
        const char content_type[] = "application/json";
        const char relative_path[] = "json/New_document.json";
        strcat(path_buffer, relative_path);
        send_file_chunked(socket, content_type, sizeof(content_type)-1, path_buffer);
        return;
    }

    const char response[] =
            "HTTP/1.1 404 OK\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<h1>Welcome to the other side.. side of 404..</h1>";
    send(socket, response, sizeof(response), 0);
    return;
}

int main(void/*int argc, char** argv*/) {
    enum {base_path_size = 256};
    char base_path[base_path_size] = {0};
    int base_path_length = get_base_path(base_path, base_path_size);
    if (base_path_length <= 0) {
        return EXIT_FAILURE;
    }

    #ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif

    enum {port = 80};

    int opt = 1;

    unsigned long long server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (!server_fd) {
        perror("Failed to get a socket\n");
        cleanup_server(server_fd);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt))) {
        perror("Failed to set socket options\n");
        cleanup_server(server_fd);
    }

    struct sockaddr_in address = {0};
    int addrlen = sizeof(address);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((unsigned short)port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Failed to bind a socket\n");
        cleanup_server(server_fd);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Failed to set a socket to listening a port\n");
        cleanup_server(server_fd);
    }

    while (1) {
        unsigned long long client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        
        if (client_socket == (unsigned long long)(-1)) {
            perror("Failed to accept a new connection\n");
            cleanup_server(server_fd);
        }

        handle_request(client_socket, base_path, base_path_length);
        close_socket(client_socket);
    }

    close_socket(server_fd);
    #ifdef _WIN32
    WSACleanup();
    #endif

    return 0;
}
