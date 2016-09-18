import sys
import rdma
import rdma.ibverbs as ibv
import rdma.IBA as IBA
from collections import namedtuple
import socket
import contextlib
import rdma.vtools
from mmap import mmap
import pickle
import time
from rdma.tools import clock_monotonic
from rdma.vtools import BufferPool

ip_port = 4444
tx_depth = 100
memsize = 1024

infotype = namedtuple('infotype', 'path addr rkey size iters')

class Endpoint(object):
    ctx = None;
    pd = None;
    cq = None;
    mr = None;
    peerinfo = None;

    def __init__(self,sz,dev):
        self.ctx = rdma.get_verbs(dev)
        self.cc = self.ctx.comp_channel();
        self.cq = self.ctx.cq(2*tx_depth,self.cc)
        self.poller = rdma.vtools.CQPoller(self.cq);
        self.pd = self.ctx.pd()
        self.qp = self.pd.qp(ibv.IBV_QPT_RC,tx_depth,self.cq,tx_depth,self.cq);
        self.mem = mmap(-1, sz)
        self.mr = self.pd.mr(self.mem,
                             ibv.IBV_ACCESS_LOCAL_WRITE|ibv.IBV_ACCESS_REMOTE_WRITE)
        self.pool = BufferPool(self.pd,2*16,256+40)

    def __enter__(self):
        return self;

    def __exit__(self,*exc_info):
        return self.close();

    def close(self):
        if self.ctx is not None:
            self.ctx.close();

    def connect(self, peerinfo):
        self.peerinfo = peerinfo
        self.qp.establish(self.path,ibv.IBV_ACCESS_REMOTE_WRITE);

    def write(self):
        swr = ibv.send_wr(wr_id=0,
                          remote_addr=self.peerinfo.addr,
                          rkey=self.peerinfo.rkey,
                          sg_list=self.mr.sge(),
                          opcode=ibv.IBV_WR_RDMA_WRITE,
                          send_flags=ibv.IBV_SEND_SIGNALED)

        n = 1
        depth = min(tx_depth, n, self.qp.max_send_wr)

        tpost = clock_monotonic()
        for i in xrange(depth):
            self.qp.post_send(swr)

        completions = 0
        posts = depth
        for wc in self.poller.iterwc(timeout=3):
            if wc.status != ibv.IBV_WC_SUCCESS:
                raise ibv.WCError(wc,self.cq,obj=self.qp);
            completions += 1
            if posts < n:
                self.qp.post_send(swr)
                posts += 1
                self.poller.wakeat = rdma.tools.clock_monotonic() + 1;
            if completions == n:
                break;
        else:
            raise rdma.RDMAError("CQ timed out");

        tcomp = clock_monotonic()
    def send(self,buf):
        buf_idx = self.pool.pop();
        self.pool.copy_to(buf,buf_idx);
        self.qp.post_send(self.pool.make_send_wr(buf_idx,len(buf),self.path));
    
    def recv(self):
        self.pool.post_recvs(self.qp,1)
def run_server(dev):
    database = {}

    def execute(command):
        command = command.split(' ', 1)
        if command[0] in 'GET get Get':
            res = database.get(command[1])
            print command[1]+' :', res
            return str(res) 
        elif command[0] in 'PUT put Put SET set Set':
            command = command[1].split(' ', 1)
            if len(command)==1:
                print 'No value set'
                return 'FAIL'
            else:
                database[command[0]] = command[1]
                print command[0]+' : '+command[1]
                return 'SUCCESS'
        elif command[0] in 'DEL del Del DELETE delete Delete':
            database.pop(command[1], None)
            print command[1]+' : None'
            return 'SUCCESS'
            
        else:
            print 'Wrong command'
            return 'FAIL'

        #print database

    print 'server running..'
    ret = socket.getaddrinfo(None,str(ip_port),0,
                             socket.SOCK_STREAM,0,
                             socket.AI_PASSIVE);
    ret = ret[0];
    with contextlib.closing(socket.socket(ret[0],ret[1])) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(ret[4]);
        sock.listen(1)

        print "Listening port..."
        s,addr = sock.accept()
        with contextlib.closing(s):
            buf = s.recv(1024)
            peerinfo = pickle.loads(buf)

            endPointStartTime = time.time();
            with Endpoint(peerinfo.size, dev) as end:
                with rdma.get_gmp_mad(end.ctx.end_port,verbs=end.ctx) as umad:
                    end.path = peerinfo.path;
                    end.path.end_port = end.ctx.end_port;
                    rdma.path.fill_path(end.qp,end.path);
                    rdma.path.resolve_path(umad,end.path);

                s.send(pickle.dumps(infotype(path=end.path,
                                             addr=end.mr.addr,
                                             rkey=end.mr.rkey,
                                             size=None,
                                             iters=None)))

                print "path to peer %r\nMR peer raddr=%x peer rkey=%x"%(
                    end.path,peerinfo.addr,peerinfo.rkey);

                end.connect(peerinfo)

                    # Synchronize the transition to RTS
                s.send("ready");
                s.recv(1024);
                    
                while True:
                    s.recv(1024);
                    end.mem.seek(0)
                    str_received = end.mem.readline().split('\0', 1)[0]
                    print 'Received: ' + str_received
                    str_response = execute(str_received)
                    end.send(str_response)


                s.shutdown(socket.SHUT_WR);
                s.recv(1024);

                
def run_client(hostname,dev):
    print 'client running..'
    with Endpoint(memsize, dev) as end:
        ret = socket.getaddrinfo(hostname,str(ip_port),0,
                                 socket.SOCK_STREAM);
        ret = ret[0];
        with contextlib.closing(socket.socket(ret[0],ret[1])) as sock:
            sock.connect(ret[4]);

            path = rdma.path.IBPath(dev,SGID=end.ctx.end_port.default_gid);
            rdma.path.fill_path(end.qp,path,max_rd_atomic=0);
            path.reverse(for_reply=False);

            sock.send(pickle.dumps(infotype(path=path,
                                            addr=end.mr.addr,
                                            rkey=end.mr.rkey,
                                            size=memsize,
                                            iters=1)))
            buf = sock.recv(1024)
            peerinfo = pickle.loads(buf)

            end.path = peerinfo.path;
            end.path.reverse(for_reply=False);
            end.path.end_port = end.ctx.end_port;

            print "path to peer %r\nMR peer raddr=%x peer rkey=%x"%(
                end.path,peerinfo.addr,peerinfo.rkey);

            end.connect(peerinfo)
            
            # Synchronize the transition to RTS
            sock.send("Ready");
            sock.recv(1024);

            while True:
                str_to_send = raw_input()
                end.mem.seek(0)
                end.mem.write(str_to_send+'\0')
                end.write()
                sock.send("Sent");
                print 'Sent: ' + str_to_send
                end.recv()
                end.pool.

            sock.shutdown(socket.SHUT_WR);
            sock.recv(1024);


            print "---client end"
        print "---sock close"
    print "--- endpoint close"

def main():
    usage_str = '''
    Usage: python ./simple_kv.py server
           python ./simple_kv.py client hostname
    '''
    
    if len(sys.argv) ==1:
        print 'parameter not enough'
        print usage_str
        return


    if sys.argv[1]=='server':
        run_server(rdma.get_end_port())
    elif sys.argv[1]=='client':
        run_client(sys.argv[2], rdma.get_end_port())
    else:
        print 'wrong parameter: should be server or client'
        print usage_str
        return
    


if __name__ == "__main__":
    main()
