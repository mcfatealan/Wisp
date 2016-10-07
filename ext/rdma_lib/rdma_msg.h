#ifndef RDMA_MSG_
#define RDMA_MSG_

#include "rdmaio.h"
#include "rdma_wrapper.h"

#define META_SZ sizeof(uint64_t)

#define MSG_DEFAULT_SZ       (4 * 1024 * 1024)
#define MSG_DEFAULT_PADDING  16 * 1024


class RDMAMessage {
  
  const uint64_t ringSize;
  const uint64_t ringPadding;
  const uint64_t totalBufSize;
  uint64_t baseOffset;
  
  int numNodes;
  
  char *basePtr;
  RDMAQueues *rdma;
  RDMAFunctionWrapper rw;

  uint64_t *offsets;
  uint64_t *headers;

  int tid;
  
 public:
  RDMAMessage(uint64_t ringSz,uint64_t ringPadding,int tid,int nodes,RDMAQueues *r,char *basePtr = NULL);
  RDMAMessage(int tid,int nodes,RDMAQueues *r,char *basePtr);  

  static uint64_t calculateBufSz(uint64_t ringSz,uint64_t padding,int nodes) {
    return (nodes) * (ringSz + padding + META_SZ) + padding;
  }

  
  uint64_t getBufSize() { return totalBufSize;}
  RDMAQueues::Status dirSendTo(int pid,char *msg,int len,int t);
  RDMAQueues::Status dirSendTo(int pid,char *msg,int len) {
    return dirSendTo(pid,msg,len,tid);
  }
  RDMAQueues::Status dirSendsTo(int num,int *pids,char *msg,int len);
  bool  tryRecvFrom(int mac,char *buffer);
  
  
};

#endif
