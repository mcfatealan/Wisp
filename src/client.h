#include <string>
#include <iostream>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <zmq.hpp>
#include "util.h"
#include "rdma.h"


class Client
{
private:

    zmq::context_t context = zmq::context_t(1);
    zmq::socket_t socket = zmq::socket_t(context, ZMQ_REQ);

    std::string clientAddr;
    std::string serverAddr;

public:
    Client(std::string sa)
    {
        serverAddr = sa;
        clientAddr =gethostname();

        std::cout << "client running on "<<clientAddr<<"..."<<std::endl;


        std::cout << "connecting "<< serverAddr <<"..."<<std::endl;
        socket.connect ("tcp://"+serverAddr+":3994");

        //send ip addr
        zmq::message_t request (32);
        memcpy (request.data (), clientAddr.c_str(), 32);
        socket.send (request);

    }
    ~Client()
    {
        std::cout << "finished. "<<std::endl;
    }

    void run_tcp();
    void run_rdma();
    void run_perftest_tcp(int keysize, int payloadsize);
    void run_perftest_rdma(int keysize, int payloadsize);

};
