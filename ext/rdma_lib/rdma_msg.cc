#include "rdma_msg.h"

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>


RDMAMessage::RDMAMessage(uint64_t ringSz,uint64_t ringPad,int tid,int nodes,RDMAQueues *r,char *basePtr)
  :ringSize(ringSz),
   ringPadding(ringPad),
   tid(tid),
   numNodes(nodes),
   basePtr(basePtr),
   rdma(r),
   rw(r),
   totalBufSize( calculateBufSz(ringSz,ringPad,nodes))
{  
  /* We assume the largest message is less than the padding */
  offsets = new uint64_t[nodes];
  headers = new uint64_t[nodes];
  for(uint i = 0;i < nodes;++i) {
    offsets[i] = 0;
    headers[i] = 0;
  }

  memset(basePtr,0,totalBufSize);
  
  char *startPtr = r->getBuffer();
  baseOffset = basePtr - startPtr;
}

RDMAMessage::RDMAMessage(int tid,int nodes,RDMAQueues *r,char *basePtr)
  :RDMAMessage(MSG_DEFAULT_SZ,MSG_DEFAULT_PADDING, tid,nodes,r,basePtr) {
  
}


RDMAQueues::Status RDMAMessage::dirSendTo(int pid,char *msg,int len,int t) {

  uint64_t offset = baseOffset + rdma->getNodeId() * (ringSize + ringPadding + META_SZ) + 
    (offsets[pid] % ringSize) + META_SZ;
  offsets[pid] += len;
  return rdma->write(_QP_ENCODE_ID(pid,t),msg,offset,len);  
}

RDMAQueues::Status RDMAMessage::dirSendsTo(int num,int *pids,char *msg,int len) {
  
  uint64_t *offs = new uint64_t[num];
  for(uint i = 0;i < num;++i) {
    offs[i] = baseOffset + rdma->getNodeId() * (ringSize + ringPadding + META_SZ) + offsets[pids[i]] % ringSize + META_SZ;
    offsets[pids[i]] += len;
    //    fprintf(stdout,"offset %d f\n",i,offs[i] / (double)(1024*1024));    
    pids[i] = _QP_ENCODE_ID(pids[i],tid);
  }
  RDMAQueues::Status res = rw.writesTo(num,pids,msg,offs,len);
  delete offs;
  return res;
}

bool RDMAMessage::tryRecvFrom(int mac,char *msg) {
  
  uint64_t pollOffset = mac * (ringSize + ringPadding + META_SZ) + headers[mac] % ringSize;
  volatile uint64_t *pollPtr = (uint64_t *)(basePtr + pollOffset + META_SZ);
  
  if(*pollPtr != 0) {
    uint64_t msgSz = *pollPtr;
    uint64_t *endPtr = (uint64_t *)((char *)pollPtr + msgSz - sizeof(uint64_t));
    //    fprintf(stdout,"rcv message size %lu\n",*pollPtr);
    //    sleep(1);
    if(*endPtr != msgSz)  {
      //      exit(-1);
      return false;
    }
    memcpy(msg,(char *)pollPtr + sizeof(uint64_t),msgSz - sizeof(uint64_t) - sizeof(uint64_t));
    memset((char *)pollPtr,0,msgSz);
    headers[mac] += msgSz;
    
    return true;      
  } else
    return false;
}




