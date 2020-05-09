#include "cache.h"
#include "def.h"
#include <cmath>
#include <iostream>
using namespace std;

void Cache::HandleRequest(uint32_t addr, int bytes, int read,
                          char *content, int &hit, int &time)
{
  hit = 0;
  time = 0;
  // Bypass?
  stats_.access_counter++;
  uint32_t block_offset = addr & (~(uint32_t(-1) << config_.off_bits));                                   //块内偏移
  uint32_t set_index = (addr >> config_.off_bits) & (~(uint32_t(-1) << config_.set_bits));                //中间的set编号
  uint32_t tag = (addr >> (config_.off_bits + config_.set_bits)) & (~(uint32_t(-1) << config_.tag_bits)); //最高位tag
  uint32_t block_addr = addr - block_offset;                                                              //块起始位置                                                               //
  uint32_t ret = -1;

  //目前的时间++
  for (int i = 0; i < config_.set_num; i++)
  {
    for (int j = 0; j < config_.associativity; j++)
    {
      if (set[i].Block[j].valid == 1)
      {
        set[i].Block[j].visited_t++;
      }
    }
  }
  // Miss?
  if (!ReplaceDecision(block_offset, set_index, tag, ret))
  {
    time += latency_.bus_latency + latency_.hit_latency; //发起一次寻址失败
    stats_.access_time += latency_.bus_latency + latency_.hit_latency;
    stats_.miss_num++;
    if (BypassDecision())
    { //不需要访问cache
      //。。。
    }
    else
    { //miss需要访问
      ReplaceAlgorithm(block_offset, set_index, tag, ret);
    }
  }
  else //命中
  {
    hit = 1;
    //选中
    time += latency_.bus_latency + latency_.hit_latency;
    stats_.access_time += latency_.bus_latency + latency_.hit_latency;
    set[set_index].Block[ret].visited_t = 0;

    if (read == 0) // 写命中
    {
      for (int i = 0; i < bytes; i++)
        set[set_index].Block[ret].blk[block_offset + i] = content[i];
      if (config_.write_through) //直写，写下一层
      {
        int lower_hit = 0, lower_time = 0;
        lower_->HandleRequest(addr, bytes, 0, content, lower_hit, lower_time);
        //写时间
        time += lower_time; //这里忽略硬件并行写的操作，不用返回所以不加总线延迟
        set[set_index].Block[ret].dirty = false;
      }
      else //写回，只完成本层
        set[set_index].Block[ret].dirty = true;
    }
    else // 读命中
    {
      for (int i = 0; i < bytes; i++)
        content[i] = set[set_index].Block[ret].blk[block_offset + i];
    }
    return;
  }

  //以下为不命中，需要向下读块。
  if (hit == 0)
  {
    // Fetch from lower layer
    hit = 0;
    int lower_hit, lower_time;
    if (read == 0)
    { //写讨论
      if (config_.write_allocate)
      { //写分配，需要读这个block再写
        char *tmp = new char[2 * config_.block_size];
        //读
        lower_->HandleRequest(block_addr, config_.block_size, 1, tmp,
                              lower_hit, lower_time);
        time += lower_time;
        //stats_.access_time += latency_.bus_latency; 不用返回所以不用总线延迟

        //写回，需要写下去被替换的block
        if (!config_.write_through && set[set_index].Block[ret].valid && set[set_index].Block[ret].dirty)
        {
          uint32_t kick_addr = (set[set_index].Block[ret].tag << (32 - config_.tag_bits)) + (set_index << config_.off_bits);
          lower_->HandleRequest(kick_addr, config_.block_size, 0, set[set_index].Block[ret].blk, lower_hit, lower_time);
          //stats_.access_time += latency_.bus_latency;
          time += lower_time; //latency_.bus_latency 也不需要返回
        }
        //else写穿，始终正确，不管dirty即可

        for (int i = 0; i < config_.block_size; i++)
          set[set_index].Block[ret].blk[i] = tmp[i];

        set[set_index].Block[ret].tag = tag;
        set[set_index].Block[ret].valid = true;
        set[set_index].Block[ret].visited_t = 0;
        set[set_index].Block[ret].dirty = true;

        if (config_.write_through) // 写穿需要再次向下写，这次不会miss了
        {
          int lower_hit = 0, lower_time = 0;
          lower_->HandleRequest(addr, bytes, 0, content, lower_hit, lower_time);
          time += lower_time; //这里忽略硬件并行写的操作 latency_.bus_latency +
          //stats_.access_time += latency_.bus_latency;
          set[set_index].Block[ret].dirty = false;
        }
        delete[] tmp;
        for (int i = 0; i < bytes; i++)
          set[set_index].Block[ret].blk[block_offset + i] = content[i];
      }
      else //写不分配：不踢出，只从所在位置开始写
      {
        lower_->HandleRequest(addr, bytes, 0, content, lower_hit, lower_time);
        time += lower_time; //latency_.bus_latency +
        //stats_.access_time += latency_.bus_latency;
      }
    }
    else
    { //读讨论
      char *tmp = new char[2 * config_.block_size];
      lower_->HandleRequest(block_addr, config_.block_size, 1, tmp,
                            lower_hit, lower_time);
      //stats_.access_time += latency_.bus_latency;
      time += lower_time; //latency_.bus_latency +

      if (!config_.write_through && set[set_index].Block[ret].valid && set[set_index].Block[ret].dirty) //踢出前需要写回。直写不会出现
      {
        uint32_t kick_addr = (set[set_index].Block[ret].tag << (32 - config_.tag_bits)) + (set_index << config_.off_bits);
        lower_->HandleRequest(kick_addr, config_.block_size, 0, set[set_index].Block[ret].blk, lower_hit, lower_time);
        //stats_.access_time += latency_.bus_latency;
        time += lower_time; //latency_.bus_latency +
      }

      for (int i = 0; i < config_.block_size; i++)
        set[set_index].Block[ret].blk[i] = tmp[i];

      set[set_index].Block[ret].tag = tag;
      set[set_index].Block[ret].valid = true;
      set[set_index].Block[ret].visited_t = 0;
      set[set_index].Block[ret].dirty = false;
      delete[] tmp;
      for (int i = 0; i < bytes; i++)
        content[i] = set[set_index].Block[ret].blk[block_offset + i];
    }
  }
  // 预取条件：目前设为hit
  if (PrefetchDecision())
  {
    PrefetchAlgorithm();
  }
}

