#ifndef _RDMA_H
#define _RDMA_H
#include "../ext/rdma_lib/rdma_msg.h"
#include <cstring>
void send(RDMAMessage *m, char *buffer);
void recv(RDMAMessage *m);
#endif
