#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <cmath>
// #include <sys/io.h>
#include <ctime>
#include <stdint.h>
#include "storage.h"
#include "elfio/elfio.hpp"
using namespace std;

#define REGNUM 32

//跳转策略：
#define AT 0
#define NT 1
#define BTFNT 2

//reg index
#define REG_ZERO 0
#define REG_RA 1
#define REG_SP 2
#define REG_GP 3
#define REG_TP 4
#define REG_T0 5
#define REG_T1 6
#define REG_T2 7
#define REG_S0 8
#define REG_S1 9
#define REG_A0 10
#define REG_A1 11
#define REG_A2 12
#define REG_A3 13
#define REG_A4 14
#define REG_A5 15
#define REG_A6 16
#define REG_A7 17
#define REG_S2 18
#define REG_S3 19
#define REG_S4 20
#define REG_S5 21
#define REG_S6 22
#define REG_S7 23
#define REG_S8 24
#define REG_S9 25
#define REG_S10 26
#define REG_S11 27
#define REG_T3 28
#define REG_T4 29
#define REG_T5 30
#define REG_T6 31

//inst idx
#define AUIPC 1
#define JAL 2
#define JALR 3
#define BEQ 4
#define BNE 5
#define BLT 6
#define BGE 7
#define BLTU 8
#define BGEU 9
#define LB 10
#define LH 11
#define LW 12
#define LBU 13
#define LHU 14
#define SB 15
#define SH 16
#define SW 17
#define ADDI 18
#define SLTI 19
#define SLTIU 20
#define XORI 21
#define ORI 22
#define ANDI 23
#define SLLI 24
#define SRLI 25
#define SRAI 26
#define ADD 27
#define SUB 28
#define SLL 29
#define SLT 30
#define SLTU 31
#define XOR 32
#define SRL 33
#define SRA 34
#define OR 35
#define AND 36
#define ECALL 37
#define LWU 38
#define LD 39
#define SD 40
#define ADDIW 41
#define SLLIW 42
#define SRLIW 43
#define SRAIW 44
#define ADDW 45
#define SUBW 46
#define SLLW 47
#define SRLW 48
#define SRAW 49
#define MUL 50
#define MULH 51
#define DIV 52
#define REM 53
#define LUI 54
#define UNKNOWN -1

//opcode
#define OP_R 0x33
#define OP_IMM 0x13
#define OP_LUI 0x37
#define OP_BRANCH 0x63
#define OP_STORE 0x23
#define OP_LOAD 0x03
#define OP_SYSTEM 0x73
#define OP_AUIPC 0x17
#define OP_JAL 0x6F
#define OP_JALR 0x67
#define OP_IMM32 0x1B
#define OP_32 0x3B

#define stack_start 0x7fffc000
#define stack_size 0x400000

#define PDE(addr) ((addr >> 22) & 0x3FF)
#define PTE(addr) ((addr >> 12) & 0x3FF)
#define OFF(addr) (addr & 0xFFF)

class Memory : public Storage
{
public:
  uint8_t **memory[1024] = {0};
  Memory() {}
  ~Memory() {}
  // Main access process
  void HandleRequest(uint32_t addr, int bytes, int read,
                     char *content, int &hit, int &time)
  {

    hit = 1;
    time = latency_.hit_latency + latency_.bus_latency;
    stats_.access_time += time;
    if (read == 1)
    {
      for (int i = 0; i < bytes; i++)
        content[i] = char(load_byte(addr + i));
    }
    else
    {
      for (int i = 0; i < bytes; i++)
        store_byte(addr + i, uint8_t(content[i]));
    }
  }

  void store_byte(uint32_t addr, uint8_t val)
  {
    uint32_t i = PDE(addr);
    uint32_t j = PTE(addr);
    uint32_t k = OFF(addr);
    if (!isValidAddr(addr))
    {
      printf("invalid write at %x\n", addr);
      exit(-1);
    }
    memory[i][j][k] = val;
  }
  bool store(uint32_t addr, uint64_t val, int bytes)
  {
    for (int i = 0; i < bytes; i++)
      store_byte(addr + i, (val >> 8 * i) & 0xFF);
    return true;
  }

  uint8_t load_byte(uint32_t addr)
  {
    uint32_t i = PDE(addr);
    uint32_t j = PTE(addr);
    uint32_t k = OFF(addr);
    if (!isValidAddr(addr))
    {
      printf("invalid read at %x\n", addr);
      dump_memory();
      exit(-1);
    }
    return memory[i][j][k];
  }

  uint64_t load(uint32_t addr, int bytes)
  {
    uint64_t res = 0;
    for (int i = 0; i < bytes; i++)
      res += ((uint64_t)load_byte(addr + i)) << (i * 8);
    return res;
  }

  void alloc(uint32_t addr)
  {
    if (isValidAddr(addr))
    {
      printf("multiple allocation for %x\n", addr);
      return;
    }
    uint32_t i = PDE(addr);
    uint32_t j = PTE(addr);
    if (memory[i] == NULL)
    {
      memory[i] = new uint8_t *[1024];
      memset(memory[i], 0, 1024 * sizeof(uint8_t *));
    }
    if (memory[i][j] == NULL)
    {
      memory[i][j] = new uint8_t[4096];
      memset(memory[i][j], 0, 4096);
    }
    return;
  }

