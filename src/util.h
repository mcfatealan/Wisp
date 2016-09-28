#ifndef _UTIL_H
#define _UTIL_H

#include <string>
#include <sstream>
#include <vector>

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

#endif
