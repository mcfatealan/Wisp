#include "rdmaio.h"
#include "simple_map.h"

/* System headers */ 
#include <byteswap.h>
#include <getopt.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include <zmq.hpp>
//#include <infiniband/verbs.h>

#define MAX_DEVICES 8
#define CAUSUAL_THRESHOLD 16

#define MAX_POLL_CQ_TIMEOUT 2000

#define unlikely(x) __builtin_expect(!!(x), 0)

static int modify_qp_to_init (struct ibv_qp *qp);
static int modify_qp_to_rtr (struct ibv_qp *qp, uint32_t remote_qpn, uint16_t dlid,uint8_t * dgid);
static int modify_qp_to_rts (struct ibv_qp *qp);


#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t
htonll (uint64_t x)
{
  return bswap_64 (x);
}

static inline uint64_t
ntohll (uint64_t x)
{
  return bswap_64 (x);
}
#elif __BYTE_ORDER == __BIG_ENDIAN

static inline uint64_t
htonll (uint64_t x)
{
  return x;
}

static inline uint64_t
ntohll (uint64_t x)
{
  return x;
}
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

struct config_t
{
  const char *dev_name;         /* IB device name */
  char *server_name;            /* server host name */
  u_int32_t tcp_port;           /* server TCP port */
  int ib_port;                  /* local IB port to work with */
  int gid_idx;                  /* gid index to use */
};


struct config_t rdmaConfig = {
  NULL,                         /* dev_name */
  NULL,                         /* server_name */
  19875,                        /* tcp_port */
  1    ,                        /* ib_port */
  -1                            /* gid_idx */
};

struct _device {

  struct ibv_device_attr device_attr;   /* Device attributes */
  struct ibv_port_attr port_attr;       /* IB port attributes */
  struct ibv_context *ib_ctx;   /* device handle */  
  
  struct ibv_pd *pd;            /* PD handle */
  struct ibv_mr *mr;            /* MR handle for buf */      

  char *buf;
};

/* structure to exchange data which is needed to connect the QPs */
struct cm_con_data_t_
{
  uint64_t addr;                /* Buffer address */
  uint32_t rkey;                /* Remote key */
  uint32_t qp_num;              /* QP number */
  uint16_t lid;                 /* LID of the IB port */
  uint8_t gid[16];              /* gid */
} __attribute__ ((packed));

struct _QP {
  
  struct cm_con_data_t_ remote_props;  /* values to connect to remote side */
  struct ibv_pd *pd;            /* PD handle */
  struct ibv_cq *cq;            /* CQ handle */
  struct ibv_qp *qp;            /* QP handle */
  struct ibv_mr *mr;
  _device *device;
  int pendings;

  int casualThrs() { return CAUSUAL_THRESHOLD;}
  
  _QP(_device *dev)
    :pd(NULL),cq(NULL),qp(NULL),mr(NULL),device(NULL),pendings(0) {
        
    /* TODO! current we does not consider creating queue pair error */
    
    /* some initilization */
    memset(&remote_props,0,sizeof(remote_props));    

    struct ibv_qp_init_attr qp_init_attr;
    int cq_size = CAUSUAL_THRESHOLD;    
    cq = ibv_create_cq (dev->ib_ctx,cq_size,NULL,NULL,0);
    
    memset (&qp_init_attr, 0, sizeof (qp_init_attr));
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.sq_sig_all = 0;
    qp_init_attr.send_cq = cq;
    qp_init_attr.recv_cq = cq;
    qp_init_attr.cap.max_send_wr = CAUSUAL_THRESHOLD;
    qp_init_attr.cap.max_recv_wr = 1;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    
    qp = ibv_create_qp (dev->pd, &qp_init_attr);        
   
    device = dev;
    mr = device->mr;
  }
  
  struct cm_con_data_t_ getConData() {

