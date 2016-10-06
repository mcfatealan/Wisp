#ifndef _UTIL_H
#define _UTIL_H

#include <string>
#include <sstream>
#include <vector>
#include <chrono>

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


#endif
