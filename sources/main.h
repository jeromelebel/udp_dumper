//
//  main.h
//  udp_dumper
//
//  Created by Jérôme Lebel on 27/04/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

extern int _main_socket;
extern struct sockaddr_in _main_listen_address;
extern int _dump_flag;

#define CHAR_PER_LINE 16

void dump_buffer(const void *buffer, ssize_t size);