    struct cm_con_data_t_ res;
    union ibv_gid my_gid;
    if(rdmaConfig.gid_idx >= 0) {
      ibv_query_gid (device->ib_ctx, rdmaConfig.ib_port, rdmaConfig.gid_idx, &my_gid);
    } else {
      memset(&my_gid,0,sizeof my_gid);
    }
    res.addr = htonll ((uintptr_t) device->buf);
    res.rkey = htonl (device->mr->rkey);
    res.qp_num = htonl (qp->qp_num);
    //    fprintf(stdout,"return qp num %d\n",qp->qp_num);
    res.lid = htons (device->port_attr.lid);
    memcpy (res.gid, &my_gid, 16);
    //    fprintf (stdout, "\n Local LID = 0x%x %d\n", device->port_attr.lid,device->mr->rkey);
    return res;
  }

  int connect(struct cm_con_data_t_ tmp_con_data) {

    struct cm_con_data_t_ remote_con_data;
    int rc = 0;
    char temp_char;
    
    /* exchange using TCP sockets info required to connect QPs */
    
    remote_con_data.addr = ntohll (tmp_con_data.addr);
    remote_con_data.rkey = ntohl (tmp_con_data.rkey);
    remote_con_data.qp_num = ntohl (tmp_con_data.qp_num);
    remote_con_data.lid = ntohs (tmp_con_data.lid);
    memcpy (remote_con_data.gid, tmp_con_data.gid, 16);
    //    fprintf(stdout,"connect qp num %d\n",remote_con_data.qp_num);
    /* save the remote side attributes, we will need it for the post SR */
    this->remote_props = remote_con_data;
    //    fprintf(stdout,"remote_props.addr %lu\n",remote_props.addr);
    
    if (rdmaConfig.gid_idx >= 0) {
      
      uint8_t *p = remote_con_data.gid;
      fprintf (stdout,
               "Remote GID = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
               p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9],
               p[10], p[11], p[12], p[13], p[14], p[15]);
    }
    /* modify the QP to init */
    rc = modify_qp_to_init (qp);
    if (rc) {
      fprintf (stderr, "change QP state to INIT failed\n");
      goto connect_qp_exit;
    }
    /* let the client post RR to be prepared for incoming messages */
  
    /* modify the QP to RTR */
    rc =
      modify_qp_to_rtr (qp, remote_con_data.qp_num, remote_con_data.lid,
			remote_con_data.gid);
    if (rc) {
      fprintf (stderr, "failed to modify QP state to RTR\n");
      goto connect_qp_exit;
    }
    //  fprintf (stderr, "Modified QP state to RTR\n");
    rc = modify_qp_to_rts (qp);
    if (rc) {
      fprintf (stderr, "failed to modify QP state to RTR\n");
      goto connect_qp_exit;
    }
    /* sync to make sure that both sides are in states that they can connect to prevent packet loose */    
  connect_qp_exit:
    return rc;    
  }
};


/************************************/ 

class RDMAQueues::_RDMAQueuesImpl {
  
private:
  
  ibv_device **devList;
  int numDevices;
  _device *deviceContexts [MAX_DEVICES];
  int tcpPort;
  
  /* local address buffer && rdma registeration info */
  char *buffer;  
  uint64_t bufferSize;
  /*****/

  SimpleMap<_QP *> qps;
  
  /* the local qps are only left for connections */
  SimpleMap<_QP *> localQps;
  std::vector<std::string> netDef;
  
  /* TCP sockets for exchanging queue pairs */
  zmq::context_t *context;
  //  zmq::socket_t  *socket;

  static void *RecvThread(void *arg);
  int postReq(_QP *qp,char *localBuf,uint64_t offset,int len,enum ibv_wr_opcode op,
	      int send_flags = IBV_SEND_SIGNALED);
  
  int postAtomic(_QP *qp,char *localBuf,uint64_t offset, uint64_t compare,uint64_t swap,enum ibv_wr_opcode op);
    
  RDMAQueues::Status pollCompletion(_QP *qp);
  
public:
  /* Wrapper for low level usage */
  inline int postReq(int id,char *localBuf,uint64_t offset,int len,enum ibv_wr_opcode op,int sendflags) {
    return postReq(qps[id],localBuf,offset,len,op,sendflags);
  }
  
