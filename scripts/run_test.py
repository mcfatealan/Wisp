import rdma
import rdma.ibverbs as ibv


def run_server():
    print 'server running'
    pass

def run_client():
    print 'client running'
    pass

def run_selftest():
    end_port = rdma.get_end_port()
    with rdma.get_verbs(end_port) as ctx:
        pd = ctx.pd();


def main():
    if len(sys.argv) ==1:
        print 'parameter not enough'
        return


    if sys.argv[1]=='server':
        run_server()
    elif sys.argv[1]=='client':
        run_client()
    elif sys.argv[1]=='selftest':
        run_selftest()
    else:
        print 'wrong parameter: should be server or client'
        return
    


if __name__ == "__main__":
    main()
