#include "Simulation.h"
#include "cache.h"
#include "elfio/elfio.hpp"
using namespace std;

extern void loader(ELFIO::elfio &reader, Memory &memory);
extern void printElfInfo(ELFIO::elfio &reader);

Memory memory;
Cache l1;
Cache l2;
Cache llc;

int branch_strat = AT;

//输出名字
string inst_name[] = {
    "",
    "AUIPC",
    "JAL",
    "JALR",
    "BEQ",
    "BNE",
    "BLT",
    "BGE",
    "BLTU",
    "BGEU",
    "LB",
    "LH",
    "LW",
    "LBU",
    "LHU",
    "SB",
    "SH",
    "SW",
    "ADDI",
    "SLTI",
    "SLTIU",
    "XORI",
    "ORI",
    "ANDI",
    "SLLI",
    "SRLI",
    "SRAI",
    "ADD",
    "SUB",
    "SLL",
    "SLT",
    "SLTU",
    "XOR",
    "SRL",
    "SRA",
    "OR",
    "AND",
    "ECALL",
    "LWU",
    "LD",
    "SD",
    "ADDIW",
    "SLLIW",
    "SRLIW",
    "SRAIW",
    "ADDW",
    "SUBW",
    "SLLW",
    "SRLW",
    "SRAW",
    "MUL",
    "MULH",
    "DIV",
    "REM",
    "LUI",
    "UNKNOWN"};
char *reg_name[REGNUM] = {
    "ZERO", "RA", "SP", "GP", "TP", "T0", "T1", "T2", "S0",
    "S1", "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
    "S2", "S3", "S4", "S5", "S6", "S7", "S8", "S9", "S10",
    "S11", "T3", "T4", "T5", "T6"};
char *strat_name[] = {
    "AT", "NT", "BTFNT"};

char *file = "";
bool verbose = 0;
bool singleStep = 0;
bool dumpMem = 0;
SIM sim;
ELFIO::elfio reader;

//性能计数相关模块
int inst_cnt = 0;
int pipeline_cycle_cnt = 0;
int cache_cycle_cnt = 0;
int correct_predict = 0;
int mispredict = 0;

int cnt_j = 0;
int control_cnt = 0;
int loaduse_cnt = 0;

int cache_cnt = 0;
int cachehit_cnt = 0;

int main(int argc, char **argv)
{
  for (int i = 1; i < argc; i++)
  {
    if (argv[i][0] != '-')
      file = argv[i];
    else
    {
      switch (argv[i][1])
      {
      case 's':
        singleStep = 1;
        verbose = 1;
        break;
      case 'm':
        dumpMem = 1;
        break;
      case 'v':
        verbose = 1;
        break;
      case 'b': //接受参数
        if (i + 1 < argc)
        {
          i++;
          if (strcmp(argv[i], "BTFNT") == 0)
          {
            branch_strat = BTFNT;
          }
          else if (strcmp(argv[i], "NT") == 0)
          {
            branch_strat = NT;
          }
          else
          {
            branch_strat = AT; //default
          }
        }
        break;
      }
    }
  }
  if (!reader.load(file))
  {
    printf("Can't find or process ELF file %s\n", file);
    return 0;
  }
  //初始化存储参数

  l1.SetLower(&l2);
  l2.SetLower(&llc);
  llc.SetLower(&memory);

  StorageStats s;
  memset(&s, 0, sizeof(s));
  memory.SetStats(s);
  l1.SetStats(s);
  l2.SetStats(s);
  llc.SetStats(s);

  StorageLatency ml;
  ml.bus_latency = 0;
  ml.hit_latency = 100;
  memory.SetLatency(ml);

  StorageLatency l1l;
  l1l.bus_latency = 0;
  l1l.hit_latency = 1;
  l1.SetLatency(l1l);

  StorageLatency l2l;
  l2l.bus_latency = 0;
  l2l.hit_latency = 8;
  l2.SetLatency(l2l);

  StorageLatency llcl;
  llcl.bus_latency = 0;
  llcl.hit_latency = 20;
  llc.SetLatency(llcl);

  CacheConfig config1;
  config1.size = 32 * 1024;
  config1.associativity = 8;
  config1.block_size = 64;
  config1.set_num = 32 * 1024 / (64 * 8);
  config1.write_through = 0;
  config1.write_allocate = 1; //?unknown
  l1.SetConfig(config1);

  CacheConfig config2;
  config2.size = 256 * 1024;
  config2.associativity = 8;
  config2.block_size = 64;
  config2.set_num = 256 * 1024 / (64 * 8);
  config2.write_through = 0;
  config2.write_allocate = 1; //?unknown
  l2.SetConfig(config2);

  CacheConfig config3;
  config3.size = 8 * 1024 * 1024;
  config3.associativity = 8;
  config3.block_size = 64;
  config3.set_num = 8 * 1024 * 1024 / (64 * 8);
  config3.write_through = 0;
  config3.write_allocate = 1; //?unknown
  llc.SetConfig(config3);

  //初始化内存
  loader(reader, memory);
  for (uint32_t addr = stack_start; addr > stack_start - stack_size; addr -= 4096)
  {
    if (!memory.isValidAddr(addr))
    {
      memory.alloc(addr);
    }
  }
  if (verbose)
  {
    printElfInfo(reader);
    memory.dump_memory();
  }
  //初始化PC和SP
  sim.Freg.cur.predPC = reader.get_entry();
  sim.Dreg.clear();
  sim.Ereg.clear();
  sim.Mreg.clear();
  sim.Wreg.clear();
  sim.reg[REG_SP] = stack_start;
  //模拟循环
  sim.simulate();

  return 0;
}