  int postReqs(int num,int *ids, char *localBuf,uint64_t *offs,int len,enum ibv_wr_opcode op,int sendflags = IBV_SEND_SIGNALED) ;

  RDMAQueues::Status pollCompletions(int num,int *ids);
    
  inline RDMAQueues::Status pollCompletion(int id) {
    return pollCompletion(qps[id]);
  }

  inline bool needPoll(int id) {
    _QP *q = qps[id];
    if(q->pendings > CAUSUAL_THRESHOLD - 1) {
      q->pendings = 0;
      return true;
    } else {
      q->pendings += 1;
      return false;
    }
  }
  /* Id of the current node */
  const int nodeId;
  
  _RDMAQueuesImpl(int id,int p,const std::vector<std::string> &net):
    nodeId(id),
    devList(NULL),
    numDevices(0),
    buffer(0),
    bufferSize(0),
    qps(NULL),
    localQps(NULL),
    tcpPort(p),
    netDef(net.begin(),net.end()),
    context(new zmq::context_t(1))
    //    socket( new zmq::socket_t(*context,ZMQ_REQ))
  {
    for(int i = 0;i < MAX_DEVICES;++i)
      deviceContexts[i] = NULL;
  }

  inline char *getBuffer(uint64_t *size) { 
    if(size != NULL)
      *size = bufferSize;
    return buffer; 
  }
  /* @ret: number of rdma devices on this mac,
           -1 on error. 
   */
  inline int queryDevice() {
    
    if(devList != NULL) 
      return numDevices;
    
    devList = ibv_get_device_list (&numDevices);
    
    if (!numDevices) {
      fprintf (stderr, "[RDMAIO] failed to get IB devices list\n");
      return -1;
    }
    return numDevices;  
  }

  inline int openDevice(int idx) {
    
    if(devList == NULL)
      this->queryDevice();
    if(numDevices == 0) 
      return -1;
    if(deviceContexts[idx] != NULL)
      return 0;
    
    /* return value */
    int rc(0);
    
    struct ibv_device *ibDev = NULL;    
    _device *dev = new _device;
    memset(dev,0,sizeof *dev);
    
    ibDev = devList[idx];    
    
    if(!ibDev) {
      fprintf (stderr, "IB device %d wasn't found\n", idx);
      rc = -1;
      goto OPEN_DEVICE_EXIT;
    }
    
    dev->ib_ctx = ibv_open_device(ibDev);
    
    if(!dev->ib_ctx) {
      fprintf (stderr, "failed to open device %d\n", idx);      
      rc = -1;
      goto OPEN_DEVICE_EXIT;
    }
   
    rc = ibv_query_device(dev->ib_ctx,&(dev->device_attr));     
    if(rc) {
      fprintf(stderr, "failed to query device %d\n",idx);
      rc = -1;
      goto OPEN_DEVICE_EXIT;      
    }

    if (ibv_query_port (dev->ib_ctx, rdmaConfig.ib_port, &dev->port_attr)) {
      fprintf (stderr, "ibv_query_port on port %u failed\n", rdmaConfig.ib_port);
      rc = 1;
      goto OPEN_DEVICE_EXIT;
    }    
    
    this->deviceContexts[idx] = dev;

  OPEN_DEVICE_EXIT:
    return rc;
  }

  inline int registerMemory(char *buf,uint64_t size,int idx,int flag) {
    
    int rc(0);
    
    _device *dev = this->deviceContexts[idx];
    
    if(dev == NULL) {
      rc = -1;
      goto REGISTER_END;
    }

    dev->pd = ibv_alloc_pd(dev->ib_ctx);
    
    if(!dev->pd) {
      fprintf (stderr, "ibv_alloc prodection doman failed at dev %d\n",idx);      
      rc = -1;
      goto REGISTER_END;
    }
    dev->mr = ibv_reg_mr(dev->pd,buf,size,flag);

    if(!dev->mr) {
      rc = -1;
      fprintf(stderr,"ibv reg mr failed at dev %d\n",idx);
      goto REGISTER_END;
    }
    
    dev->buf = buf;
    
    this->buffer = buf;
    this->bufferSize = size;
    
  REGISTER_END:
    if(rc < 0) {
      if (dev->mr){
	ibv_dereg_mr (dev->mr);	
	dev->mr = NULL;
      }
      if (dev->pd){
	ibv_dealloc_pd (dev->pd);
	dev->pd = NULL;
      }
    } 
    return rc;
  }