int Cache::BypassDecision()
{
  return FALSE;
}

void Cache::PartitionAlgorithm()
{
}

int Cache::ReplaceDecision(uint32_t block_offset, uint32_t set_index, uint32_t tag, uint32_t &block_num)
{
  //找不到对应的就返回False
  for (int i = 0; i < config_.associativity; i++)
  {
    if (set[set_index].Block[i].valid == 1 && set[set_index].Block[i].tag == tag)
    {
      block_num = i;
      //cout << "found " << i << endl;
      return TRUE;
    }
  }
  return FALSE;
}

void Cache::ReplaceAlgorithm(uint32_t block_offset, uint32_t set_index, uint32_t tag, uint32_t &ret)
{
  //选取准备替换掉的blk号，后续具体操作视策略而定
  int max_t = set[set_index].Block[0].visited_t;
  ret = 0;
  for (int i = 0; i < config_.associativity; i++)
  {
    //无效，直接使用，准备后面驱逐
    if (set[set_index].Block[i].valid == 0)
    {
      ret = i;
      return;
    }
    //lru
    else if (set[set_index].Block[i].visited_t > max_t)
    {
      ret = i;
      max_t = set[set_index].Block[i].visited_t;
    }
  }
}

int Cache::PrefetchDecision()
{
  return FALSE;
}

void Cache::PrefetchAlgorithm()
{
}

void Cache::SetConfig(CacheConfig cc)
{
  config_ = cc;
  set = new SET[config_.set_num];
  for (int i = 0; i < config_.set_num; i++)
  {
    set[i].Block = new BLK[config_.associativity];
    for (int j = 0; j < config_.associativity; j++)
      set[i].Block[j].blk = new char[config_.block_size];
  }

  config_.off_bits = ceil(log2(float(cc.block_size)));
  config_.set_bits = ceil(log2(float(cc.set_num)));
  config_.tag_bits = 32 - config_.off_bits - config_.set_bits;
  // cout << config_.off_bits << ' ' << config_.set_bits << ' ' << config_.tag_bits << endl;
}
void Cache::GetConfig(CacheConfig &cc)
{
  cc = config_;
}