void SIM::simulate()
{

  // 指令循环
  while (true)
  {
    pipeline_cycle_cnt++;
    WB();
    MEM();
    EX();
    ID();
    IF();

    uint32_t opcode = Dreg.cur.inst & 0x7F;
    if (opcode == OP_JAL || opcode == OP_JALR)
    {
      Freg.stall = true;  //已经转发到pred，将会重新取地址。也可以是bubble
      Dreg.bubble = true; //冲刷送进来的F
      if (verbose)
      {
        printf("Jxx in ID,  %.8x at address %llx, bubble +1 \n", Dreg.cur.inst, Dreg.cur.pc);
      }
      cnt_j++;
      control_cnt++;
    }
    int32_t ins = Ereg.cur.inst;

    if ((ins == BEQ || ins == BNE || ins == BLT || ins == BGE || ins == BLTU || ins == BGEU) && e_Bwrong)
    {
      Freg.stall = true; //下一周期M转发，会被成功预测，也可以是bubble
      Dreg.bubble = true;
      Ereg.bubble = true;
      mispredict++;
      control_cnt++;
      if (verbose)
      {
        printf("e_Bwrong=1, branch mispredicted in Ereg, %.8x at address %llx. bubble +2\n", inst_name[Ereg.cur.inst].c_str(), Ereg.cur.pc);
      }
    }

    int destReg = Ereg.cur.destreg;
    uint32_t inst = Ereg.cur.inst;
    if (destReg != REG_ZERO && (inst == LB || inst == LH || inst == LW || inst == LBU || inst == LHU || inst == LWU || inst == LD))
    {
      if (d_reg1 == destReg || d_reg2 == destReg)
      {
        Freg.stall = true; //1
        Dreg.stall = true;
        Ereg.bubble = true;
        loaduse_cnt++;
        if (verbose)
        {
          printf("Load-use detected, D instruction %.8x at address %llx. bubble +1 \n", Dreg.cur.inst, Dreg.cur.pc);
        }
      }
    }

    Freg.tick();
    Dreg.tick();
    Ereg.tick();
    Mreg.tick();
    Wreg.tick();

    if (reg[REG_SP] < stack_start - stack_size)
      printf("stack overflow\n");
    reg[0] = 0;
    // debug用每次打印,否则输出太长
    // if (verbose)
    //   print_regs_info();
    if (singleStep)
    {
      printf("'g' to proceed, 'm' to dump memory and 'r' for reg info\n");
      char c;
      while ((c = getchar()) != 'g')
      {
        if (c == 'm')
          memory.dump_memory();
        if (c == 'r')
          print_regs_info();
      }
    }
  }
}

void SIM::print_regs_info()
{
  printf("Register info\n");
  printf("PC %llx\n", Freg.cur.predPC);
  for (int i = 0; i < REGNUM; ++i)
    printf("%s: %llx (%lld)\n", reg_name[i], reg[i], reg[i]);
}

void SIM::IF()
{
  //来自e的转发
  uint64_t pc = Freg.cur.predPC;

  if (e_isJ)
  {
    pc = e_calcPC;
    if (verbose)
      printf("IF: using current e_calcPC %x\n", pc);
  }

  if (Mreg.cur.M_Bwrong)
  { //上一轮E确定的结果，此时变成了M. --注意M如果是bubble必须清空这个区域，否则没有机会修改
    pc = Mreg.cur.M_calcPC;
    if (verbose)
      printf("IF: using current Mreg.cur.M_calcPC %x\n", pc);
  }

  if (!memory.isValidAddr(pc))
  {
    printf("IF: invalid read at %x\n", pc);
    printf("IF: e_isJ: %d, e_Jname: %s, cnt_j: %d\n", e_isJ, inst_name[e_Jname].c_str(), cnt_j);
    printf("IF: jalr_pc: %x\n", jalr_pc);
    printf("IF: M_Bwrong: %d\n", Mreg.cur.M_Bwrong);
  }
  // uint32_t inst = memory.load(pc, 4);
  // cache_cycle_cnt+=100;
  int cache_hit = 0, cache_time = 0;
  uint32_t inst;
  l1.HandleRequest(pc, 4, 1, (char *)&inst, cache_hit, cache_time);
  cachehit_cnt += cache_hit;
  cache_cnt++;
  cache_cycle_cnt += cache_time;

  if (verbose)
  {
    printf("IF: Fetch instruction %.8x at address %llx\n", inst, pc);
  }
  if (pc % 4 != 0)
  {
    printf("PC %x not multiple of 4\n", pc);
    exit(0);
  }

  //预测分支
  uint64_t predictedPC, pcnottaken;
  uint32_t opcode = inst & 0x7F;
  bool take_branch = false;
  if (opcode == OP_BRANCH)
  {
    int32_t inst_i = inst;
    int32_t offset = (((inst_i >> 7) & 0x1E) | ((inst_i >> 20) & 0x7E0) | ((inst_i << 4) & 0x800) | ((inst_i >> 19) & 0x1000)) << 19 >> 19;

    take_branch = false;
    switch (branch_strat)
    {
    case NT:
      take_branch = false;
      break;
    case AT:
      take_branch = true;
      break;
    case BTFNT:
    {
      if (offset >= 0)
      {
        take_branch = false;
      }
      else
      {
        take_branch = true;
      }
      break;
    }
    }
    if (take_branch)
    {
      predictedPC = pc + offset;
      pcnottaken = pc + 4;
    }
    else
    {
      predictedPC = pc + 4;
      pcnottaken = pc + offset;
    }
  }
  else
  {
    predictedPC = pc + 4;
    pcnottaken = pc + 4; //防止有错
  }
  if (verbose)
    printf("IF: predicted PC %.8x for next clock\n", predictedPC);
  //本指令向前走
  Freg.upd.predPC = predictedPC;
  Dreg.upd.inst = inst;
  Dreg.upd.pc = pc;
  Dreg.upd.PCnottaken = pcnottaken;
  Dreg.upd.take_branch = take_branch;
}