  inline void createQp(int id,int idx) {
    if(qps[id] == NULL) {

      qps.Insert(id,new _QP(deviceContexts[idx]));
    }
  }

  inline void connectLocalQp(int id,int idx) {

    if(localQps[id] != NULL)  {
      /* avoid double connections */
      return;
    }
    
    //    _QP *nqp = new _QP(deviceLocalContexts[idx]);
    //    localQps.Insert(id,nqp);
    //    assert(qps[id] != NULL);
    
    struct cm_con_data_t_ conData = qps[id]->getConData();
    qps[id]->connect(conData);
  }

  inline void connectQp(int id,int idx) {
    
    //    fprintf(stdout,"connect qp %d\n",id);
    char address[30];
    
    int mid = _QP_DECODE_MAC(id);
        
    snprintf(address,30,"tcp://%s:%d",netDef[mid].c_str(),tcpPort);
    //    socket->connect(address);
    zmq::socket_t socket(*context,ZMQ_REQ);
    socket.connect(address);
    //    fprintf(stdout,"connect to address %s\n",address);
    
    zmq::message_t request(sizeof(int));
    //    fprintf(stdout,"encode id %d %d\n",this->nodeId,_QP_DECODE_INDEX(id));
    *((int *)request.data()) = _QP_ENCODE_ID(this->nodeId,_QP_DECODE_INDEX(id));
    
  RETRY:
    socket.send(request);
    
    zmq::message_t reply;
    socket.recv(&reply);
    struct cm_con_data_t_ remote_con_data;

#if 1
    if( ((char *)reply.data())[0] != IO_SUCC) {
      assert(false);
      goto RETRY;
    }
#endif
    //    fprintf(stdout,"recv from %d\n",((char *)reply.data())[0]);
    
    memcpy(&remote_con_data,(char *)reply.data() + 1,sizeof(remote_con_data));
    qps[id]->connect(remote_con_data);
  }
  
  inline int getTCPPort() {
    return tcpPort;
  }

  inline _QP *getQp(int id) {
    return qps[id];
  }

  inline void startServer() {
    pthread_t tid;
    pthread_create(&tid, NULL, RecvThread, (void *)this);      
  }

  inline RDMAQueues::Status read(int qid,char *localBuffer,uint64_t remoteOffset,int size) {
    
    _QP *qp = qps[qid];
    if(unlikely(this->postReq(qp,localBuffer,remoteOffset,size,IBV_WR_RDMA_READ))) {
      return IO_ERR;
    }
    
    return pollCompletion(qp);
  }

  inline RDMAQueues::Status write(int qid,char *localBuffer,uint64_t remoteOffset,int size) {
    _QP *qp = qps[qid];
    if(unlikely(this->postReq(qp,localBuffer,remoteOffset,size,IBV_WR_RDMA_WRITE)))
      return IO_ERR;
    return pollCompletion(qp);
  }

  inline RDMAQueues::Status syncCompareSwap(int qid,char *localBuffer,uint64_t remoteOffset,
				     uint64_t compare,uint64_t swap) {
    _QP *qp = qps[qid];
    if(unlikely(this->postAtomic(qp,localBuffer,remoteOffset,compare,swap,IBV_WR_ATOMIC_CMP_AND_SWP)))
      return IO_ERR;
    return pollCompletion(qp);

  }

  inline int getNodeId() {
    return nodeId;
  }
  
};

