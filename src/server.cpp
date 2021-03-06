#include "server.h"
void Server::run_tcp() {
    while (true) {
        zmq::message_t request;

        //  Wait for next request from client
        socket.recv(&request);
        char request_str[1024] = {0};
        memcpy(request_str, request.data(), 1024);
        std::cout << "Received " << request_str << std::endl;

        //  Do some 'work'
        std::string response_str = execute_request(std::string(request_str));

        //  Send reply back to client
        zmq::message_t reply(1024);
        std::cout << "Send " << response_str << std::endl;
        memcpy(reply.data(), response_str.c_str(), 1024);
        socket.send(reply);
    }
}

std::string Server::execute_request(std::string request) {
    //may use cityhash
    OP op;
    std::string arg1, arg2;
    if (parse_request(request, op, arg1, arg2)) {
        switch (op) {
            case OP_PUT:
                if (put(arg1, arg2) == ERR_OK) {
                    return std::string("Put suc");
                } else
                    return std::string("Put fail");
            case OP_GET:
                if (get(arg1, arg2) == ERR_OK) {
                    return arg2;
                } else
                    return std::string("Get fail");
            case OP_DEL:
                if (del(arg1) == ERR_OK) {
                    return std::string("Del suc");
                } else
                    return std::string("Del fail");
            default:
                return std::string("Wrong op");
        }
    } else {
        return std::string("Request can not be parsed");
    }


}

KV_ERR Server::put(std::string key, std::string value) {
    if (Table.insert(key, value))
        return ERR_OK;
    else if (Table.update(key, value))
        return ERR_OK;
    else
        return ERR_FAIL;
}

KV_ERR Server::get(std::string key, std::string &value) {
    if (Table.find(key, value))
        return ERR_OK;
    else
        return ERR_FAIL;
}

KV_ERR Server::del(std::string key) {
    if (Table.erase(key))
        return ERR_OK;
    else
        return ERR_FAIL;
}

bool Server::parse_request(std::string request, OP &op, std::string &arg1, std::string &arg2) {
    if (request.substr(0, 3).compare("put") == 0) {
        op = OP_PUT;
        vector<std::string> kv = split(request.substr(4, string::npos), ' ');
        arg1 = kv[0];
        arg2 = kv[1];
    } else if (request.substr(0, 3).compare("get") == 0) {
        op = OP_GET;
        arg1 = request.substr(4, string::npos);
    } else if (request.substr(0, 3).compare("del") == 0) {
        op = OP_DEL;
        arg1 = request.substr(4, string::npos);
    }
    return true;
}

void Server::run_rdma() {
    int id = 1;

    std::vector<std::string> netDef;
    netDef.push_back(clientAddr);
    netDef.push_back(serverAddr);

    char *send_buffer,*recv_buffer;

    uint64_t bufSize = 1024*1024*1024;
    bufSize *= 4;
    send_buffer = new char[bufSize];
    recv_buffer = new char[bufSize];

    int send_port = 5556,recv_port=5555;
    int qpPerMac = 4;

    RDMAQueues *recv_rdma = bootstrapRDMA(1,recv_port,netDef,qpPerMac,recv_buffer,bufSize);
    RDMAMessage *recv_msg = new RDMAMessage(0,2,recv_rdma,recv_buffer);
    RDMAQueues *send_rdma = bootstrapRDMA(1,send_port,netDef,qpPerMac,send_buffer,bufSize);
    RDMAMessage *send_msg = new RDMAMessage(0,2,send_rdma,send_buffer);

    std::cout<< "--------------------------------------------------------------------------------\n"
             << "start solving requests\n";
    char req_str[1024] = {0};
    while (true) {

        recv(recv_msg,req_str,0);
        //  Wait for next request from client
        //std::cout << "Recv " << req_str << std::endl;

        //  Do some 'work'
        std::string resp_str = execute_request(std::string(req_str));

        send(send_msg,resp_str.c_str());
        //std::cout << "Send "<< resp_str << std::endl;

    }
}