void SIM::ID()
{
  Ereg.upd.inst = Dreg.cur.inst;
  if (Dreg.cur.inst == 0)
  {
    if (verbose)
      printf("ID: Bubble\n");
    d_reg1 = -1;
    d_reg2 = -1;
    memset(&Ereg.upd, 0, sizeof(Ereg.upd));
    return;
  }
  uint32_t inst = Dreg.cur.inst;
  uint32_t opcode = inst & 0x7F;
  uint32_t funct3 = (inst >> 12) & 0x7;
  uint32_t funct7 = (inst >> 25) & 0x7F;
  int rd = (inst >> 7) & 0x1F;
  int rs1 = (inst >> 15) & 0x1F;
  int rs2 = (inst >> 20) & 0x1F;
  //有符号立即数！！
  int32_t inst_i = int32_t(inst);
  int32_t imm_i = inst_i >> 20;
  int32_t imm_s = (((inst_i >> 7) & 0x1F) | ((inst_i >> 20) & 0xFE0)) << 20 >> 20; //sign!!
  int32_t imm_sb = (((inst_i >> 7) & 0x1E) | ((inst_i >> 20) & 0x7E0) | ((inst_i << 4) & 0x800) | ((inst_i >> 19) & 0x1000)) << 19 >> 19;
  int32_t imm_u = (inst_i) >> 12;
  int32_t imm_uj = (((inst_i >> 21) & 0x3FF) | ((inst_i >> 10) & 0x400) | ((inst_i >> 1) & 0x7F800) | ((inst_i >> 12) & 0x80000)) << 12 >> 11;

  string inststr = "";
  string dest_str, val1_str, val2_str, offsetstr;
  string type_str = "";
  int32_t insttype = UNKNOWN;
  int64_t valA = 0, valB = 0, offset = 0;
  int dest = -1, reg1 = -1, reg2 = -1;
  switch (opcode)
  {
  case OP_R:
    valA = reg[rs1];
    valB = reg[rs2];
    reg1 = rs1;
    reg2 = rs2;
    dest = rd;
    switch (funct3)
    {
    case 0:
      if (funct7 == 0x00)
        insttype = ADD;
      else if (funct7 == 0x01)
        insttype = MUL;
      else if (funct7 == 0x20)
        insttype = SUB;
      else
      {
        printf("Unknown funct7 %x of funct3 %x\n", funct7, funct3);
        exit(0);
      }
      break;
    case 1:
      if (funct7 == 0x00)
        insttype = SLL;
      else if (funct7 == 0x01)
        insttype = MULH;
      else
      {
        printf("Unknown funct7 %x of funct3 %x\n", funct7, funct3);
        exit(0);
      }
      break;
    case 2:
      if (funct7 == 0x00)
        insttype = SLT;
      else
      {
        printf("Unknown funct7 %x of funct3 %x\n", funct7, funct3);
        exit(0);
      }
      break;
    case 4:
      if (funct7 == 0x00)
        insttype = XOR;
      else if (funct7 == 0x01)
        insttype = DIV;
      else
      {
        printf("Unknown funct7 %x of funct3 %x\n", funct7, funct3);
        exit(0);
      }
      break;
    case 5:
      if (funct7 == 0x00)
        insttype = SRL;
      else if (funct7 == 0x20)
        insttype = SRA;
      else
      {
        printf("Unknown funct7 %x of funct3 %x\n", funct7, funct3);
        exit(0);
      }
      break;
    case 6:
      if (funct7 == 0x00)
        insttype = OR;
      else if (funct7 == 0x01)
        insttype = REM;
      else
      {
        printf("Unknown funct7 %x of funct3 %x\n", funct7, funct3);
        exit(0);
      }
      break;
    case 7:
      if (funct7 == 0x00)
        insttype = AND;
      else
      {
        printf("Unknown funct7 %x of funct3 %x\n", funct7, funct3);
        exit(0);
      }
      break;
    default:
      printf("Unknown Funct3 field %x\n", funct3);
      exit(0);
    }
    val1_str = reg_name[rs1];
    val2_str = reg_name[rs2];
    dest_str = reg_name[rd];
    type_str = inst_name[insttype];
    inststr = type_str + " " + dest_str + " " + val1_str + " " + val2_str;
    break;
  case OP_IMM:
    valA = reg[rs1];
    reg1 = rs1;
    valB = imm_i;
    dest = rd;
    switch (funct3)
    {
    case 0:
      insttype = ADDI;
      break;
    case 1:
      insttype = SLLI;
      valB = valB & 0x3F;
      break;
    case 2:
      insttype = SLTI;
      break;
    case 3:
      insttype = SLTIU;
      break;
    case 4:
      insttype = XORI;
      break;
    case 5:
      if (((inst >> 26) & 0x3F) == 0x0)
      {
        insttype = SRLI;
        valB = valB & 0x3F;
      }
      else if (((inst >> 26) & 0x3F) == 0x10)
      {
        insttype = SRAI;
        valB = valB & 0x3F;
      }
      else
      {
        printf("Unknown funct7 %x of OP_IMM\n", (inst >> 26) & 0x3F);
        exit(0);
      }
      break;
    case 6:
      insttype = ORI;
      break;
    case 7:
      insttype = ANDI;
      break;
    default:
      printf("Unknown funct3 field %x\n", funct3);
      exit(0);
    }
    val1_str = reg_name[rs1];
    val2_str = to_string(valB);
    dest_str = reg_name[dest];
    type_str = inst_name[insttype];
    inststr = type_str + " " + dest_str + " " + val1_str + " " + val2_str;
    break;
  case OP_JAL:
    valA = imm_uj;
    valB = 0;
    offset = imm_uj;
    dest = rd;
    insttype = JAL;
    val1_str = to_string(imm_uj);
    dest_str = reg_name[dest];
    type_str = inst_name[insttype];
    inststr = type_str + " " + dest_str + " " + val1_str;
    break;
  case OP_JALR:
    valA = reg[rs1];
    reg1 = rs1;
    valB = imm_i;
    dest = rd;
    insttype = JALR;
    val1_str = reg_name[rs1];
    val2_str = to_string(valB);
    dest_str = reg_name[dest];
    type_str = inst_name[insttype];
    inststr = type_str + " " + dest_str + " " + val1_str + " " + val2_str;
    break;
  case OP_BRANCH:
    valA = reg[rs1];
    valB = reg[rs2];
    reg1 = rs1;
    reg2 = rs2;
    offset = imm_sb;
    switch (funct3)
    {
    case 0:
      insttype = BEQ;
      break;
    case 1:
      insttype = BNE;
      break;
    case 4:
      insttype = BLT;
      break;
    case 5:
      insttype = BGE;
      break;
    case 6:
      insttype = BLTU;
      break;
    case 7:
      insttype = BGEU;
      break;
    default:
      printf("Unknown funct3 %x of OP_BRANCH\n", funct3);
      exit(0);
    }
    val1_str = reg_name[rs1];
    val2_str = reg_name[rs2];
    offsetstr = to_string(offset);
    type_str = inst_name[insttype];
    inststr = type_str + " " + val1_str + " " + val2_str + " " + offsetstr;
    break;
  case OP_LOAD:
    valA = reg[rs1];
    reg1 = rs1;
    valB = imm_i;
    offset = imm_i;
    dest = rd;
    switch (funct3)
    {
    case 0:
      insttype = LB;
      break;
    case 1:
      insttype = LH;
      break;
    case 2:
      insttype = LW;
      break;
    case 3:
      insttype = LD;
      break;
    case 4:
      insttype = LBU;
      break;
    case 5:
      insttype = LHU;
      break;
    case 6:
      insttype = LWU;
    default:
      printf("Unknown funct3 %x of OP_LOAD\n", funct3);
      exit(0);
    }
    val1_str = reg_name[rs1];
    val2_str = to_string(valB);
    dest_str = reg_name[rd];
    type_str = inst_name[insttype];
    inststr = type_str + " " + dest_str + " " + val2_str + "(" + val1_str + ")";
    break;
  case OP_STORE:
    valA = reg[rs1];
    valB = reg[rs2];
    reg1 = rs1;
    reg2 = rs2;
    offset = imm_s;
    switch (funct3)
    {
    case 0:
      insttype = SB;
      break;
    case 1:
      insttype = SH;
      break;
    case 2:
      insttype = SW;
      break;
    case 3:
      insttype = SD;
      break;
    default:
      printf("Unknown funct3 %x of OP_STORE\n", funct3);
      exit(0);
    }
    val1_str = reg_name[rs1];
    val2_str = reg_name[rs2];
    offsetstr = to_string(offset);
    type_str = inst_name[insttype];
    inststr = type_str + " " + val2_str + " " + offsetstr + "(" + val1_str + ")";
    break;
  case OP_LUI:
    valA = imm_u;
    valB = 0;
    offset = imm_u;
    dest = rd;
    insttype = LUI;
    val1_str = to_string(imm_u);
    dest_str = reg_name[dest];
    type_str = inst_name[insttype];
    inststr = type_str + " " + dest_str + " " + val1_str;
    break;
  case OP_AUIPC:
    valA = imm_u;
    valB = 0;
    offset = imm_u;
    dest = rd;
    insttype = AUIPC;
    val1_str = to_string(imm_u);
    dest_str = reg_name[dest];
    type_str = inst_name[insttype];
    inststr = type_str + " " + dest_str + " " + val1_str;
    break;
  case OP_SYSTEM:
    if (funct3 == 0x0 && funct7 == 0x000)
    {
      valA = reg[REG_A0];
      valB = reg[REG_A7];
      reg1 = REG_A0;
      reg2 = REG_A7;
      dest = -1;
      insttype = ECALL;
    }
    else
    {
      printf("Unknown OP_SYSTEM inst with funct3 %x and funct7 %x\n",
             funct3, funct7);
      exit(0);
    }
    type_str = inst_name[insttype];
    inststr = type_str;
    break;
  case OP_IMM32:
    valA = reg[rs1];
    reg1 = rs1;
    valB = imm_i;
    dest = rd;
    switch (funct3)
    {
    case 0:
      insttype = ADDIW;
      break;
    case 1:
      insttype = SLLIW;
      break;
    case 5:
      if (((inst >> 25) & 0x7F) == 0x0)
        insttype = SRLIW;
      else if (((inst >> 25) & 0x7F) == 0x20)
        insttype = SRAIW;
      break;
    default:
      printf("Unknown funct3 %x of OP_ADDIW\n", funct3);
      exit(0);
    }
    val1_str = reg_name[rs1];
    val2_str = to_string(valB);
    dest_str = reg_name[rd];
    type_str = inst_name[insttype];
    inststr = type_str + " " + dest_str + " " + val1_str + " " + val2_str;
    break;
  case OP_32:
  {
    valA = reg[rs1];
    valB = reg[rs2];
    reg1 = rs1;
    reg2 = rs2;
    dest = rd;
    uint32_t f7 = (inst >> 25) & 0x7F;
    switch (funct3)
    {
    case 0:
      if (f7 == 0x0)
        insttype = ADDW;
      else if (f7 == 0x20)
        insttype = SUBW;
      break;
    case 1:
      if (f7 == 0x0)
        insttype = SLLW;
    case 5:
      if (f7 == 0x0)
        insttype = SRLW;
      else if (f7 == 0x20)
        insttype = SRAW;
      break;
    default:
      printf("Unknown funct3 %x\n", funct3);
      exit(0);
    }
    type_str = inst_name[insttype];
  }
  break;
  default:
    printf("Unknown opcode %x\n", opcode);
    exit(0);
  }
  if (verbose)
    printf("ID: %.8x interpreted as \'%s\'\n", inst, inststr.c_str());

  //转发：两种情况
  bool forw = false;

  //先检查低优先级转发
  if (m_reg_write && Mreg.cur.destReg != REG_ZERO) //ret is JALR ZERO 0 A0
  {
    int destReg = Mreg.cur.destReg;
    if (reg1 == destReg)
    {
      valA = m_valM;
      if (verbose)
        printf("MEM Forward %s to ID valA\n", reg_name[destReg]);
      forw = true;
    }
    if (reg2 == destReg)
    {
      valB = m_valM;
      if (verbose)
        printf("MEM Forward %s to ID valB\n", reg_name[destReg]);
      forw = true;
    }
  }
  //  最高优先级，应该最后一个被检查
  if (e_reg_write && Ereg.cur.destreg != REG_ZERO)
  {
    int destReg = Ereg.cur.destreg;

    if (reg1 == destReg) //reg1之后给 Ereg.upd.rs1。注意这里不可以用upd，因为可能会被bubble掉
    {
      valA = e_valE; //马上给Ereg.upd.valA
      if (verbose)
        printf("EX Forward %s to ID valA\n", reg_name[destReg]);
      forw = true;
    }
    if (reg2 == destReg)
    {
      valB = e_valE;
      if (verbose)
        printf("EX Forward %s to ID valB\n", reg_name[destReg]);
      forw = true;
    }
  }

  d_reg1 = reg1;
  d_reg2 = reg2;
  Ereg.upd.rs1 = reg1;
  Ereg.upd.rs2 = reg2;
  Ereg.upd.pc = Dreg.cur.pc;
  Ereg.upd.inst = insttype;

  Ereg.upd.destreg = dest;
  Ereg.upd.valA = valA;
  Ereg.upd.valB = valB;
  Ereg.upd.offset = offset;
  Ereg.upd.PCnottaken = Dreg.cur.PCnottaken;
  Ereg.upd.take_branch = Dreg.cur.take_branch;
}