/* Thread which will reply rdma connection requests */ 
void* RDMAQueues::_RDMAQueuesImpl::RecvThread(void * arg) { 
  
  RDMAQueues::_RDMAQueuesImpl *rdma = (RDMAQueues::_RDMAQueuesImpl *) arg;
  zmq::context_t context(1);
  zmq::socket_t socket(context,ZMQ_REP);
  
  char address[30]="";
  int port = rdma->getTCPPort();
  sprintf(address,"tcp://*:%d",port);
  fprintf(stdout,"RDMA listener binding: %s\n",address);
  socket.bind(address);

  //  char msg[sizeof "0000:000000:000000"];
  
  while(1) {
    
    zmq::message_t request;
    
    socket.recv(&request);
    
    int id =  *((int *)(request.data()));
    int mid = _QP_DECODE_MAC(id);
    int idx = _QP_DECODE_INDEX(id);
    
    _QP *qp = rdma->getQp(id);
    struct cm_con_data_t_ local_con_data;
    zmq::message_t reply(sizeof(local_con_data) + 1);    
    if(qp != NULL) {
      local_con_data = qp->getConData();    
      * (char *)(reply.data()) = IO_SUCC;      
      memcpy((char *)(reply.data()) + 1,(char *)(&local_con_data),sizeof(local_con_data));      
      //      fprintf(stdout,"recv req from %d, idx %d\n",mid,idx);
      //      fprintf(stdout,"local copy for %d %lu\n",id,ntohll(local_con_data.addr));
      //      memcpy((char *)(reply.data()),(char *)(&local_con_data),sizeof(local_con_data));      
    } else {
      * (char *)(reply.data()) = IO_ERR;
    }
    socket.send(reply);
  }
}

int RDMAQueues::_RDMAQueuesImpl
::postAtomic(_QP *qp,char *localBuf,uint64_t offset,uint64_t compare,uint64_t swap, enum ibv_wr_opcode op) {
  
  struct ibv_send_wr sr;
  struct ibv_sge sge;
  struct ibv_send_wr *bad_wr = NULL;
  int rc;
  /* prepare the scatter/gather entry */
  memset (&sge, 0, sizeof (sge));
  sge.addr = (uintptr_t) localBuf;
  sge.length = sizeof(uint64_t);
  sge.lkey = qp->mr->lkey;
  /* prepare the send work request */
  memset (&sr, 0, sizeof (sr));
  sr.next = NULL;
  sr.wr_id = 0;
  sr.sg_list = &sge;
  sr.num_sge = 1;
  sr.opcode = op;
  sr.send_flags = IBV_SEND_SIGNALED;
  
  sr.wr.atomic.remote_addr = qp->remote_props.addr + offset;
  sr.wr.atomic.rkey = qp->remote_props.rkey;
  sr.wr.atomic.compare_add = compare;
  sr.wr.atomic.swap = swap;
  
  /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
  rc = ibv_post_send (qp->qp, &sr, &bad_wr);
  if (rc)
    fprintf (stderr, "failed to post SR atomic\n");
  return rc;  
}

int RDMAQueues::_RDMAQueuesImpl
::postReqs(int num,int *ids,char *localBuf,uint64_t *offs,int len, enum ibv_wr_opcode op,int sendflags) {
  
  struct ibv_send_wr sr;
  struct ibv_sge *sge = new struct ibv_sge[num];
  struct ibv_send_wr *bad_wr = NULL;
  int rc,ret;
  ret = 0;
  /* prepare the scatter/gather entry */
  //  memset (&sge, 0, sizeof (sge));
  for(uint i = 0;i < num; ++i) {
    _QP *q = qps[ids[i]];
    
    memset(&sge[i],0,sizeof(sge));
    sge[i].addr = (uintptr_t) localBuf;
    sge[i].length = len;
    sge[i].lkey = q->mr->lkey;
    /* prepare the send work request */
    memset (&sr, 0, sizeof (sr));
    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &(sge[i]);
    sr.num_sge = 1;
    sr.opcode = op;
    sr.send_flags = sendflags;

    sr.wr.rdma.remote_addr = q->remote_props.addr + offs[i];//remte_addr is an unint64_t
    sr.wr.rdma.rkey = q->remote_props.rkey;

    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
    rc = ibv_post_send (q->qp, &sr, &bad_wr);
    
    if (rc){
      fprintf (stderr, "failed to post SR %d at qid %d\n",rc,ids[i]);
      ret = ret | rc;
    }
  }
  delete sge;
  return ret;
}

