#include <iostream>
#include <libcuckoo/cuckoohash_map.hh>
#include <libcuckoo/city_hasher.hh>
#include <zmq.hpp>
#include "util.h"
#include "../ext/rdma_lib/rdma_msg.h"

enum KV_ERR {ERR_OK, ERR_FAIL};
enum OP {OP_PUT, OP_GET, OP_DEL};

class Server
{
private:
    cuckoohash_map<std::string, std::string> Table;

    zmq::context_t context = zmq::context_t(1);
    zmq::socket_t socket = zmq::socket_t(context, ZMQ_REP);
    
public:

    Server()
    {
        std::cout << "server running..." <<std::endl;
        
        socket.bind ("tcp://*:3994");
        
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

    void run();
};
