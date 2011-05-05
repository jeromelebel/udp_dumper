//
//  main.cpp
//  udp_dumper
//
//  Created by Jérôme Lebel on 26/04/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "UDPClient.h"
#include "main.h"

int _main_socket;
struct sockaddr_in _main_listen_address;
int _dump_flag = 0;
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
	fprintf(stderr, "%s [-d] listen_port destionation_ip destination_port\n", executable);
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

UDPClient *process_client_request(struct sockaddr_in server_address)
{
    struct sockaddr_in client_address;
    socklen_t size = sizeof(client_address);
    UDPClient *result = NULL;
    ssize_t read_bytes;
    
    read_bytes = recvfrom(_main_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &size);
    
    if (read_bytes > 0) {
        result = new UDPClient(client_address, server_address);
        result->send_to_server(buffer, read_bytes);
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
    highest_socket = _main_socket;
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

void server_with_select(struct sockaddr_in server_address)
{
    fd_set active_fd_set, read_fd_set;
    struct timeval timeout;
    int readsocks;
    int ii;
    
    FD_ZERO(&active_fd_set);
    FD_SET(_main_socket, &active_fd_set);
    highest_socket = _main_socket;
    while (1) {
        read_fd_set = active_fd_set;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        readsocks = select(highest_socket + 1, &read_fd_set, NULL, NULL, &timeout);
        for (ii = 0; ii < highest_socket + 1; ++ii) {
            if (FD_ISSET(ii, &read_fd_set)) {
                if (ii == _main_socket) {
                    UDPClient *new_client;
                    
                    new_client = process_client_request(server_address);
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

int main(int argc, char *argv[])
{
	int opt;
	struct sockaddr_in server_address;
    struct timeval tv;
	
	while ((opt = getopt(argc, argv, "d")) != -1) {
		switch (opt) {
			case 'd':
				_dump_flag = 1;
				break;
			default:
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	
	if (argc - optind != 3) {
		fprintf(stderr, "need to have 3 arguments at the end\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	
    buffer = (char *)malloc(BUFFER_SIZE);
	_main_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	_main_listen_address.sin_family = AF_INET;
	_main_listen_address.sin_addr.s_addr = inet_addr("0.0.0.0");
	_main_listen_address.sin_port = htons(atoi(argv[optind]));
    tv.tv_sec = TIME_OUT_ACTIVITY;
    tv.tv_usec = 0;
    setsockopt(_main_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,  sizeof(tv));
	if (bind(_main_socket, (struct sockaddr *)&_main_listen_address, sizeof(_main_listen_address)) == -1) {
		fprintf(stderr, "can't listen to %s:%d\n", inet_ntoa(_main_listen_address.sin_addr), ntohs(_main_listen_address.sin_port));
		exit(EXIT_FAILURE);
	}
    
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(argv[optind + 1]);
	server_address.sin_port = htons(atoi(argv[optind + 2]));

    printf("listening to %s:%d\n", inet_ntoa(_main_listen_address.sin_addr), ntohs(_main_listen_address.sin_port));
    printf("sending to %s:%d\n", inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));
    
    server_with_select(server_address);
    
	return 0;
}