int RDMAQueues::_RDMAQueuesImpl
::postReq(_QP *qp,char *localBuf,uint64_t offset,int size,enum ibv_wr_opcode op,int sendflags) {
  
  struct ibv_send_wr sr;
  struct ibv_sge sge;
  struct ibv_send_wr *bad_wr = NULL;
  int rc;
  /* prepare the scatter/gather entry */
  memset (&sge, 0, sizeof (sge));
  sge.addr = (uintptr_t) localBuf;
  sge.length = size;
  sge.lkey = qp->mr->lkey;
  /* prepare the send work request */
  memset (&sr, 0, sizeof (sr));
  sr.next = NULL;
  sr.wr_id = 0;
  sr.sg_list = &sge;
  sr.num_sge = 1;
  sr.opcode = op;
  //  sr.send_flags = IBV_SEND_SIGNALED | IBV_SEND_FENCE;
  sr.send_flags = sendflags;
  
  sr.wr.rdma.remote_addr = qp->remote_props.addr + offset;//remte_addr is an unint64_t
  sr.wr.rdma.rkey = qp->remote_props.rkey;
  //  fprintf(stdout,"addr %lu qp num %d\n",qp->remote_props.addr,qp->remote_props.qp_num);
  //  fprintf (stdout, "\nread Local LID = 0x%x %d\n", qp->remote_props.lid,sr.wr.rdma.rkey);
  
  /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
  rc = ibv_post_send (qp->qp, &sr, &bad_wr);
  if (rc) {
    fprintf (stderr, "failed to post SR\n");
  }
  return rc;
  
}

RDMAQueues::Status RDMAQueues::_RDMAQueuesImpl::pollCompletions(int num,int *ids) {

  RDMAQueues::Status rc = IO_SUCC;
  
  int finish_counts = 0;
  struct ibv_wc wc;
  unsigned long start_time_msec;
  unsigned long cur_time_msec;
  struct timeval cur_time;
  int poll_result;

  gettimeofday(&cur_time, NULL);
  //poll completions
  /* poll the completion for a while before giving up of doing it .. */
  start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
  bool finishes[16]; //!!hard coded,assume RDMA reads will write to at most 16 machines
  memset(finishes,0,sizeof(bool) * 16);

  gettimeofday (&cur_time, NULL);
  
  do {
    
    for(uint i = 0;i < num;++i) {
      if(finishes[i])
        continue;
      
      poll_result = ibv_poll_cq( qps[ids[i]]->cq,1,&wc);   
      if(poll_result < 0) {
        finishes[i] = true;
        finish_counts++;
        rc = IO_ERR;	
      }else if( poll_result == 0 ) {
	continue;
      }
      else {
        if (wc.status != IBV_WC_SUCCESS)
          {
            fprintf (stderr,
                     "got bad completion with status: 0x%x, vendor syndrome: 0x%x at remote writes %s\n",
                     wc.status, wc.vendor_err,ibv_wc_status_str(wc.status));
            rc = IO_ERR;
          } else {
          //      result.push_back(pids[i]);
        }
        finishes[i] = true;
        finish_counts += 1;
        //end else
      }
      //end for loop
    }
    gettimeofday(&cur_time,NULL);
    cur_time_msec = (cur_time.tv_sec * 1000) +  (cur_time.tv_usec / 1000);
    //end while
  } while( (finish_counts < num ) &&  (cur_time_msec - start_time_msec) < MAX_POLL_CQ_TIMEOUT );
  
  if(finish_counts < num)
    rc = IO_TIMEOUT;
  return rc;  
}

