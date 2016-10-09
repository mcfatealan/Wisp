#ifndef _UTIL_H
#define _UTIL_H

#include <string>
#include <sstream>
#include <vector>
#include <chrono>
#include <stdio.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

using namespace std;

inline void split(const string &s, char delim, vector<string> &elems) {
    stringstream ss;
    ss.str(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
}


inline vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}

inline std::chrono::milliseconds gettimestamp()
{
    using namespace std::chrono;
    return duration_cast< milliseconds >(
            system_clock::now().time_since_epoch()
    );
}

//return seconds
inline float gettimeinterval(std::chrono::milliseconds t1,std::chrono::milliseconds t2)
{
    typedef std::chrono::duration<float> fsec;
    fsec fs = t2 - t1;
    return fs.count();
}

inline std::string gethostname () {
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            if(strcmp(addressBuffer,"127.0.0.1")==0)
                continue;
            return std::string(addressBuffer);
        } else if (ifa->ifa_addr->sa_family == AF_INET6) { // check it is IP6
            // is a valid IP6 Address
            tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            return std::string(addressBuffer);
        }
    }
    return std::string();
}
#endif
