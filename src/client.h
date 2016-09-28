#include <string>
#include <iostream>
#include <zmq.hpp>

class Client
{
private:
    std::string server_address;

    zmq::context_t context = zmq::context_t(1);
    zmq::socket_t socket = zmq::socket_t(context, ZMQ_REQ);

public:
    Client(std::string sa)
    {
        std::cout << "client running..."<<std::endl;
        
        server_address = sa;

        std::cout << "connecting "<< server_address <<"..."<<std::endl;
        socket.connect ("tcp://"+server_address+":3994");
    }
    ~Client()
    {
        std::cout << "finished. "<<std::endl;
    }

    void run();

};
