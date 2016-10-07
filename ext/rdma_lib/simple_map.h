#ifndef SIMPLE_MAP
#define SIMPLE_MAP

#include <cstdint>
#include <iostream>

//#include "port/atomic.h"

#define S_BUCKET_NUM 64

/* A simple constrained thread-safe map */

template <typename T>
class SimpleMap {

  int size;
  
  struct simple_bucket {  
    int key;
    T val;
    simple_bucket *next;
  };

  simple_bucket slots[S_BUCKET_NUM]; 
  bool slot_empty[S_BUCKET_NUM];

  inline uint64_t GetHash(int key) {
    return MurmurHash64A(key, 0xdeadbeef) % S_BUCKET_NUM;
  }

  static inline uint64_t MurmurHash64A (uint64_t key, unsigned int seed )  {
      
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;
    uint64_t h = seed ^ (8 * m);
    const uint64_t * data = &key;
    const uint64_t * end = data + 1;

    while(data != end)  {
      uint64_t k = *data++;
      k *= m;
      k ^= k >> r;
      k *= m;
      h ^= k;
      h *= m;
    }

    const unsigned char * data2 = (const unsigned char*)data;

    switch(8 & 7)   {
    case 7: h ^= uint64_t(data2[6]) << 48;
    case 6: h ^= uint64_t(data2[5]) << 40;
    case 5: h ^= uint64_t(data2[4]) << 32;
    case 4: h ^= uint64_t(data2[3]) << 24;
    case 3: h ^= uint64_t(data2[2]) << 16;
    case 2: h ^= uint64_t(data2[1]) << 8;
    case 1: h ^= uint64_t(data2[0]);
      h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
  }



 public:
  T   dummy_val;
  T   *record_sets;
  
  //  SimpleMap(char *ptr,int 
 SimpleMap(T d,int max_capacity = -1)
   :size(0),
    dummy_val(d)
    {
      for(int i = 0;i < S_BUCKET_NUM;++i)
	slot_empty[i] = true;
      if(max_capacity > 0) {
	record_sets = new T[max_capacity];
      } else
	record_sets = NULL;
    }
  
  void Insert(int key,T val) {
    
    int hash = GetHash(key);
    //    std::cout<<hash<<std::endl;
    if(slot_empty[hash]) {
      slots[hash].key = key;
      slots[hash].val = val;
      slots[hash].next = NULL;
      slot_empty[hash] = false;
      size += 1;
      return;
    }
    
    simple_bucket *b = &(slots[hash]);
    //    std::cout<<"second case\n";
    while(b->next != NULL)
      b = (b->next);
    
    simple_bucket *nb = new simple_bucket;
    nb->key = key;
    nb->val = val;
    nb->next = NULL;
    //    std::cout<<"second case alloc\n";    
    /* a fence */
    asm volatile("mfence":::"memory");
    b->next = nb;
    size += 1;
  }
  
  T  &operator[] (const int key) {
    int hash = GetHash(key);
  
    if(slot_empty[hash])
      return dummy_val;
  
    simple_bucket &b = slots[hash];
    if(b.key == key)
      return b.val;
    simple_bucket *p = b.next;
    while(p != NULL) {
      if(p->key == key)
	return p->val;
      p = p->next;
    }

    return dummy_val;

  }

  
};

#endif
