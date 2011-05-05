//
//  UDPClient.h
//  udp_dumper
//
//  Created by Jérôme Lebel on 27/04/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#include <netinet/in.h>
#include <sys/types.h>


#define BUFFER_SIZE 65535
#define TIME_OUT_ACTIVITY 20

class UDPClient
{
protected:
    struct sockaddr_in _client_address;
    struct sockaddr_in _server_address;
    int _socket;
    time_t _last_activity_time;
    
public:
    UDPClient(struct sockaddr_in client_address, struct sockaddr_in server_address);
    ~UDPClient();
    
    struct sockaddr_in client_address();
    void send_to_server(const void *buffer, ssize_t size);
    void send_to_client(const void *buffer, ssize_t size);
    int server_socket();
    time_t last_activity();
};
