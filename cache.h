#ifndef CACHE_CACHE_H_
#define CACHE_CACHE_H_

#include <stdint.h>
#include "storage.h"
using namespace std;
typedef struct CacheConfig_
{
  int size;
  int associativity;
  int set_num;        // Number of cache sets
  int write_through;  // 0|1 for back|through
  int write_allocate; // 0|1 for no-alc|alc

  int block_size;
  int off_bits;
  int tag_bits;
  int set_bits;

} CacheConfig;

struct BLK
{
  char *blk;
  uint32_t tag;
  bool dirty, valid;
  int visited_t;
  BLK()
  {
    blk = NULL;
    tag = 0;
    dirty = false;
    valid = false;
    visited_t = 0;
  }
};

struct SET
{
  BLK *Block;
  SET()
  {
    Block = NULL;
  }
};

class Cache : public Storage
{
public:
  Cache() {}
  ~Cache() {}

  // Sets & Gets
  void SetConfig(CacheConfig cc);
  void GetConfig(CacheConfig &cc);
  void SetLower(Storage *ll) { lower_ = ll; }
  // Main access process
  void HandleRequest(uint32_t addr, int bytes, int read,
                     char *content, int &hit, int &time);

private:
  // Bypassing
  int BypassDecision();
  // Partitioning
  void PartitionAlgorithm();
  // Replacement
  int ReplaceDecision(uint32_t, uint32_t, uint32_t, uint32_t &);
  void ReplaceAlgorithm(uint32_t, uint32_t, uint32_t, uint32_t &);
  // Prefetching
  int PrefetchDecision();
  void PrefetchAlgorithm();
  SET *set;
  CacheConfig config_;
  Storage *lower_;
  DISALLOW_COPY_AND_ASSIGN(Cache);
};

#endif //CACHE_CACHE_H_
