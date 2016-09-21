# Wisp
Wisp is a simple key-value storage database specially designed for RDMA-enhanced network.

##update
add python-implemented simple-kv and test performance, for `keysize=3,payloadsize=100`, the qps is around 20000. 
That's because the python-rdma write is very costful and around 30 us to finish.
The test is even boosted with some tricks like preheat the server by sending some requests before the formal test,
so the client needn't wait until the requests finish.