  void dump_memory()
  {
    FILE *fp;
    fp = fopen("memory.txt", "w+");
    fprintf(fp, "Memory Dump\n");
    for (int i = 0; i < 1024; ++i)
      if (memory[i])
        for (int j = 0; j < 1024; ++j)
          if (memory[i][j])
            for (int k = 0; k < 1024; ++k)
              fprintf(fp, "0x%x:%x\n", (i << 22) + (j << 12) + k,
                      memory[i][j][k]);
    fclose(fp);
    printf("Memory dump in memory.txt\n");
  }

  bool isValidAddr(uint32_t addr)
  {
    if (memory[PDE(addr)] && memory[PDE(addr)][PTE(addr)])
      return true;
    return false;
  }
  DISALLOW_COPY_AND_ASSIGN(Memory);
};
class PIPE_REG
{
public:
  bool bubble = 0;
  bool stall = 0;

  void clear()
  {
    bubble = 0;
    stall = 0;
  }
};
class F_REG : public PIPE_REG
{
public:
  struct region
  {
    uint64_t predPC;
    uint32_t inst;

  } cur, upd;
  void tick()
  {
    if (bubble)
      cur.inst = 0;
    else if (!stall)
      cur = upd;
    PIPE_REG::clear();
    memset(&upd, 0, sizeof(upd));
  };
  void clear()
  {
    PIPE_REG::clear();
    memset(&cur, 0, sizeof(cur));
    memset(&upd, 0, sizeof(upd));
  }
};
class D_REG : public PIPE_REG
{
public:
  struct region
  {
    uint64_t pc;
    uint32_t inst;
    uint64_t PCnottaken;
    bool take_branch;
  } cur, upd;
  void tick()
  {
    if (bubble)
      cur.inst = 0;
    else if (!stall)
      cur = upd;
    PIPE_REG::clear();
    memset(&upd, 0, sizeof(upd));
  };
  void clear()
  {
    PIPE_REG::clear();
    memset(&cur, 0, sizeof(cur));
    memset(&upd, 0, sizeof(upd));
  }
};
class E_REG : public PIPE_REG
{
public:
  struct region
  {
    uint64_t pc;
    int rs1, rs2, destreg;
    uint32_t inst;
    uint64_t PCnottaken;
    int64_t valA, valB;
    int64_t offset;
    bool take_branch;
  } cur, upd;
  void tick()
  {
    if (bubble)
      cur.inst = 0;
    else if (!stall)
      cur = upd;
    PIPE_REG::clear();
    memset(&upd, 0, sizeof(upd));
  };
  void clear()
  {
    PIPE_REG::clear();
    memset(&cur, 0, sizeof(cur));
    memset(&upd, 0, sizeof(upd));
  }
};
struct M_REG : public PIPE_REG
{
public:
  struct region
  {
    int64_t valA;
    int64_t valB;
    int64_t valE;
    uint64_t M_calcPC;
    uint64_t pc;
    uint32_t inst;
    uint32_t memLen;
    int destReg;
    bool reg_write;
    bool mem_write;
    bool mem_read;
    bool mem_signed;
    bool M_Bwrong;
    bool branch;
  } cur, upd;
  void clear()
  {
    PIPE_REG::clear();
    memset(&cur, 0, sizeof(cur));
    memset(&upd, 0, sizeof(upd));
  }
  void tick()
  {
    if (bubble)
      cur.inst = 0;
    else if (!stall)
      cur = upd;
    PIPE_REG::clear();
    memset(&upd, 0, sizeof(upd));
  };
};
class W_REG : public PIPE_REG
{
public:
  struct region
  {
    uint64_t pc;
    uint32_t inst;
    int64_t valA;
    int64_t valB;
    int64_t valM;
    int64_t valE;
    bool reg_write;
    int destReg;
  } cur, upd;
  void clear()
  {
    PIPE_REG::clear();
    memset(&cur, 0, sizeof(cur));
    memset(&upd, 0, sizeof(upd));
  }
  void tick()
  {
    if (bubble)
      cur.inst = 0;
    else if (!stall)
      cur = upd;
    PIPE_REG::clear();
    memset(&upd, 0, sizeof(upd));
  };
};

class SIM
{
public:
  uint64_t reg[REGNUM] = {0};

  F_REG Freg;
  D_REG Dreg;
  E_REG Ereg;
  M_REG Mreg;
  W_REG Wreg;
  SIM(){};
  ~SIM(){};
  void print_regs_info();
  void simulate();

  bool e_isJ = 0, e_Bwrong = 0, e_reg_write = 0, m_reg_write = 0;
  int64_t e_valE = 0, m_valM = 0;
  uint64_t e_calcPC = 0;
  int d_reg1 = -1, d_reg2 = -1;
  int e_Jname = 0;
  uint64_t jalr_pc = 0;
  uint32_t jalr_inst = 0;

  void IF();
  void ID();
  void EX();
  void MEM();
  void WB();
  void syscall(int64_t valA, int64_t valB);
};
