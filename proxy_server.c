/***************************************************************************************************
MIT License

Copyright (c) 2018 Sorabh Gandhi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
****************************************************************************************************/

/**
 * @\file   proxy_server.c
 * @\author Sorabh Gandhi
 * @\brief  This file contains the implementation of web proxy server
 * @\date   11/13/2018
 *
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <error.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/md5.h>

#define MAX_BUF_SIZE 2048
#define t_out 0

/*Function to print error message*/
void print_error(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/*Function to build HTTP OK response for valid HTTP Request*/
void http_ok_resp(char *msg, char *version, ssize_t f_size, char *f_type, char *conn_status)
{
    snprintf(msg, 512, "%s 200 OK\r\n""Content-Type: %s\r\n""Connection: %s\r\n""Content-Length: %ld\r\n\r\n", version, f_type, conn_status, f_size);
}

/*Function to build HTTP Error response for invalid HTTP Request*/
void http_error_resp(char *msg, char *err, char *version, char *conn_status, int c_size)
{
	snprintf(msg, 512, "%s %s\r\n""Content-Type: html\r\n""Connection: %s\r\n""Content-Length: %d\r\n\r\n", version, err, conn_status, c_size);
}

static int isURLpresent(char *filename, char *url)
{
    char buffer[1024];
    FILE *f = fopen(filename, "r");

    while (!feof(f))
    {
        fgets(buffer, 1024, f);
        char *match = strstr(buffer, url);

        if (match != NULL)
            return 1;
    }

    return 0;
}

static char *parsehostname(char *url)
{
	char temp[1024];
	strcpy(temp, url);

    char *host_name = strstr(temp, "//");
    char *domain_name = strtok(&host_name[2], "/");

    if (domain_name != NULL)
        return domain_name;
    else
        return &host_name[2];
}

static int hostname_to_ip(char *hostname, char *ip)
{
    struct hostent *host;
    struct in_addr **addr_list;

	if (hostname == NULL)
	{
		return -1;
	}

    host = gethostbyname(hostname);

    if (host == NULL)
    {
        perror("gethostbyname");
        return -1;
    }

    addr_list = (struct in_addr **) host->h_addr_list;

    strcpy(ip, inet_ntoa(*addr_list[0]));
    printf("IP ---> %s\n", ip);

    return 0;
}

void addURLtoFile(char *filename, char *url, char *ip)
{
    char buffer[1024];
    
    FILE *f = fopen(filename, "a");
    snprintf(buffer, 1024, "%s : %s\n", url, ip);
    fprintf(f, "%s", buffer);
    fclose(f);
}

static int checkpagecache(char *filename, char *url)
{
    char buffer[512];
    char s_url[512];
    snprintf(s_url, 511, "%s ", url);

    FILE *f = fopen(filename, "r");

    while(!feof(f))
    {
        fgets(buffer, 511, f);
        char *match = strstr(buffer, s_url);

        if (match != NULL)
            return 1;
    }

    return 0;
}

void addToPageCache(char *filename, char *url)
{
    char buffer[512];

    FILE *f = fopen(filename, "a");
    snprintf(buffer, 511, "%s %lu\n", url, time(NULL));
    fprintf(f, "%s", buffer);

    fclose(f);
}

static int checkTimeStamp(char *filename, char *url, int timeout)
{
    char buffer[512];
    char s_url[512];
    snprintf(s_url, 511, "%s ", url);

    FILE *f = fopen(filename, "r");
    
    while(!feof(f))
    {
        fgets(buffer, 511, f);
        char *match = strstr(buffer, s_url);

        if (match != NULL) {
            char temp[256];
            time_t prev_time;
            sscanf(match, "%s %ld", temp, &prev_time);

            if (difftime(time(NULL), prev_time) > timeout)
                return 1;
        }
    }

    return 0;
}

void updateTimeStamp(char *filename, char *input_url)
{
    FILE *ptr = fopen(filename, "a+");
    char url[512];
    strcpy(url, input_url);

    fseek(ptr, 0, SEEK_END);
    size_t file_size = ftell(ptr);
    fseek(ptr, 0, SEEK_SET);

    char *data = (char *)malloc(file_size);
    fread(data, 1, file_size, ptr);
    char *match = strstr(data, url);
    fclose(ptr);

    ptr = fopen(filename, "w+");
    char new_data[512];
    sprintf(new_data, "%s %lu", url, time(NULL));
    int i = 0;
    while(new_data[i])
    {
        *match = new_data[i];
        i++; match++;
    }

    fwrite(data, 1, file_size, ptr);
    fclose(ptr);
    free(data);
}

char *str2md5(const char *str, int length) {
    int n;
    MD5_CTX c;
    unsigned char digest[16];
    char *out = (char*)malloc(33);

    MD5_Init(&c);

    while (length > 0) {
        if (length > 512) {
            MD5_Update(&c, str, 512);
        } else {
            MD5_Update(&c, str, length);
        }
        length -= 512;
        str += 512;
    }

    MD5_Final(digest, &c);

    for (n = 0; n < 16; ++n) {
        snprintf(&(out[n*2]), 16*2, "%02x", (unsigned int)digest[n]);
    }

    return out;
}

void init_ServerRequest(char *url, char *c_resp, char *ip, int cfd, int port)
{
    char buf[1024];
    struct sockaddr_in p_server;
    int p_sfd, p_sock, n;

    memset(&p_server, 0, sizeof(p_server));
    p_server.sin_family = AF_INET;
    p_server.sin_addr.s_addr = inet_addr(ip);
    p_server.sin_port = htons(port);

	printf("For remote Connection: IP -> %s	Port -> %d\n", ip, port);
	printf("For remote Connnection: HTTp Request -> %s\n", c_resp);

    p_sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (p_sfd < 0)
		perror("Socket init failed");

    p_sock = connect(p_sfd, (struct sockaddr *)&p_server, sizeof(p_server));

    if (p_sock < 0)
        perror("Connection Failed");
    
    if (send(p_sfd, c_resp, strlen(c_resp), 0) < 0)
        perror("Remote Socket write failed");
    else {

        char *filename = str2md5(url, strlen(url));
        printf("filename --> %s\n", filename);
        char filepath[512];
        sprintf(filepath, "./cache/%s.html", filename);
        FILE *fp = fopen(filepath, "w+");
        do
        {
			printf("getting page from the remote connection\n");
            memset(buf, 0, sizeof(buf));
            n = recv(p_sfd, buf, 500, 0);
            fwrite(buf, 1, n, fp);
            if (!(n<=0))
                send(cfd, buf, n, 0);
        } while(n > 0);

        fclose(fp);
    }

	close(p_sfd);

}

void send_from_cache(char *url, int cfd)
{
    char filepath[1024];
    int n;
    char *filename = str2md5(url, strlen(url));
    sprintf(filepath, "./cache/%s.html", filename);

    FILE *fp = fopen(filepath, "r");
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *data = (char *)malloc(file_size);
    n = fread(data, 1, file_size, fp);
    send(cfd, data, n, 0);

    free(data);
}

void handleClientRequest(char *url, char *path, char *c_resp, char *ip, int cfd, int port, int timeout)
{
    if (checkpagecache("pagecache.txt", url) == 0)
    {
        printf("Page previously not stored in cache\n");

        //add Page cache
        addToPageCache("pagecache.txt", url);

        //get the page from server
        printf("Servicing the request through the remote server\n");
        init_ServerRequest(url, c_resp, ip, cfd, port);
    }
    else {

        //Check Time
        if (checkTimeStamp("pagecache.txt", url, timeout) == 1)
        {
            printf("Page Timeout\n");

            //Update the timestamp
            printf("Updating the timestamp\n");
            updateTimeStamp("pagecache.txt", url);

            //get the page from Server
            printf("Servicing the request through the remote server\n");
            init_ServerRequest(url, c_resp, ip, cfd, port);

        }
        else {
            printf("cache can service the request\n");
            //Service the Request from Cache
            send_from_cache(url, cfd);
        }
    }
}


int main(int argc, char **argv)
{
	if ((argc < 3) || (argc > 3)) {
		printf("Usage --> ./[%s] [Port Number] [Timeout]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in server, client;
	struct stat st;
	struct timeval timeout;
	
	char r_buffer[MAX_BUF_SIZE];		//Stores the CLient Response
	char s_buffer[512];					//Stores HTTP_Ok_Response
	char e_buffer[512];					//Stores HTTP_Error_Response
	char post_data[50];					//Stores Content of post data
	char method[10];					//Stores the Requested HTTP Method
	char url[30];						//Stores the Requested File Path
	char path[35];						//Stores the domain name
	char version[10];					//Stores the HTTP Version
	char f_type[30];					//Stores the type of file requested
	char f_name[30];					//Stores the file name requested

	char ip[30];

	/*HTTP Error Message Content in HTML Format*/
	char error_msg[] = "<!DOCTYPE html><html><title>Bad Request</title>""<pre><h1>HTTP 400 Bad Request</h1></pre>""</html>\r\n";

	char error_msg_url[] = "<!DOCTYPE html><html><title>Page Not Found</title>""<pre><h1>HTTP 404 Not Found</h1></pre>""</html>\r\n";

	char error_msg_block[] = "<!DOCTYPE html><html><title>URL Forbidden</title>""<pre><h1>ERROR 403 Forbidden</h1></pre>""</html>\r\n";

	ssize_t length;
	ssize_t f_size;

	int sfd, cfd;
	int child_pid = 0;
	//int rbytes;
	int option = 1;
	int c_size;
	int time_out = atoi(argv[2]);

	sfd = socket(AF_INET, SOCK_STREAM, 0);		//Create a Socket
	if (sfd == -1)
		print_error("Server: socket");

	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));		//Set Reuseable address option in socket

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(atoi(argv[1]));

	/*Bind the socket to the defined port*/
	if (bind(sfd, (struct sockaddr *) &server, sizeof(server)) == -1)
		print_error("Server: bind");

	if (listen(sfd, 10) == -1)
		print_error("Server: listen");

	/*Continuously Look for the incomming connections*/
	for (;;)
	{	
		length = sizeof(client);

		cfd = accept(sfd, (struct sockaddr *) &client, (socklen_t *) &length);
		if (cfd == -1)
		{
			perror("Server: accept\n");
			continue;
		}

		printf("\n\nAccepted a new connection = %d\n", client.sin_port);
		
		/*Create Child process*/
		child_pid = fork();
		//printf("Created a child process --> %d\n", child_pid);
		
		if (child_pid < 0)
			print_error("Server: fork\n");

		
		if (child_pid > 0)
		{
			close(cfd);
			waitpid(0, NULL, WNOHANG);	//Wait for state change of the child process
		}

		/*Service the request in Child Process*/
		if (child_pid == 0)
		{
			//close(sfd);
			printf("In the child process\n");
			
			memset(r_buffer, 0, sizeof(r_buffer));
			
			while ((recv(cfd, r_buffer, MAX_BUF_SIZE, 0)) > 0)
			{
			
				memset(s_buffer, 0, sizeof(s_buffer));
				memset(e_buffer, 0, sizeof(e_buffer));
				memset(method, 0, sizeof(method));
				memset(url, 0, sizeof(url));
				memset(path, 0, sizeof(path));
				memset(version, 0, sizeof(version));
				memset(f_type, 0, sizeof(f_type));
				memset(f_name, 0, sizeof(f_name));
				memset(ip, 0, sizeof(ip));

				sscanf(r_buffer, "%s %s %s", method, url, version);
				printf("Rec request -> %s\n", r_buffer);

				char *conn_status = strstr(r_buffer, "Connection: keep-alive");

                /*Check for connection status and set the timeout period*/
                if (conn_status)
                {
                    printf("Connection: Keep-alive\n");
                    timeout.tv_sec = t_out;
                    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeout, sizeof(struct timeval));
                }
                else {
                    printf("Connection: Close\n");
                    timeout.tv_sec = 0;
                    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeout, sizeof(struct timeval));
                }

				char *valid_url = strstr(url, "://");
				char *valid_req = strstr(url, "favicon");
				
				printf("--> %p	%p\n", valid_url, valid_req);

				if ((valid_url != NULL))
				{
				printf("Method: %s\nPath: %s\nVersion: %s\n", method, url, version);
				/*Check for inappropriate method*/
				if (strcmp(method, "GET") != 0)
				{
					printf("Inappropriate Method\n");
					c_size = strlen(error_msg);

					if (conn_status)
						http_error_resp(e_buffer, "Bad Request", version, "keep-alive", c_size);
					else
						http_error_resp(e_buffer, "Bad Request", version, "Close", c_size);

					send(cfd, e_buffer, strlen(e_buffer), 0);
					send(cfd, error_msg, strlen(error_msg), 0);
					
					if (conn_status) {
						continue;
					}
					
					else {
						printf("Closing Socket %d\n", client.sin_port);
						close(cfd);
						exit(0);
					}
				}

					/*Check for inappropriate version*/
				if ((strcmp(version, "HTTP/1.1") != 0) && (strcmp(version, "HTTP/1.0") != 0))
				{
					printf("Inappropriate Version\n");
					c_size = strlen(error_msg);

					if (conn_status)
						http_error_resp(e_buffer, "Bad Request", version, "keep-alive", c_size);
					else
						http_error_resp(e_buffer, "Bad Request", version, "Close", c_size);

					send(cfd, e_buffer, strlen(e_buffer), 0);
					send(cfd, error_msg, strlen(error_msg), 0);
					
					if (conn_status) {
						continue;
					}
					else {
						printf("Closing Socket %d\n", client.sin_port);
						close(cfd);
						exit(0);
					}
				}


				//Check URL
				strcpy(path, parsehostname(url));

				//Check the URL in blocked.txt
                if (isURLpresent("blocked.txt", path) == 1)
                {
                    printf("Blocked URL\n");
                    c_size = strlen(error_msg_block);

                    if (conn_status)
                        http_error_resp(e_buffer, "ERROR 403 Forbidden", version, "keep-alive", c_size);
                    else
                        http_error_resp(e_buffer, "ERROR 404 Forbidden", version, "Close", c_size);

                    send(cfd, e_buffer, strlen(e_buffer), 0);
                    send(cfd, error_msg_block, strlen(error_msg_block), 0);

                    if (conn_status) {
                        continue;
                    }
                    else {
                        printf("Closing Socket %d\n", client.sin_port);
                        close(cfd);
                        exit(0);
                    }

                }


				if (path != NULL)
				{
					//Check IPCache.txt for the given URL
					if (isURLpresent("IPCache.txt", path) == 1)
					{
						printf("URL found in IPCache.txt and DNS not required\n");
						int valid_hostname = hostname_to_ip(path, ip);
					}
					else
					{
						int valid_hostname = hostname_to_ip(path, ip);

						if (valid_hostname == -1)
						{
							printf("Inappropriate hostname\n");
							c_size = strlen(error_msg_url);

							if (conn_status)
								http_error_resp(e_buffer, "Page Not Found",  version, "keep-alive", c_size);
                    		else
								http_error_resp(e_buffer, "Page Not Found", version, "Close", c_size);

							send(cfd, e_buffer, strlen(e_buffer), 0);
							send(cfd, error_msg_url, strlen(error_msg_url), 0);

							if (conn_status) {
								continue;
							}
							else {
								printf("Closing Socket %d\n", client.sin_port);
								close(cfd);
								exit(0);
                    		}

						}

						//Store the URL in IPCache.txt
						addURLtoFile("IPCache.txt", path, ip);

					}
				}

				handleClientRequest(url, path, r_buffer, ip, cfd, 80, time_out);

			}
			else
			{
				close(cfd);
			}

			} //While LOOP
			printf("Connection Close due to timeout for socket %d\n", client.sin_port);
			close(cfd);
		} //If loop

		close(cfd);

	} //For Loop
}
