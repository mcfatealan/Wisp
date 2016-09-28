#include "client.h"

void Client::run()
{
    while(1)
    {
        char req_str[1024]={0};
        std::cin.getline(req_str, 1024);
        zmq::message_t request (1024);
        memcpy (request.data (), req_str, 1024);
        std::cout << "Sending  " << req_str << "..." << std::endl;
        socket.send (request);

        //  Get the reply.
        zmq::message_t reply;
        socket.recv (&reply);
        char reply_str[1024]={0};

        memcpy(reply_str,reply.data(),1024);
        std::cout << "Received "<< reply_str << std::endl;
    }


}

