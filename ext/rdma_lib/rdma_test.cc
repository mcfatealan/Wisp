#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "rdmaio.h"
#include "rdma_wrapper.h"


inline uint64_t
rdtsc(void)
{
  uint32_t hi, lo;
  __asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)lo)|(((uint64_t)hi)<<32);
}

void testRw(RDMAQueues *rdma,char *buffer,int id) ;

int main(int argc, char **argv) {
  
  if(argc < 2) {
    fprintf(stderr,"need one parmenter..\n");
    exit(-1);
  }

  int id = atol(argv[1]);
  fprintf(stdout,"tests start with id %d\n",id);

  std::vector<std::string> netDef;
  netDef.push_back("10.0.0.100");
  netDef.push_back("10.0.0.101");
  
  
  int bufSize = 1024*1024;  
  char *buffer = new char[bufSize];
  if(id != 0) {
    uint64_t *setptr = (uint64_t *)buffer;
    for(uint i = 0;i < 4;++i) {
      setptr[i] = 73 + i;
    }
  }
  
  int port = 5555;
  int qpPerMac = 4;
  RDMAQueues *rdma = bootstrapRDMA(id,port,netDef,qpPerMac,buffer,bufSize);

  if(id == 0)
    testRw(rdma,buffer,id);
  
  while(1) {sleep(1);}
}


void testRw(RDMAQueues *rdma,char *buffer,int id) {
  /* test local read write */
  fprintf(stdout,"start testing local rw..\n");
  char *localBuffer = buffer;
  
  uint64_t offset = 1024;
  
  char *testBuffer = buffer + offset;
  const char *testStr = "RDMA local tests %d.";

#if 1

  for(int i = 0;i < 4;++i) {
    sprintf(testBuffer,testStr,i);
    assert(rdma->read(_QP_ENCODE_ID(id,i),localBuffer,offset,sizeof(uint64_t)) == RDMAQueues::IO_SUCC);
    assert(rdma->read(_QP_ENCODE_ID(id,i),localBuffer,offset,strlen(testBuffer) + 1) == RDMAQueues::IO_SUCC);
    fprintf(stdout,"read %s, check %s\n",localBuffer,testBuffer);
  }

#endif
  
  fprintf(stdout,"start testing remote rw..\n");
  for(int i = 0;i < 4;++i)  {
    assert(rdma->read(_QP_ENCODE_ID(1,i),localBuffer,0 + i * sizeof(uint64_t),sizeof(uint64_t)) == RDMAQueues::IO_SUCC);  
    fprintf(stdout,"poll value %lu\n", *((uint64_t *)localBuffer));
  }

  char *localBuffer2 = localBuffer + sizeof(uint64_t);
  for(int i = 0;i < 4;++i)  {
    *((uint64_t *)localBuffer) = i * 73;
    assert(rdma->write(_QP_ENCODE_ID(1,i),localBuffer,0 + i * sizeof(uint64_t),sizeof(uint64_t)) == RDMAQueues::IO_SUCC);  
    assert(rdma->read(_QP_ENCODE_ID(1,i),localBuffer2,0 + i * sizeof(uint64_t),sizeof(uint64_t)) == RDMAQueues::IO_SUCC);  
    fprintf(stdout,"poll value after write %lu\n", *((uint64_t *)localBuffer2));    
  }

  fprintf(stdout,"start testing cmp and swap...\n");
  uint64_t compare = 0;
  for(int i = 0;i < 4;++i) {
    assert(rdma->syncCompareSwap(_QP_ENCODE_ID(1,i), localBuffer,0,compare, i * 73) == RDMAQueues::IO_SUCC);
    if(*((uint64_t *)localBuffer) != compare) {
      fprintf(stdout,"compare value %lu, local buf %lu\n",compare,*((uint64_t *)localBuffer));
    }
    compare = i * 73;
  }
  
  assert(rdma->read(_QP_ENCODE_ID(1,0),localBuffer,0 ,sizeof(uint64_t)) == RDMAQueues::IO_SUCC);    
  fprintf(stdout,"final val %lu\n",*((uint64_t *)localBuffer));


  RDMAFunctionWrapper w(rdma);
  * ((uint64_t *)localBuffer) = 73;
  /* Test causual write */
  uint64_t start = rdtsc();
  assert(w.casualWrite(_QP_ENCODE_ID(1,0), localBuffer,1024,sizeof(uint64_t)) == RDMAQueues::IO_SUCC);
  uint64_t end  = rdtsc();
  uint64_t gap0 = end - start;

  *((uint64_t *)localBuffer2) = 0;
  start = rdtsc();
  assert(rdma->read(_QP_ENCODE_ID(1,0),localBuffer2,1024 ,sizeof(uint64_t)) == RDMAQueues::IO_SUCC);      
  uint64_t gap1 = rdtsc() - start;
  
  fprintf(stdout,"check val %lu\n",*((uint64_t *)localBuffer2));
  fprintf(stdout,"casual cycles %lu, otherwise %lu\n",gap0,gap1);

  for(uint i = 0;i < 64;++i) {
    * ((uint64_t *)localBuffer) = 73 + i;    
    uint64_t start = rdtsc();
    assert(w.casualWrite(_QP_ENCODE_ID(1,0), localBuffer,1024 + i * sizeof(uint64_t),sizeof(uint64_t)) == RDMAQueues::IO_SUCC);    
    uint64_t end  = rdtsc();
    fprintf(stdout,"gap %d %lu\n",i,end - start);
  }
  assert(rdma->read(_QP_ENCODE_ID(1,0),localBuffer2,1024 + 32 * sizeof(uint64_t) ,sizeof(uint64_t)) == RDMAQueues::IO_SUCC);  
  fprintf(stdout,"check again val %lu\n",*((uint64_t *)localBuffer2));
  
  /*Test multiple writs*/
  int ids[2];
  uint64_t offs[2];
  ids[0] = _QP_ENCODE_ID(0,0);
  ids[1] = _QP_ENCODE_ID(1,0);
  
  offs[0] = 0;
  offs[1] = 1024;
  
  *((uint64_t *)localBuffer2) = 2049;
  assert(w.writesTo(2,ids,localBuffer2,offs,sizeof(uint64_t)) == RDMAQueues::IO_SUCC);
  
  fprintf(stdout,"check local buf %lu\n", *((uint64_t *)buffer));
  assert(rdma->read(_QP_ENCODE_ID(1,0),localBuffer,offs[1],sizeof(uint64_t)) == RDMAQueues::IO_SUCC);
  fprintf(stdout,"check remote buf %lu\n", *((uint64_t *)buffer));  
  
  fprintf(stdout,"done\n");
}
