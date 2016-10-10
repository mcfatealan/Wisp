# Wisp
Wisp is a simple key-value store service specially designed for RDMA-enhanced network.

##dependency
* zeromq 4.0.5 or higher
* Mellanox OFED v3.0-2.0.1 stack or higher
* libcuckoo

##usage
```
chlou@cube0:~/Wisp$ ./wisp
--------------------------------------------------------------------------------
> Wisp v0.1.0
--------------------------------------------------------------------------------
Usage:
  [-h]                                                        help
  [-s]                                                        run as server
  [-c SERVER_ADDRESS]                                         run as client
  [-p SERVER_ADDRESS] [--key_size key_size] [--payload_size payload_size]
                                                              run as client_perf
  [-t]                                                        use tcp instead of rdma
--------------------------------------------------------------------------------

[ Examples ]

< simple cli >
  server: ./wisp -s
  client: ./wisp -c 10.0.0.100
< simple cli using tcp >
  server: ./wisp -s -t
  client: ./wisp -c 10.0.0.100 -t
< perf test >
  server: ./wisp -s
  client: ./wisp -p 10.0.0.100 --key_size 3 --payload_size 10
--------------------------------------------------------------------------------
```

##update
in `scripts`, add `simple_kv`: a python-implemented key/value store service with performance test.
This script is experimental. The test shows that the performance of `python-rdma` is not so promising.
On occasion that `keysize=3,payloadsize=100`, the qps is only around 20000. 
The main reason the `python-rdma` write is very costful and costs around 30 us to finish.
That's even under the premise that the test is boosted with some tricks like preheat the server by sending some requests before the formal test so the client needn't wait until the requests finish.

##acknowledgment
some codes comes from Xinda Wei's `rdma_lib`(http://ipads.se.sjtu.edu.cn:1312/weixd/rdma_lib), thanks Xinda!
