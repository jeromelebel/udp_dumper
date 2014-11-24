//
//  main.cpp
//  udp_dumper
//
//  Created by Jérôme Lebel on 26/04/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#define _WITH_GETLINE
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include "UDPClient.h"
#include "main.h"

#define CHAR_PER_LINE 16

int _server_socket = -1;
struct sockaddr_in _main_listen_address;
char *buffer;
int highest_socket;

struct _UDPClientElement {
    UDPClient *_udpClient;
    struct _UDPClientElement *_previous;
    struct _UDPClientElement *_next;
};

typedef struct _UDPClientElement UDPClientElement;

UDPClientElement *_first_client_element = NULL;

void test_receive(int socket)
{
    struct sockaddr_in client_address;
    socklen_t address_size = sizeof(client_address);
    while (1) {
        printf("bytes %ld\n", recvfrom(socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &address_size));
    }
}

void print_usage(const char *executable)
{
    fprintf(stdout, "\n");
    fprintf(stdout, "%s [-d] listen listen_port\n", executable);
    fprintf(stdout, "       To listen on port\n");
    fprintf(stdout, "%s [-d] send destionation_ip destination_port\n", executable);
    fprintf(stdout, "       To send Hello to this server\n");
    fprintf(stdout, "%s [-d] relay listen_port destionation_ip destination_port\n", executable);
    fprintf(stdout, "       To be a relay between who ever send a packet, and the destination\n");
}

void dump_buffer(const void *buffer, ssize_t size)
{
	int ii = 0;
	char line_hex[256];
	char line_char[256];
	int char_count = 0;
	char *line_hex_cursor;
	char *line_char_cursor;
    
	while(ii < size) {
		char current_char;
        
		if (char_count == 0) {
			line_hex_cursor = line_hex;
			line_char_cursor = line_char;
		}
		if (char_count % 4 == 0) {
			line_hex_cursor[0] = ' ';
			line_hex_cursor++;
			line_char_cursor[0] = ' ';
			line_char_cursor++;
		}
		current_char = ((const char *)buffer)[ii];
		if (current_char < ' ') {
			line_char_cursor[0] = '.';
		} else {
			line_char_cursor[0] = current_char;
		}
		line_char_cursor++;
		snprintf(line_hex_cursor, 3, "%02X", current_char);
		line_hex_cursor += 2;
		ii++;
		char_count++;
		if (char_count == CHAR_PER_LINE || ii == size) {
			while (char_count < CHAR_PER_LINE) {
                if (char_count % 4 == 0) {
                    line_hex_cursor[0] = ' ';
                    line_hex_cursor++;
                }
				line_hex_cursor[0] = ' ';
				line_hex_cursor++;
				line_hex_cursor[0] = ' ';
				line_hex_cursor++;
				line_char_cursor[0] = ' ';
                line_char_cursor++;
                char_count++;
			}
			line_hex_cursor[0] = 0;
			line_char_cursor[0] = 0;
			printf("%s %s\n", line_hex, line_char);
			char_count = 0;
		}
  	}
}

UDPClient *process_client_request(struct sockaddr_in *server_address, bool dump_flag)
{
    struct sockaddr_in client_address;
    socklen_t size = sizeof(client_address);
    UDPClient *result = NULL;
    ssize_t read_bytes;
    
    read_bytes = recvfrom(_server_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &size);
    
    if (read_bytes > 0) {
        printf("Received %ld from %s\n", read_bytes, inet_ntoa(client_address.sin_addr));
        if (server_address) {
            result = new UDPClient(client_address, *server_address, _server_socket, dump_flag);
            result->send_to_server(buffer, read_bytes);
        } else if (dump_flag) {
            dump_buffer(buffer, read_bytes);
        }
    }
    return result;
}

void add_client(UDPClient *client)
{
    UDPClientElement *element;
    
    element = (UDPClientElement *)malloc(sizeof(*element));
    element->_udpClient = client;
    element->_previous = NULL;
    element->_next = _first_client_element;
    if (_first_client_element) {
        _first_client_element->_previous = element;
    }
    _first_client_element = element;
}

UDPClientElement *_remove_client_element(UDPClientElement *element)
{
    UDPClientElement *next;
    
    next = element->_next;
    delete element->_udpClient;
    if (element->_previous) {
        element->_previous->_next = element->_next;
    }
    if (element->_next) {
        element->_next->_previous = element->_previous;
    }
    if (_first_client_element == element) {
        _first_client_element = element->_next;
    }
    free(element);
    return next;
}

void remove_client(UDPClient *client)
{
    UDPClientElement *cursor = _first_client_element;
    
    while (cursor) {
        if (cursor->_udpClient == client) {
            _remove_client_element(cursor);
            break;
        }
        cursor = cursor->_next;
    }
}

