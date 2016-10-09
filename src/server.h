#include <iostream>
#include <libcuckoo/cuckoohash_map.hh>
#include <libcuckoo/city_hasher.hh>
#include <zmq.hpp>
#include "util.h"
#include "rdma.h"
#include <sys/socket.h>
#include <arpa/inet.h>

enum KV_ERR {ERR_OK, ERR_FAIL};
enum OP {OP_PUT, OP_GET, OP_DEL};

class Server
{
private:
    cuckoohash_map<std::string, std::string> Table;

    zmq::context_t context = zmq::context_t(1);
    zmq::socket_t socket = zmq::socket_t(context, ZMQ_REP);

    std::string clientAddr;
    std::string serverAddr;

public:

    Server()
    {
        serverAddr = gethostname();

        std::cout << "server running on "<<serverAddr<<"..." <<std::endl;
        
        socket.bind ("tcp://*:3994");

        //recv client's ip addr
        zmq::message_t request;
        socket.recv(&request);
        char clientip[32] = {0};
        memcpy(clientip, request.data(), 32);
        clientAddr = std::string(clientip);
        std::cout << "recv connection from "<<clientAddr <<std::endl;

    }

    ~Server()
    {
        std::cout << "finished. "<<std::endl;
    }

    std::string execute_request(std::string request);
    bool parse_request(std::string request,OP &op,std::string &arg1, std::string &arg2);

    KV_ERR put(std::string key, std::string value);
    KV_ERR get(std::string key, std::string &value);
    KV_ERR del(std::string key);

    void run_tcp();
    void run_rdma();
};