RDMAQueues::Status RDMAQueues::_RDMAQueuesImpl::pollCompletion(_QP *qp) {
  
  struct ibv_wc wc;
  unsigned long start_time_msec;
  unsigned long cur_time_msec;
  struct timeval cur_time;
  
  int poll_result;
  RDMAQueues::Status rc = IO_SUCC;
  /* poll the completion for a while before giving up of doing it .. */
  gettimeofday (&cur_time, NULL);
  start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
  qp->pendings = 0;
  
  do
    {
      poll_result = ibv_poll_cq (qp->cq, 1, &wc);
      gettimeofday (&cur_time, NULL);
      cur_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    }  
  while ((poll_result == 0)
	 && ((cur_time_msec - start_time_msec) < MAX_POLL_CQ_TIMEOUT));
  
  if (poll_result < 0)
    {
      /* poll CQ failed */
      fprintf (stderr, "poll CQ failed\n");
      rc = IO_ERR;
    }
  else if (poll_result == 0)
    {
      /* the CQ is empty */
      fprintf (stderr, "completion wasn't found in the CQ after timeout\n");
      //      rc = IO_TIMEOUT;
      rc = IO_ERR;
    }
  else
    {
      /* CQE found */
      // fprintf (stdout, "completion was found in CQ with status 0x%x\n",
      //          wc.status);
      /* check the completion status (here we don't care about the completion opcode */
      if (wc.status != IBV_WC_SUCCESS)
	{
	  fprintf (stderr,
		   "got bad completion with status: 0x%x, vendor syndrome: 0x%x, with error %s\n",
		   wc.status, wc.vendor_err,ibv_wc_status_str(wc.status));
	  rc = IO_ERR;
	}
    }
  return rc;  
}


/************************************/ 
/* real implementations of rdma lib */

RDMAQueues::RDMAQueues (int nodeId,int port, const std::vector<std::string> &netDef ):
  rdmap(new _RDMAQueuesImpl(nodeId,port,netDef))
{
  
}

int RDMAQueues::queryDevice() {
  return rdmap->queryDevice();
}

int RDMAQueues::openDevice(int idx) {
  return rdmap->openDevice(idx);
}

int RDMAQueues::registerMemory(char *buffer,uint64_t size,int idx,int flag) {
  return rdmap->registerMemory(buffer,size,idx,flag);
}

void RDMAQueues::createQp(int id,int idx) {
  return rdmap->createQp(id,idx);
}

void RDMAQueues::connectQp(int id,int idx) {
  return rdmap->connectQp(id,idx);
}

void RDMAQueues::connectLocalQp(int id,int idx) {
  return rdmap->connectLocalQp(id,idx);
}

void RDMAQueues::startServer() {
  return rdmap->startServer();
}

RDMAQueues::Status RDMAQueues::read(int id,char *localBuf,uint64_t remoteOffset,int size) {
  return rdmap->read(id,localBuf,remoteOffset,size);
}

RDMAQueues::Status RDMAQueues::write(int id,char *localBuf,uint64_t remoteOffset,int size) {
  return rdmap->write(id,localBuf,remoteOffset,size);
}

RDMAQueues::Status RDMAQueues::syncCompareSwap(int id,char *localBuf,uint64_t remoteOffset,
					       uint64_t compare,uint64_t swap) {
  return rdmap->syncCompareSwap(id,localBuf,remoteOffset,compare,swap);
}

char *RDMAQueues::getBuffer(uint64_t *size) {
  return rdmap->getBuffer(size);
}

int RDMAQueues::getNodeId() {
  return rdmap->getNodeId();
}

int RDMAQueues::postReq(int id,char *localBuf,uint64_t offset,int len,enum ibv_wr_opcode op,int sendflags) {
  return rdmap->postReq(id,localBuf,offset,len,op,sendflags);
}

int RDMAQueues::postReqs(int num,int *ids,char *localBuf,uint64_t *offs,int len,enum ibv_wr_opcode op,int sendflags) {
  return rdmap->postReqs(num,ids,localBuf,offs,len,op,sendflags);
}

RDMAQueues::Status RDMAQueues::pollCompletion(int id) {
  return rdmap->pollCompletion(id);
}

RDMAQueues::Status RDMAQueues::pollCompletions(int num,int *ids) {
  return rdmap->pollCompletions(num,ids);
}

bool RDMAQueues::needPoll(int id) {
  return rdmap->needPoll(id);
}

