#include "rdma.h"
void send(RDMAMessage *m, const char* buffer) {

    //  char msg[36];
    //fprintf(stdout, "total buffer size %f\n", m->getBufSize() / (double) (1024 * 1024));
    char *msg = m->getbasePtr() + m->getBufSize();
    int pids[2];

    *((uint64_t *) msg) = 64;
    *((uint64_t * )(msg + 64 - sizeof(uint64_t))) = 64;
    pids[0] = 0;
    pids[1] = 1;
    std::strcpy(msg + sizeof(uint64_t), buffer);
    RDMAQueues::Status res = m->dirSendsTo(2, pids, msg, 64);
    assert(res == RDMAQueues::IO_SUCC);

    return;
}

void recv(RDMAMessage *m, char* buffer, int id) {
    int recv_count = 0;
    while(recv_count != 1) {
        if (m->tryRecvFrom(id, buffer)) {
            recv_count += 1;
        }
    }
    return;
}
