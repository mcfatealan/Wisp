#include "rdma.h"
void send(RDMAMessage *m, char* buffer) {

    //  char msg[36];
    fprintf(stdout, "total buffer size %f\n", m->getBufSize() / (double) (1024 * 1024));
    char *msg = buffer + m->getBufSize();
    int pids[2];

        *((uint64_t *) msg) = 64;
        *((uint64_t * )(msg + 64 - sizeof(uint64_t))) = 64;
        pids[0] = 0;
        pids[1] = 1;
        std::strcpy(msg + sizeof(uint64_t), buffer);
        //    m->dirSendTo(1,msg,64);
        RDMAQueues::Status res = m->dirSendsTo(2, pids, msg, 64);
        //    fprintf(stdout,"status %d\n",res);
        assert(res == RDMAQueues::IO_SUCC);
        //    msg += 64;

    char rep[1024];
        if (m->tryRecvFrom(0, rep)) {
            fprintf(stdout, "recv msg @ %s",  rep);
        } else
            assert(false);
    return;
}

void recv(RDMAMessage *m) {

    char msg[1024];
    int recv_count = 0;
    while(recv_count != 1) {
        if (m->tryRecvFrom(0, msg)) {
            fprintf(stdout, "recv msg @ %s", msg);
            recv_count += 1;
        }
    }
    return;
}
