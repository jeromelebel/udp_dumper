//
//  UDPClient.cpp
//  udp_dumper
//
//  Created by Jérôme Lebel on 27/04/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#include "UDPClient.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "main.h"

UDPClient::UDPClient(struct sockaddr_in client_address, struct sockaddr_in server_address)
{
    struct sockaddr_in server_listen_address;
    struct timeval tv;
    
    _client_address = client_address;
    _server_address = server_address;
	_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	server_listen_address.sin_family = AF_INET;
	server_listen_address.sin_addr.s_addr = inet_addr("0.0.0.0");
	server_listen_address.sin_port = htons(0);
    
    tv.tv_sec = TIME_OUT_ACTIVITY;
    tv.tv_usec = 0;
    setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,  sizeof(tv));
    
	if (bind(_socket, (struct sockaddr *)&server_listen_address, sizeof(server_listen_address)) == -1) {
		fprintf(stderr, "can't listen to %s:%d\n", inet_ntoa(server_listen_address.sin_addr), ntohs(server_listen_address.sin_port));
        close(_socket);
        _socket = 0;
	}
}

UDPClient::~UDPClient()
{
    if (_socket) {
        close(_socket);
    }
}

struct sockaddr_in UDPClient::client_address()
{
    return _client_address;
}

void UDPClient::send_to_server(const void *buffer, ssize_t size)
{
    sendto(_socket, buffer, size, 0, (struct sockaddr *)&_server_address, sizeof(_server_address));
    printf("client (%s:%d) -> server (%s:%d) length %ld\n", inet_ntoa(_client_address.sin_addr), ntohs(_client_address.sin_port), inet_ntoa(_server_address.sin_addr), ntohs(_server_address.sin_port), size);
    if (_dump_flag) {
        dump_buffer(buffer, size);
    }
    time(&_last_activity_time);
}

void UDPClient::send_to_client(const void *buffer, ssize_t size)
{
    sendto(_main_socket, buffer, size, 0, (struct sockaddr *)&_client_address, sizeof(_client_address));
    printf("server (%s:%d) -> client (%s:%d) length %ld\n", inet_ntoa(_server_address.sin_addr), ntohs(_server_address.sin_port), inet_ntoa(_client_address.sin_addr), ntohs(_client_address.sin_port), size);
    if (_dump_flag) {
        dump_buffer(buffer, size);
    }
    time(&_last_activity_time);
}

int UDPClient::server_socket()
{
    return _socket;
}

time_t UDPClient::last_activity()
{
    return _last_activity_time;
}

