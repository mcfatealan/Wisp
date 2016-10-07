#ifndef _RDMA_IO_
#define _RDMA_IO_

/* Simple wrapper to verbs API. 
   Provides functionality for QP connections and communications, 
  also for detecting failure. 
*/

#include <vector>
#include <string>
#include <memory>
#include <rdma/rdma_verbs.h>

#define _RDMA_INDEX_MASK (0xffff)
#define _RDMA_MAC_MASK (_RDMA_INDEX_MASK << 16)

#define _QP_ENCODE_ID(mac,index) (mac << 16 | index)
#define _QP_DECODE_MAC(id) ((id & _RDMA_MAC_MASK) >> 16 )
#define _QP_DECODE_INDEX(id) (id & _RDMA_INDEX_MASK)

#define DEFAULT_PROTECTION_FLAG ( IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | \
				  IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC)


class RDMAQueues {
  
 public:
  RDMAQueues (int nodeId,int tcpPort, const std::vector<std::string> &netDef);
  enum Status {
    IO_SUCC,
    IO_TIMEOUT,
    IO_ERR
  };
  
  /* Not thread-safe methods!! */  
  int queryDevice();
  int openDevice(int idx);  
  char *getBuffer(uint64_t *size = NULL);
  void createQp(int id,int idx);
  void connectQp(int id,int idx);
  void connectLocalQp(int id,int idx);
  
  /* Basic RDMA functionalities */
  Status read(int qid, char *localBuf,uint64_t remoteOffset,int size);
  Status write(int qid,char *localBuf,uint64_t remoteOffset,int size);
  Status syncCompareSwap(int qid,char *localBuf,uint64_t remoteOffset,uint64_t compare,uint64_t swap);  

  /* Low level operations */
  int postReq(int id,char *localBuf,uint64_t offset,int len,enum ibv_wr_opcode op,int sendflags);
  int postReqs(int num,int *ids,char *localBuf,uint64_t *offs,int len,enum ibv_wr_opcode op,int sendflags);
  RDMAQueues::Status pollCompletion(int id);
  RDMAQueues::Status pollCompletions(int num,int *ids);
  /* Return true which means that this QP needs poll completion. 
     If return false, the pending requests will increase by one
   */
  bool needPoll(int id);
  
  /*
   * @ret -1 on error, 0 on success
   */
  int registerMemory(char *buf,uint64_t size,int idx,int flag = DEFAULT_PROTECTION_FLAG);
  /* must be called only once!! */
  void startServer();
  int getNodeId();

  
 private:
  class _RDMAQueuesImpl;
  std::unique_ptr<_RDMAQueuesImpl> rdmap;
  
};

/*
 * Simplest way to establish an RDMA connection given a netdef.
 * @Input
 *  nodeId: Id of the current node.
 *  port:   tcp port for node to communicate using TCP
 *  netdef: <id,IP> pairs
 *  qpPerMac: number of qp used for this mac (!! actually the true number of qp alloced
 * on this server is qpPerMac * netdef.size() )
 *
 * @Ret
 *  NULL on error, or a poniter that can be used.
 *  Note, this function will block if the connection is not established.
 */

RDMAQueues *bootstrapRDMA(int nodeId,int port,std::vector<std::string> &netdef,
			  int qpPerMac,char *buffer,uint64_t bufSize);


#endif