void SIM::EX()
{
  Mreg.upd.inst = Ereg.cur.inst;
  e_Bwrong = false;
  e_isJ = false;
  e_reg_write = false;
  e_valE = 0;
  e_calcPC = -1;
  if (Ereg.cur.inst == 0)
  {
    if (verbose)
    {
      printf("EX: Bubble\n");
    }
    e_valE = 0;
    e_reg_write = false;
    memset(&Mreg.upd, 0, sizeof(Mreg.upd));
    return;
  }

  if (verbose)
  {
    printf("EX: %s\n", inst_name[Ereg.cur.inst].c_str());
  }

  int32_t inst = Ereg.cur.inst;
  int64_t valA = Ereg.cur.valA;
  int64_t valB = Ereg.cur.valB;
  int64_t offset = Ereg.cur.offset;
  bool take_branch = Ereg.cur.take_branch;

  uint64_t calculated_PC = 0;
  uint64_t origin_pc = Ereg.cur.pc;
  bool reg_write = false;
  int destReg = Ereg.cur.destreg;
  int64_t valE = 0; //计算结果 evale
  bool mem_write = false;
  bool mem_read = false;
  bool mem_signed = false;
  uint32_t memLen = 0;
  bool res_branch = false;

  switch (inst)
  {
  case LUI:
    reg_write = true;
    valE = offset << 12;
    break;
  case AUIPC:
    reg_write = true;
    valE = origin_pc + (offset << 12);
    break;
  case JAL:
    reg_write = true;
    valE = origin_pc + 4;
    calculated_PC = origin_pc + valA;
    break;
  case JALR:
    reg_write = true;
    valE = origin_pc + 4; //只有这时使用calcPC
    calculated_PC = (valA + valB) & (~(uint64_t)1);
    break;
  case BEQ:
    if (valA == valB)
    {
      res_branch = true;
      valE = origin_pc + offset;
    }
    break;
  case BNE:
    if (valA != valB)
    {
      res_branch = true;
      valE = origin_pc + offset;
    }
    break;
  case BLT:
    if (valA < valB)
    {
      res_branch = true;
      valE = origin_pc + offset;
    }
    break;
  case BGE:
    if (valA >= valB)
    {
      res_branch = true;
      valE = origin_pc + offset;
    }
    break;
  case BLTU:
    if ((uint64_t)valA < (uint64_t)valB)
    {
      res_branch = true;
      valE = origin_pc + offset;
    }
    break;
  case BGEU:
    if ((uint64_t)valA >= (uint64_t)valB)
    {
      res_branch = true;
      valE = origin_pc + offset;
    }
    break;
  case LB:
    mem_read = true;
    reg_write = true;
    memLen = 1;
    valE = valA + offset;
    mem_signed = true;
    break;
  case LH:
    mem_read = true;
    reg_write = true;
    memLen = 2;
    valE = valA + offset;
    mem_signed = true;
    break;
  case LW:
    mem_read = true;
    reg_write = true;
    memLen = 4;
    valE = valA + offset;
    mem_signed = true;
    break;
  case LD:
    mem_read = true;
    reg_write = true;
    memLen = 8;
    valE = valA + offset;
    mem_signed = true;
    break;
  case LBU:
    mem_read = true;
    reg_write = true;
    memLen = 1;
    valE = valA + offset;
    break;
  case LHU:
    mem_read = true;
    reg_write = true;
    memLen = 2;
    valE = valA + offset;
    break;
  case LWU:
    mem_read = true;
    reg_write = true;
    memLen = 4;
    valE = valA + offset;
    break;
  case SB:
    mem_write = true;
    memLen = 1;
    valE = valA + offset;
    valB = valB & 0xFF;
    break;
  case SH:
    mem_write = true;
    memLen = 2;
    valE = valA + offset;
    valB = valB & 0xFFFF;
    break;
  case SW:
    mem_write = true;
    memLen = 4;
    valE = valA + offset;
    valB = valB & 0xFFFFFFFF;
    break;
  case SD:
    mem_write = true;
    memLen = 8;
    valE = valA + offset;
    break;
  case ADDI:
  case ADD:
    reg_write = true;
    valE = valA + valB;
    break;
  case ADDIW:
  case ADDW:
    reg_write = true;
    valE = (int64_t)((int32_t)valA + (int32_t)valB);
    break;
  case SUB:
    reg_write = true;
    valE = valA - valB;
    break;
  case SUBW:
    reg_write = true;
    valE = (int64_t)((int32_t)valA - (int32_t)valB);
    break;
  case MUL:
    reg_write = true;
    valE = (int64_t)(valA * valB);
    break;
  case DIV:
    reg_write = true;
    valE = valA / valB;
    break;
  case SLTI:
  case SLT:
    reg_write = true;
    valE = valA < valB ? 1 : 0;
    break;
  case SLTIU:
  case SLTU:
    reg_write = true;
    valE = (uint64_t)valA < (uint64_t)valB ? 1 : 0; //unsigned!!
    break;
  case XORI:
  case XOR:
    reg_write = true;
    valE = valA ^ valB;
    break;
  case ORI:
  case OR:
    reg_write = true;
    valE = valA | valB;
    break;
  case ANDI:
  case AND:
    reg_write = true;
    valE = valA & valB;
    break;
  case SLLI:
  case SLL:
    reg_write = true;
    valE = valA << valB;
    break;
  case SLLIW:
  case SLLW:
    reg_write = true;
    valE = int64_t(int32_t(valA << valB));
    break;
    break;
  case SRLI:
  case SRL:
    reg_write = true;
    valE = (uint64_t)valA >> (uint64_t)valB;
    break;
  case SRLIW:
  case SRLW:
    reg_write = true;
    valE = uint64_t(uint32_t((uint32_t)valA >> (uint32_t)valB));
    break;
  case SRAI:
  case SRA:
    reg_write = true;
    valE = valA >> valB;
    break;
  case SRAW:
  case SRAIW:
    reg_write = true;
    valE = int64_t(int32_t((int32_t)valA >> (int32_t)valB));
    break;
  case ECALL:
    syscall(valA, valB);
    break;
  }

  if (inst == BEQ || inst == BNE || inst == BLT || inst == BGE || inst == BLTU || inst == BGEU)
  {
    //此时calculated_PC无意义
    reg_write = false;
    if (take_branch == res_branch)
      correct_predict++;
    else
    {
      e_Bwrong = true; //先用于mispredicted加bubble；下一阶段用于predict
      e_calcPC = Ereg.cur.PCnottaken;
      if (verbose)
        printf("Found BXX mispredicted in Execute, 2 bubbles.\n");
    }
  }
  if (inst == JAL || inst == JALR)
  {
    e_isJ = true;
    e_calcPC = calculated_PC;
    e_Jname = inst;
    jalr_pc = origin_pc;
    if (verbose)
      printf("Found JAL / JALR in Execute, 1 bubble.\n");
  }
  e_reg_write = reg_write;
  Mreg.upd.pc = Ereg.cur.pc; //调试用
  Mreg.upd.inst = inst;
  Mreg.upd.valA = valA;
  Mreg.upd.valB = valB;
  Mreg.upd.reg_write = reg_write;
  Mreg.upd.destReg = destReg;
  Mreg.upd.M_Bwrong = e_Bwrong;
  Mreg.upd.M_calcPC = e_calcPC;
  e_valE = valE; //B开头的=0
  Mreg.upd.valE = valE;
  Mreg.upd.mem_write = mem_write;
  Mreg.upd.mem_read = mem_read;
  Mreg.upd.mem_signed = mem_signed;
  Mreg.upd.memLen = memLen;
  Mreg.upd.branch = res_branch;
}

