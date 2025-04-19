#include <stdio.h>
#include <stdbool.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define DEFAULT_PORT "80"
#define MAX_URL_LENGTH 256
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

bool parse_client_request(int   connfd, 
                          char *host, 
                          char *port, 
                          char *content,
                          char *other_headers)
{
    size_t n; 
    char buf[MAXLINE]; 

    rio_t rio;

    Rio_readinitb(&rio, connfd);
    if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        
        printf("[INFO]: server received %d bytes: %s\n", (int)n, buf);

        const char* get_pos = strstr(buf, "GET");
        if (get_pos == NULL) {
            return false;
        }
        const char* url_start = strstr(buf, "http");
        if (url_start == NULL) {
            return false;
        }
        // Find the end of URL (before " HTTP/*")
        const char* url_end = strstr(url_start, " HTTP");
        if (url_end == NULL) {
            return false;
        }
        // Calculate URL length
        size_t url_len = strlen(url_start) - strlen(url_end);
        
        if (url_len > MAX_URL_LENGTH) {
            return false;
        }
        
        char url[MAX_URL_LENGTH];
        strncpy(url, url_start, url_len);
        url[url_len] = '\0';

        size_t url_trim_start = url_start + 7;
        char url_trim[MAX_URL_LENGTH];
        size_t url_trim_len = strlen(url_trim_start) - strlen(url_end);
        strncpy(url_trim, url_trim_start, url_trim_len);
        url_trim[url_trim_len] = '\0';

        // Parse host, port, and content from the URL
        const char* host_end = strchr(url_trim, '/'); // Find first '/' after host
        const char* port_colon = strchr(url_trim, ':'); // Find port colon if exists
        
        // Case 1: Port is specified (colon exists before first slash)
        if (port_colon != NULL && (host_end == NULL || strlen(port_colon) > strlen(host_end))) {
            // Extract host (between start and colon)
            size_t host_len = strlen(url_trim) - strlen(port_colon);
            strncpy(host, url_trim, host_len);
            host[host_len] = '\0';

            // Extract port (between colon and slash or end)
            size_t port_len = strlen(port_colon) - strlen(host_end) - 1;
            strncpy(port, port_colon + 1, port_len);
            port[port_len] = '\0';

            // Extract content (after slash if exists)
            if (host_end) {
                strcpy(content, host_end);
            } else {
                strcpy(content, "/");
            }
        }
        // Case 2: No port specified
        else {
            if (host_end) {
                // Copy host part
                size_t host_len = strlen(url_trim) - strlen(host_end);
                strncpy(host, url_trim, host_len);
                host[host_len] = '\0';
                
                // Copy content
                strcpy(content, host_end);
            } else {
                // Entire URL is host, content is "/"
                strcpy(host, url_trim);
                strcpy(content, "/");
            }
            
            // Set default port
            strcpy(port, DEFAULT_PORT);
        }
        
        int cx = 0;
        while (true) {
            n = Rio_readlineb(&rio, buf, MAXLINE);
            if (strcmp(buf, "\r\n")) {
                break;
            }
            if (strstr("Host", buf) != NULL) {
                continue;
            }
            if (strstr("User-Agent", buf) != NULL) {
                continue;
            }
            if (strstr("Connection", buf) != NULL) {
                continue;
            }
            if (strstr("Proxy-Connection", buf) != NULL) {
                continue;
            }
            if (n > 0) {
                cx += snprintf(other_headers + cx, MAXLINE - cx, buf);
            }
        }
        return true;
    }
    return false;
}

void send_proxy_request(rio_t      *rp_proxy_server,
                        int         proxy_server_fd,
                        const char *proxy_request)
{
    Rio_readinitb(rp_proxy_server, proxy_server_fd);
    Rio_writen(proxy_server_fd, proxy_request, strlen(proxy_request));
    printf("[INFO]: proxy request sent\n%s\n", proxy_request);
}

void process_server_response(rio_t *rp_proxy_server,
                             int    client_proxy_fd) 
{
    int n_bytes;
    char server_response[MAX_OBJECT_SIZE];
    
    n_bytes = Rio_readnb(rp_proxy_server, server_response, MAX_OBJECT_SIZE);
    printf("[INFO]: proxy received %d bytes from server\n", (int)n_bytes);
    Rio_writen(client_proxy_fd, server_response, n_bytes);
    printf("[INFO]: proxy sent %d bytes back to client\n", (int)n_bytes);
}

void generate_proxy_request(char       *proxy_request, 
                            const char *server_content, 
                            const char *server_hostname) 
{
    int cx;
    cx  = snprintf(proxy_request,      MAXLINE,      "GET %s HTTP/1.0\r\n", server_content);
    cx += snprintf(proxy_request + cx, MAXLINE - cx, "Host: %s\r\n", server_hostname);
    cx += snprintf(proxy_request + cx, MAXLINE - cx, user_agent_hdr);
    cx += snprintf(proxy_request + cx, MAXLINE - cx, "Connection: close\r\n");
    cx += snprintf(proxy_request + cx, MAXLINE - cx, "Proxy-Connection: close\r\n");
    cx += snprintf(proxy_request + cx, MAXLINE - cx, "\r\n");
}

int main(int argc, char **argv)
{ 
    if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    // listen for incoming connections on a port number
    int proxy_listenfd, client_proxy_fd, proxy_server_fd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    char server_hostname[MAXLINE], server_port[MAXLINE];
    char request_content[MAXLINE], proxy_request[MAXLINE];
    char other_headers[MAXLINE];
    
    rio_t rio_proxy_server;
    proxy_listenfd = Open_listenfd(argv[1]);
    while (true) {
        // -- connect with client
        clientlen = sizeof(struct sockaddr_storage); 
        client_proxy_fd = Accept(proxy_listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);
        printf("[INFO]: Connected to (%s, %s)\n", client_hostname, client_port);

        // -- parse the requests from client
        if(parse_client_request(client_proxy_fd, server_hostname, 
                                server_port, request_content, 
                                other_headers)) {
            
            generate_proxy_request(proxy_request, request_content, server_hostname);
            proxy_server_fd = Open_clientfd(server_hostname, server_port);
            send_proxy_request(&rio_proxy_server, proxy_server_fd, proxy_request);
            process_server_response(&rio_proxy_server, client_proxy_fd);

            Close(proxy_server_fd);
            Close(client_proxy_fd);
        } else {
            printf("[WARNING]: request format error\n");
        }
    }
    Close(proxy_listenfd);
    exit(0);
}