RDMAQueues *bootstrapRDMA(int nodeId,int port,std::vector<std::string> &netdef,
			  int qpPerMac,char *buffer,uint64_t bufSize) {
  
  RDMAQueues *rdma = new RDMAQueues(nodeId,port,netdef);
  
  int num = rdma->queryDevice();
  if(num <= 0) {
    fprintf(stderr,"No devices on this mac or some errors happends.\n");
    return NULL;
  }
  
  /* In default we use the first NIC device */
  int ret = rdma->openDevice(0);
  
  if(ret) {
    fprintf(stderr,"Open the first device error!\n");
    return NULL;
  }

  ret = rdma->registerMemory(buffer,bufSize,0);
  if(ret) {
    fprintf(stderr,"Registering the memory for the first device error!\n");
    return NULL;
  }

  /* Start creating QPs */
  for(int i = 0;i < netdef.size();++i) {
    for(int j = 0;j < qpPerMac;++j) 
      rdma->createQp(_QP_ENCODE_ID(i,j),0);
  }
  
  rdma->startServer();
  
  for(int i = 0;i < netdef.size();++i) {
    for(int  j = 0;j < qpPerMac;++j) {
      if(i == nodeId)
	rdma->connectLocalQp(_QP_ENCODE_ID(i,j),0);
      else
	rdma->connectQp(_QP_ENCODE_ID(i,j),0);
    }
  }
  
  return rdma;
}


/**********************************************************/

static int
modify_qp_to_init (struct ibv_qp *qp)
{
  struct ibv_qp_attr attr;
  int flags;
  int rc;
  memset (&attr, 0, sizeof (attr));
  attr.qp_state = IBV_QPS_INIT;
  attr.port_num = rdmaConfig.ib_port;
  attr.pkey_index = 0;
  attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
    IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;
  
  flags =
    IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
  rc = ibv_modify_qp (qp, &attr, flags);
  if (rc)
    fprintf (stderr, "failed to modify QP state to INIT\n");
  return rc;
}


static int
modify_qp_to_rtr (struct ibv_qp *qp, uint32_t remote_qpn, uint16_t dlid,
                  uint8_t * dgid)
{
  struct ibv_qp_attr attr;
  int flags;
  int rc;
  memset (&attr, 0, sizeof (attr));
  attr.qp_state = IBV_QPS_RTR;
  attr.path_mtu = IBV_MTU_256;
  attr.dest_qp_num = remote_qpn;
  attr.rq_psn = 0;
  attr.max_dest_rd_atomic = 1;
  attr.min_rnr_timer = 0x12;
  attr.ah_attr.is_global = 0;
  attr.ah_attr.dlid = dlid;
  attr.ah_attr.sl = 0;
  attr.ah_attr.src_path_bits = 0;
  attr.ah_attr.port_num = rdmaConfig.ib_port;
  if (rdmaConfig.gid_idx >= 0)
    {
      attr.ah_attr.is_global = 1;
      attr.ah_attr.port_num = 1;
      memcpy (&attr.ah_attr.grh.dgid, dgid, 16);
      attr.ah_attr.grh.flow_label = 0;
      attr.ah_attr.grh.hop_limit = 1;
      attr.ah_attr.grh.sgid_index = rdmaConfig.gid_idx;
      attr.ah_attr.grh.traffic_class = 0;
    }
  flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
    IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
  rc = ibv_modify_qp (qp, &attr, flags);
  if (rc)
    fprintf (stderr, "failed to modify QP state to RTR\n");
  return rc;
}


static int
modify_qp_to_rts (struct ibv_qp *qp)
{
  struct ibv_qp_attr attr;
  int flags;
  int rc;
  memset (&attr, 0, sizeof (attr));
  attr.qp_state = IBV_QPS_RTS;
  attr.timeout = 0x12;
  attr.retry_cnt = 6;
  attr.rnr_retry = 0;
  attr.sq_psn = 0;
  attr.max_rd_atomic = 1;
  attr.max_dest_rd_atomic = 1;
  flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
    IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
  rc = ibv_modify_qp (qp, &attr, flags);
  if (rc)
    fprintf (stderr, "failed to modify QP state to RTS\n");
  return rc;
}