void SIM::MEM()
{
  Wreg.upd.inst = Mreg.cur.inst;
  m_reg_write = false;
  m_valM = 0;
  if (Mreg.cur.inst == 0)
  {
    if (verbose)
    {
      printf("MEM: Bubble\n");
    }
    m_reg_write = false;
    m_valM = 0;
    memset(&Wreg.upd, 0, sizeof(Wreg.upd));
    return;
  }
  inst_cnt++;
  uint64_t origin_pc = Mreg.cur.pc;
  int32_t inst = Mreg.cur.inst;
  bool reg_write = Mreg.cur.reg_write;
  int destReg = Mreg.cur.destReg;
  int64_t valA = Mreg.cur.valA;
  int64_t valB = Mreg.cur.valB;
  int64_t valE = Mreg.cur.valE;
  int64_t valM = valE; //合并，任何valM都要写入寄存器
  bool mem_write = Mreg.cur.mem_write;
  bool mem_read = Mreg.cur.mem_read;
  bool mem_signed = Mreg.cur.mem_signed;
  uint32_t memLen = Mreg.cur.memLen;

  int cache_hit = 0, cache_time = 0;

  if (mem_write)
  {
    //memory.store(valE, valB, memLen);
    //cache_cycle_cnt+=100;
    l1.HandleRequest(valE, memLen, 0, (char *)(&valB), cache_hit, cache_time);
    cache_cycle_cnt += cache_time;
    cachehit_cnt += cache_hit;
    cache_cnt++;
  }

  if (mem_read) //只有这一种情况修改valM，且不会再使用valE
  {
    if (!memory.isValidAddr(valE))
    {
      printf("MEM: invalid read at %x\n", valE);
      exit(-1);
    }
    // if (mem_signed)
    //   valM = (int64_t)memory.load(valE, memLen);
    // else
    //   valM = (uint64_t)memory.load(valE, memLen);
    // cache_cycle_cnt+=100;

    char tmp[16]; //switch中不可声明，统一char
    l1.HandleRequest(valE, memLen, 1, tmp, cache_hit, cache_time);
    cache_cycle_cnt += cache_time;
    cachehit_cnt += cache_hit;
    cache_cnt++;
    //小端法使得可以直接扩展读取

    for (int i = memLen; i < 16; i++)
    {
      tmp[i] = 0;
    }
    switch (memLen)
    {
    case 8:
    {
      if (mem_signed)
        valM = (int64_t) * ((uint64_t *)tmp);
      else
        valM = (uint64_t) * ((uint64_t *)tmp);
      break;
    }
    case 4:
    {
      if (mem_signed)
        valM = (int64_t) * ((uint32_t *)tmp);
      else
        valM = (uint64_t) * ((uint32_t *)tmp);
      break;
    }
    case 2:
    {
      if (mem_signed)
        valM = (int64_t) * ((uint16_t *)tmp);
      else
        valM = (uint64_t) * ((uint16_t *)tmp);
      break;
    }
    case 1:
    {
      if (mem_signed)
        valM = (int64_t) * ((uint8_t *)tmp);
      else
        valM = (uint64_t) * ((uint8_t *)tmp);
      break;
    }
    }
  }

  if (verbose)
  {
    printf("MEM: %s\n", inst_name[inst].c_str());
  }

  Wreg.upd.pc = origin_pc;
  Wreg.upd.inst = inst;
  Wreg.upd.valA = valA;
  Wreg.upd.valB = valB;
  Wreg.upd.destReg = destReg;
  m_reg_write = reg_write;
  Wreg.upd.reg_write = reg_write;
  m_valM = valM;
  Wreg.upd.valM = valM;
}

