//#include "rdma_client.h"
#include "rdma_msg.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

void send(RDMAMessage *c);
void recv(RDMAMessage *c);

char *buffer;

int main(int argc,char **argv) {
  
  if(argc < 2) {
    fprintf(stderr,"need one parmenter..\n");
    exit(-1);
  }
  
  int id = atol(argv[1]);
  fprintf(stdout,"Client tests start with id %d\n",id);

  std::vector<std::string> netDef;
  netDef.push_back("10.0.0.100");
  netDef.push_back("10.0.0.101");
  
  
  uint64_t bufSize = 1024*1024*1024;  
  bufSize = bufSize * 4;
  buffer = new char[bufSize];
  
  int port = 5555;
  int qpPerMac = 4;

  RDMAQueues *rdma = bootstrapRDMA(id,port,netDef,qpPerMac,buffer,bufSize);
  fprintf(stdout,"rdma bootstrap done\n");
  //  RDMAClient *client = new RDMAClient(rdma,buffer);  
  RDMAMessage *msg = new RDMAMessage(0,2,rdma,buffer);
  
  if(id == 0) {
    send(msg);
  } else
    recv(msg);
  
  fprintf(stdout,"done\n");
  while(1) {sleep(1);}  
}


void send(RDMAMessage *m) {
  
  //  char msg[36];
  fprintf(stdout,"total buffer size %f\n",m->getBufSize() / (double)(1024 * 1024));
  char *msg = buffer + m->getBufSize();
  int pids[2];
  
  for(int i = 0;i < 4;++i) {
    *((uint64_t *)msg) = 64;
    *((uint64_t *)(msg + 64 - sizeof(uint64_t))) = 64;
    pids[0] = 0;
    pids[1] = 1;
    sprintf(msg + sizeof(uint64_t),"Hello world from %d %d\n",0,i);
    //    m->dirSendTo(1,msg,64);
    RDMAQueues::Status res = m->dirSendsTo(2,pids,msg,64);
    //    fprintf(stdout,"status %d\n",res);
    assert(res == RDMAQueues::IO_SUCC);
    //    msg += 64;
  }
  
  char rep[1024];
  for(int i = 0;i < 4;++i) {
    if(m->tryRecvFrom(0,rep)) {
      fprintf(stdout,"recv msg @ %d ,%s",i,rep);
    } else 
      assert(false);
  }
  return;
}

void recv(RDMAMessage *m) {

  char msg[1024];
  int recv_count = 0;
  
  while(recv_count != 4) {
    for(int i = 0;i < 4;++i) {
      if(m->tryRecvFrom(0,msg)) {
	fprintf(stdout,"recv msg @ %d ,%s",i,msg);
	recv_count += 1;
      }
    }
  }
  return ;
}
