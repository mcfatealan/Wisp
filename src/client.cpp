#include "client.h"

void Client::run_tcp()
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
void Client::run_perftest_tcp(int keysize, int payloadsize)
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
                value_str = "";
                break;
            case 2:
                op_str = "del";
                value_str = "";
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
    std::cout << "It takes " << interval <<" seconds to finish "<<test_size<<" requests."<<std::endl;
    std::cout << "The qps is " << qps <<" requests/second."<<std::endl;
}

void Client::run_rdma()
{

    int id = 0;
    char *send_buffer,*recv_buffer;

    std::vector<std::string> netDef;
    netDef.push_back(clientAddr);
    netDef.push_back(serverAddr);

    uint64_t bufSize = 1024*1024*1024;
    bufSize *= 4;
    send_buffer = new char[bufSize];
    recv_buffer = new char[bufSize];

    int send_port = 5555,recv_port=5556;
    int qpPerMac = 4;

    RDMAQueues *send_rdma = bootstrapRDMA(0,send_port,netDef,qpPerMac,send_buffer,bufSize);
    RDMAMessage *send_msg = new RDMAMessage(0,2,send_rdma,send_buffer);
    RDMAQueues *recv_rdma = bootstrapRDMA(0,recv_port,netDef,qpPerMac,recv_buffer,bufSize);
    RDMAMessage *recv_msg = new RDMAMessage(0,2,recv_rdma,recv_buffer);

    std::cout << "--------------------------------------------------------------------------------\n"
              << "Please input request: "<< std::endl;
    char *req_str = new char[bufSize];
    char resp_str[1024] = {0};
    while(1)
    {
        std::cout << "> ";
        std::cin.getline(req_str, bufSize);

        send(send_msg,req_str);
        std::cout << "Send "<<req_str<<std::endl;

        recv(recv_msg,resp_str,1);
        std::cout << "Recv "<<resp_str<<std::endl;

    }

}

void Client::run_perftest_rdma(int keysize, int payloadsize)
{

    int id = 0;
    char *send_buffer,*recv_buffer;

    std::vector<std::string> netDef;
    netDef.push_back(clientAddr);
    netDef.push_back(serverAddr);

    uint64_t bufSize = 1024*1024*1024;
    bufSize *= 4;
    send_buffer = new char[bufSize];
    recv_buffer = new char[bufSize];

    int send_port = 5555,recv_port=5556;
    int qpPerMac = 4;

    RDMAQueues *send_rdma = bootstrapRDMA(0,send_port,netDef,qpPerMac,send_buffer,bufSize);
    RDMAMessage *send_msg = new RDMAMessage(0,2,send_rdma,send_buffer);
    RDMAQueues *recv_rdma = bootstrapRDMA(0,recv_port,netDef,qpPerMac,recv_buffer,bufSize);
    RDMAMessage *recv_msg = new RDMAMessage(0,2,recv_rdma,recv_buffer);

    auto test_size = 100000;

    auto max =pow(10,keysize);
    std::string sample_str = std::string(payloadsize, 'a');
    std::cout << "--------------------------------------------------------------------------------\n"
              << "Start perf test.. "<< std::endl;
    srand(time(NULL));
    auto time_start = gettimestamp();

    auto count =0;
    std::string op_str,key_str,value_str;
    char req_str[1024]={0};
    char resp_str[1024] = {0};
    while (count<test_size)
    {
        key_str = std::to_string(rand() % (int)(max + 1));
        switch (rand() %3)
        {
            case 0:
                op_str = "put";
                value_str = " "+sample_str;
                break;
            case 1:
                op_str = "get";
                value_str = "";
                break;
            case 2:
                op_str = "del";
                value_str = "";
                break;
        }
        strcpy(req_str,(op_str+" "+key_str+value_str).c_str());
        send(send_msg,req_str);
        //std::cout << req_str<<std::endl;

        recv(recv_msg,resp_str,1);
        //std::cout << resp_str<<std::endl;

        count +=1;
        //std::cout << count<<std::endl;
    }
    auto interval = gettimeinterval(time_start,gettimestamp());
    auto qps = test_size/interval;
    std::cout << "perf test ended.\n"
              << "--------------------------------------------------------------------------------\n"
              << "It takes " << interval <<" seconds to finish "<<test_size<<" requests."<<std::endl
              << "The qps is " << qps <<" requests/second."<<std::endl
              << "--------------------------------------------------------------------------------\n";



}
