#ifndef RDMA_WRAPPER_
#define RDMA_WRAPPER_

#include "rdmaio.h"

#include <set>

/* A wrapper class over RDMAQueues, which provides some advanced rdma operations */
class RDMAFunctionWrapper {
  RDMAQueues *rdma;
 public:
  
 RDMAFunctionWrapper(RDMAQueues *r) :rdma(r) {};
  
  RDMAQueues::Status casualWrite(int qid,char *localBuf,uint64_t offset,int len) {
    int sendflags = IBV_SEND_FENCE;    
    if(rdma->needPoll(qid)) {
      sendflags = sendflags | IBV_SEND_SIGNALED;      
      if( rdma->postReq(qid,localBuf,offset,len,IBV_WR_RDMA_WRITE,sendflags))
	return RDMAQueues::IO_ERR;
      return rdma->pollCompletion(qid);
    } else {
      if( rdma->postReq(qid,localBuf,offset,len,IBV_WR_RDMA_WRITE,sendflags))
	return RDMAQueues::IO_ERR;
      return RDMAQueues::IO_SUCC;
    }
  }

  RDMAQueues::Status writesTo(int num,int *qids, char *localBuf,uint64_t *offsets,int len) {
    int sendflags = IBV_SEND_SIGNALED;
    if(rdma->postReqs(num,qids,localBuf,offsets,len,IBV_WR_RDMA_WRITE,sendflags) )
      return RDMAQueues::IO_ERR;
    return rdma->pollCompletions(num,qids);
  }
};

#endif