UDPClient *client_for_socket(int socket)
{
    UDPClientElement *cursor = _first_client_element;
    
    while (cursor) {
        if (cursor->_udpClient->server_socket() == socket) {
            break;
        }
        cursor = cursor->_next;
    }
    return cursor?cursor->_udpClient:NULL;
}

void listen_from_server(UDPClient *client)
{
    struct sockaddr_in client_address;
    socklen_t address_size = sizeof(client_address);
    ssize_t read_bytes;
    
    read_bytes = recvfrom(client->server_socket(), buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &address_size);
    if (read_bytes > 0) {
        client->send_to_client(buffer, read_bytes);
    }
}

void clean_up_clients()
{
    UDPClientElement *cursor = _first_client_element;
    time_t current_time;
    
    time(&current_time);
    highest_socket = _server_socket;
    while (cursor) {
        if (current_time - cursor->_udpClient->last_activity() > TIME_OUT_ACTIVITY) {
            cursor = _remove_client_element(cursor);
        } else {
            if (highest_socket < cursor->_udpClient->server_socket()) {
                highest_socket = cursor->_udpClient->server_socket();
            }
            cursor = cursor->_next;
        }
    }
}

void server_with_select(struct sockaddr_in *destination, bool dump_flag)
{
    fd_set active_fd_set, read_fd_set;
    struct timeval timeout;
    int readsocks;
    int ii;
    
    FD_ZERO(&active_fd_set);
    FD_SET(_server_socket, &active_fd_set);
    highest_socket = _server_socket;
    while (1) {
        read_fd_set = active_fd_set;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        readsocks = select(highest_socket + 1, &read_fd_set, NULL, NULL, &timeout);
        for (ii = 0; ii < highest_socket + 1; ++ii) {
            if (FD_ISSET(ii, &read_fd_set)) {
                if (ii == _server_socket) {
                    UDPClient *new_client;
                    
                    new_client = process_client_request(destination, dump_flag);
                    if (new_client) {
                        FD_SET(new_client->server_socket(), &active_fd_set);
                        add_client(new_client);
                    }
                } else {
                    UDPClient *client;
                    
                    client = client_for_socket(ii);
                    if (client) {
                        listen_from_server(client);
                    }
                }
            }
        }
        clean_up_clients();
    }
}

struct sockaddr_in socket_address(const char *address, int port)
{
    struct sockaddr_in result;
    
    result.sin_family = AF_INET;
    if (address) {
        result.sin_addr.s_addr = inet_addr(address);
    } else {
        result.sin_addr.s_addr = INADDR_ANY;
    }
    result.sin_port = htons(port);
    return result;
}

int open_server_socket(int port)
{
    sockaddr_in server_address;
    struct timeval tv;
    int socket_result;
    
    server_address = socket_address(NULL, port);
    socket_result = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    tv.tv_sec = TIME_OUT_ACTIVITY;
    tv.tv_usec = 0;
    setsockopt(socket_result, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,  sizeof(tv));
    if (bind(socket_result, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        fprintf(stderr, "can't listen to %s:%d\n", inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));
        exit(EXIT_FAILURE);
    }
    printf("listening from %s:%d\n", inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));
    return socket_result;
}

int main(int argc, char *argv[])
{
	int opt;
    bool dump_flag = false;
	
    buffer = (char *)malloc(BUFFER_SIZE);
    
	while ((opt = getopt(argc, argv, "d")) != -1) {
		switch (opt) {
			case 'd':
				dump_flag = true;
				break;
			default:
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	
	if (argc - optind < 2 || argc - optind > 4) {
		fprintf(stderr, "wrong argument count\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	
    if (strcmp(argv[optind], "listen") == 0) {
        optind++;
        _server_socket = open_server_socket(atoi(argv[optind]));
        server_with_select(NULL, dump_flag);
    } else if (strcmp(argv[optind], "send") == 0) {
        UDPClient *client;
        struct sockaddr_in from_address, to_address;
        size_t len;
        ssize_t read;
        
        optind++;
        
        to_address = socket_address(argv[optind], atoi(argv[optind + 1]));
        from_address = socket_address(NULL, atoi(argv[optind + 1]));
        
        read = getline(&buffer, &len, stdin);
        if (read > 0) {
            client = new UDPClient(from_address, to_address, -1, dump_flag);
            client->send_to_server(buffer, read);
        }
    } else if (strcmp(argv[optind], "relay") == 0) {
        struct sockaddr_in to_address;
        
        optind++;
        _server_socket = open_server_socket(atoi(argv[optind]));
        to_address = socket_address(argv[optind + 1], atoi(argv[optind + 2]));
        server_with_select(&to_address, dump_flag);
    }
    
	return 0;
}
