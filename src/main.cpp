#include <iostream>
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
       Client client(server_address);
       client.run();
   }


   return 0;
}

void show_usage()
{
    std::cout << "Usage:\n\t-h\t\t\t help\n\t-s\t\t\t run as server\n\t-c [SERVER_ADDRESS]\t run as client\n" << std::endl;
}
