#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 4096
#define MAX_THREADS 100

typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} client_data_t;

int server_fd;
int active_threads = 0;
pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

void* handle_client(void* arg);
void parse_http_request(char* request, char* method, char* path, char* version);
void send_response(int client_fd, char* method, char* path, char* version);
void signal_handler(int signum);
void setup_signal_handlers();

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    pthread_t thread_id;
    
    printf(" Simple Web Server - Tema 2 \n");
    printf("Pornire server pe portul %d...\n\n", PORT);
    
    setup_signal_handlers();
    
    // Creare socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Eroare la crearea socket-ului");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Eroare la setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Configurare adresa server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Eroare la bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, BACKLOG) < 0) {
        perror("Eroare la listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Server pornit cu succes!\n");
    printf("Ascultare pe portul %d...\n", PORT);
    printf("Accesati: http://localhost:%d\n\n", PORT);
    
    // Bucla principala - acceptare conexiuni
    while (1) {
        client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue; // Semnal primit, continuam
            }
            perror("Eroare la accept");
            continue;
        }
        
        // Afisare informatii despre client
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("[CONEXIUNE NOUA] Client conectat: %s:%d\n", 
               client_ip, ntohs(client_addr.sin_port));
        
        // Verificare numar de thread-uri active
        pthread_mutex_lock(&thread_mutex);
        if (active_threads >= MAX_THREADS) {
            pthread_mutex_unlock(&thread_mutex);
            printf("[EROARE] Numar maxim de thread-uri atins. Conexiune refuzata.\n");
            close(client_fd);
            continue;
        }
        active_threads++;
        pthread_mutex_unlock(&thread_mutex);
        
        // Alocare structura pentru datele clientului
        client_data_t* client_data = malloc(sizeof(client_data_t));
        if (client_data == NULL) {
            perror("Eroare la alocare memorie");
            close(client_fd);
            pthread_mutex_lock(&thread_mutex);
            active_threads--;
            pthread_mutex_unlock(&thread_mutex);
            continue;
        }
        
        client_data->client_fd = client_fd;
        client_data->client_addr = client_addr;
        
        // Creare thread pentru procesarea clientului
        if (pthread_create(&thread_id, NULL, handle_client, (void*)client_data) != 0) {
            perror("Eroare la crearea thread-ului");
            close(client_fd);
            free(client_data);
            pthread_mutex_lock(&thread_mutex);
            active_threads--;
            pthread_mutex_unlock(&thread_mutex);
            continue;
        }
        
        pthread_detach(thread_id);
    }
    
    close(server_fd);
    return 0;
}

// Functie pentru procesarea clientului in thread separat
void* handle_client(void* arg) {
    client_data_t* data = (client_data_t*)arg;
    int client_fd = data->client_fd;
    char buffer[BUFFER_SIZE];
    char method[16], path[256], version[16];
    
    // Afisare informatii thread
    printf("[THREAD %lu] Procesare client...\n", pthread_self());
    
    // Citire request HTTP
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_read < 0) {
        perror("Eroare la citirea datelor");
    } else if (bytes_read == 0) {
        printf("[THREAD %lu] Client deconectat.\n", pthread_self());
    } else {
        buffer[bytes_read] = '\0';
        
        printf("\n[THREAD %lu] ===== REQUEST PRIMIT =====\n%s", pthread_self(), buffer);
        printf("[THREAD %lu] ========================\n\n", pthread_self());
        
        parse_http_request(buffer, method, path, version);
        
        send_response(client_fd, method, path, version);
    }
    
    close(client_fd);
    free(data);
    
    pthread_mutex_lock(&thread_mutex);
    active_threads--;
    pthread_mutex_unlock(&thread_mutex);
    
    printf("[THREAD %lu] Thread incheiat. Thread-uri active: %d\n", 
           pthread_self(), active_threads);
    
    return NULL;
}

// Functie pentru parsarea request-ului HTTP
void parse_http_request(char* request, char* method, char* path, char* version) {
    // Initializare cu valori default
    strcpy(method, "UNKNOWN");
    strcpy(path, "/");
    strcpy(version, "HTTP/1.0");
    
    // Parsare prima linie (Request Line)
    char* line_end = strstr(request, "\r\n");
    if (line_end == NULL) {
        line_end = strstr(request, "\n");
    }
    
    if (line_end != NULL) {
        size_t line_len = line_end - request;
        char first_line[BUFFER_SIZE];
        strncpy(first_line, request, line_len);
        first_line[line_len] = '\0';
        
        // Parsare metoda, path si versiune
        char* token = strtok(first_line, " ");
        if (token != NULL) {
            strncpy(method, token, 15);
            method[15] = '\0';
            
            token = strtok(NULL, " ");
            if (token != NULL) {
                strncpy(path, token, 255);
                path[255] = '\0';
                
                token = strtok(NULL, " \r\n");
                if (token != NULL) {
                    strncpy(version, token, 15);
                    version[15] = '\0';
                }
            }
        }
    }
    
    printf("[PARSARE] Metoda: %s, Path: %s, Versiune: %s\n", method, path, version);
}

// Functie pentru trimiterea raspunsului de confirmare
void send_response(int client_fd, char* method, char* path, char* version) {
    char response[BUFFER_SIZE];
    char body[BUFFER_SIZE];
    
    // Creare continut HTML pentru raspuns
    snprintf(body, sizeof(body),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <title>Confirmare Comanda - Simple Web Server</title>\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; margin: 50px; background-color: #f0f0f0; }\n"
        "        .container { background-color: white; padding: 30px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }\n"
        "        h1 { color: #2c3e50; }\n"
        "        .success { color: #27ae60; font-weight: bold; }\n"
        "        .info { background-color: #ecf0f1; padding: 15px; border-left: 4px solid #3498db; margin: 20px 0; }\n"
        "        .param { margin: 10px 0; }\n"
        "        .label { font-weight: bold; color: #34495e; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class=\"container\">\n"
        "        <h1> Simple Web Server - Tema 2</h1>\n"
        "        <p class=\"success\">✓ Comanda HTTP identificata cu succes!</p>\n"
        "        <div class=\"info\">\n"
        "            <div class=\"param\"><span class=\"label\">Metoda HTTP:</span> %s</div>\n"
        "            <div class=\"param\"><span class=\"label\">Resursa solicitata:</span> %s</div>\n"
        "            <div class=\"param\"><span class=\"label\">Versiune protocol:</span> %s</div>\n"
        "        </div>\n"
        "        <p><strong>Status:</strong> Comanda a fost recunoscuta si procesata de server.</p>\n"
        "    </div>\n"
        "</body>\n"
        "</html>",
        method, path, version);
    
    // Creare header HTTP
    snprintf(response, sizeof(response),
        "%s 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %lu\r\n"
        "Connection: close\r\n"
        "Server: SimpleWebServer/1.0\r\n"
        "\r\n"
        "%s",
        version, strlen(body), body);
    
    // Trimitere raspuns
    ssize_t bytes_sent = send(client_fd, response, strlen(response), 0);
    if (bytes_sent < 0) {
        perror("Eroare la trimirea raspunsului");
    } else {
        printf("[RASPUNS] Trimis raspuns de confirmare (%zd bytes)\n", bytes_sent);
    }
}

// Handler pentru semnale
void signal_handler(int signum) {
    printf("\n\n[SEMNAL] Semnal %d primit. Inchidere server...\n", signum);
    close(server_fd);
    exit(0);
}

// Configurare signal handlers
void setup_signal_handlers() {
    signal(SIGINT, signal_handler);   
    signal(SIGTERM, signal_handler);  
}