void SIM::WB()
{
  if (Wreg.cur.inst == 0)
  {
    if (verbose)
    {
      printf("WB: Bubble\n");
    }
    return;
  }

  if (verbose)
  {
    printf("WB: %s\n", inst_name[Wreg.cur.inst].c_str());
  }

  if (Wreg.cur.reg_write && Wreg.cur.destReg != REG_ZERO)
    reg[Wreg.cur.destReg] = Wreg.cur.valM;
}
void SIM::syscall(int64_t valA, int64_t valB)
{
  int64_t syscallnum = valB; //a7
  int64_t arg1 = valA;       //a0
  int cache_hit = 0, cache_time = 0;
  char c;
  switch (syscallnum)
  {
  case 0: //char* str
    //c = memory.load_byte(arg1);
    l1.HandleRequest(arg1, 1, 1, &c, cache_hit, cache_time);
    cachehit_cnt += cache_hit;
    cache_cnt++;
    cache_cycle_cnt += cache_time;
    while (c != '\0')
    {
      printf("%c", c);
      arg1++;
      //c = memory.load_byte(arg1);
      int cache_hit = 0, cache_time = 0;
      l1.HandleRequest(arg1, 1, 1, &c, cache_hit, cache_time);
      cachehit_cnt += cache_hit;
      cache_cnt++;
      cache_cycle_cnt += cache_time;
    }
    break;
  case 1: //char
    printf("%c", arg1);
    break;
  case 2: //int
    printf("%d", arg1);
    break;
  case 93:
  {
    int total_cycle_cnt = cache_cycle_cnt + pipeline_cycle_cnt;
    printf("Exit normally\n\n");
    printf("Total Cycles: %d\n", total_cycle_cnt);
    printf("Pipeline Cycles: %d\n", pipeline_cycle_cnt);
    printf("Instructions: %d\n", inst_cnt);
    printf("CPI with cache: %f\n", (double)total_cycle_cnt / inst_cnt);
    printf("CPI w/o cache: %f\n", (double)(pipeline_cycle_cnt + 100 * cache_cnt) / inst_cnt);
    printf("Pipeline CPI: %f\n\n", (double)pipeline_cycle_cnt / inst_cnt);

    printf("Load-use hazards: %d\n", loaduse_cnt);
    printf("Jumps: %d\n", cnt_j);
    printf("Mispredicts: %d\n", mispredict);
    printf("Control hazards (JXX + BXX): %d\n", control_cnt);
    printf("%s prediction accuracy: %lf \n\n",
           strat_name[branch_strat], (double)correct_predict / (correct_predict + mispredict)); //注意不加double按整数舍入

    printf("L1 Cache accesses: %d\n", cache_cnt);
    printf("L1 Cache miss: %d\n", cache_cnt - cachehit_cnt);
    printf("L1 Miss rate: %f\n", (double)(cache_cnt - cachehit_cnt) / cache_cnt);

    StorageStats l2s;
    l2.GetStats(l2s);
    printf("L2 Cache accesses: %d\n", l2s.access_counter);
    printf("L2 Cache miss: %d\n", l2s.miss_num);
    printf("L2 Miss rate: %f\n", (double)l2s.miss_num / l2s.access_counter);

    StorageStats l3s;
    llc.GetStats(l3s);
    printf("L3 Cache accesses: %d\n", l3s.access_counter);
    printf("L3 Cache miss: %d\n", l3s.miss_num);
    printf("L3 Miss rate: %f\n", (double)l3s.miss_num / l3s.access_counter);

    if (dumpMem)
      memory.dump_memory();

    exit(0);
  }
  default:
    printf("Unknown syscall %d\n", syscallnum);
    exit(0);
  }
}
