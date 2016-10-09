#ifndef _RDMA_H
#define _RDMA_H
#include "../ext/rdma_lib/rdma_msg.h"
#include <cstring>
#include <iostream>
void send(RDMAMessage *m, const char *buffer);
void recv(RDMAMessage *m, char *buffer, int id);
#endif
