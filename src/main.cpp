#include <iostream>
#include <string>
#include "../ext/getoptpp/getopt_pp.h"
#include "client.h"
#include "server.h"

void show_usage();
int main(int argc, char* argv[]) {
   using namespace GetOpt;
   GetOpt_pp ops(argc, argv);

   if (argc<2 || ops >> OptionPresent('h',"help"))
       show_usage();

   else if (ops >> OptionPresent('s',"server"))
   {
       Server server;
       server.run();
   }
   else if (ops >> OptionPresent('c',"client"))
   {
       std::string server_address;
       ops >> Option('c', "client", server_address);
       if(server_address.compare("")==0)
       {
            std::cout << "lacks server address" << std::endl;
            return(-1);
       }
       Client client(server_address);
       client.run();
   }
   else if (ops >> OptionPresent('p',"client_perf"))
   {
       std::string server_address, key_size, payload_size;
       int ks,ps;
       ops >> Option('p', "client_perf", server_address)
           >> Option("key_size", key_size)
           >> Option("payload_size", payload_size);
       try{
           ks = std::stoi( key_size );
           ps = std::stoi( payload_size );
       }
       catch (std::invalid_argument)
       {
           std::cout << "key_size or payload_size invalid" << std::endl;
           return(-1);
       }
       catch (std::out_of_range)
       {
           std::cout << "key_size or payload_size invalid" << std::endl;
           return(-1);
       }

       if(server_address.compare("")==0)
       {
           std::cout << "lacks server address" << std::endl;
           return(-1);
       }
       Client client(server_address);
       client.run_perftest(ks,ps);
   }


   return 0;
}

void show_usage()
{
    std::cout << "Usage:\n\t-h\t\t\t help\n\t-s\t\t\t run as server\n\t-c [SERVER_ADDRESS]\t run as client\n\t-p [SERVER_ADDRESS] --key_size 3 --payload_size 100\t run as client_perf\n" << std::endl;
}
