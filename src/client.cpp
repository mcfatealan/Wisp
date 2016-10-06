#include "client.h"

void Client::run()
{
    std::cout << "Input request.. "<< std::endl;
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
void Client::run_perftest(int keysize, int payloadsize)
{
    auto test_size = 100000;

    auto max =pow(10,keysize);
    std::string sample_str = std::string(payloadsize, 'a');
    std::cout << "Start perf test.. "<< std::endl;
    srand(time(NULL));
    auto time_start = gettimestamp();

    auto count =0;
    while (count<test_size)
    {
        std::string op_str,key_str,value_str;
        char req_str[1024]={0};

        key_str = std::to_string(rand() % (int)(max + 1));
        switch (rand() %3)
        {
            case 0:
                op_str = "put";
                value_str = " "+sample_str;
                break;
            case 1:
                op_str = "get";
                break;
            case 2:
                op_str = "del";
                break;
        }
        strcpy(req_str,(op_str+" "+key_str+value_str).c_str());

        zmq::message_t request (1024);
        memcpy (request.data (), req_str, 1024);
        socket.send (request);

        //  Get the reply.
        zmq::message_t reply;
        socket.recv (&reply);
        count +=1;
    }
    auto interval = gettimeinterval(time_start,gettimestamp());
    auto qps = test_size/interval;
    std::cout << "It takes " << interval <<" seconds to finish 1000000 requests."<<std::endl;
    std::cout << "The qps is " << qps <<" requests/second."<<std::endl;
}

