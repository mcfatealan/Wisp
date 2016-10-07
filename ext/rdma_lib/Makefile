base_tests:
	g++ -I. -pthread -L/usr/local/ofed/lib64 -L/usr/local/ofed/lib -std=c++0x rdma_test.cc rdmaio.cc -o test -libverbs -lzmq

client_tests:
	g++ -I. -pthread -L/usr/local/ofed/lib64 -L/usr/local/ofed/lib -std=c++0x rdma_client_tests.cc rdmaio.cc rdma_msg.cc -o test -libverbs -lzmq

clean:
	rm test;

