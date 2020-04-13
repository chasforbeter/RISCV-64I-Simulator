编译在当前目录下执行：
g++ -I./ *.cpp -o ./sim.out -std=c++11

运行方法：
./sim.out [riscv-elffile] [-m] [-v] [-s] [-b strategy]
 
riscv-elffile为待执⾏的riscv ELF⽂件。-m为运⾏结束时打出所有内存空间到memory.txt。-v为
verbose，会详细输出模拟器流⽔线每⼀阶段步骤。-s为单步调试。-b添加分⽀预测策略，模拟器⽀持
AT(Always Taken), NT(Not Taken), BTFNT(Backward Taken, Forward Not Taken)。

举例：
$ ./sim.out test-sample/qs.riscv -s -b BTFNT
WB: Bubble
MEM: Bubble
EX: Bubble
ID: Bubble
IF: Fetch instruction 00003197 at address 100b0
IF: predicted PC 000100b4 for next clock
'g' to proceed, 'm' to dump memory and 'r' for reg info
g
WB: Bubble
MEM: Bubble
EX: Bubble
ID: 00003197 interpreted as 'AUIPC GP 3'
IF: Fetch instruction 1a018193 at address 100b4
IF: predicted PC 000100b8 for next clock
'g' to proceed, 'm' to dump memory and 'r' for reg info
g
WB: Bubble
MEM: Bubble
EX: AUIPC
ID: 1a018193 interpreted as 'ADDI GP GP 416'
EX Forward GP to ID valA
IF: Fetch instruction 81818513 at address 100b8
IF: predicted PC 000100bc for next clock
'g' to proceed, 'm' to dump memory and 'r' for reg info
r
Register info
PC 100bc
ZERO: 0 (0)
RA: 0 (0)
SP: 7fffc000 (2147467264)
GP: 0 (0)
TP: 0 (0)
T0: 0 (0)
T1: 0 (0)
T2: 0 (0)
S0: 0 (0)
S1: 0 (0)
A0: 0 (0)
A1: 0 (0)
A2: 0 (0)
A3: 0 (0)
A4: 0 (0)
A5: 0 (0)
A6: 0 (0)
A7: 0 (0)
S2: 0 (0)
S3: 0 (0)
S4: 0 (0)
S5: 0 (0)
S6: 0 (0)
S7: 0 (0)
S8: 0 (0)
S9: 0 (0)
S10: 0 (0)
S11: 0 (0)
T3: 0 (0)
T4: 0 (0)
T5: 0 (0)
T6: 0 (0)


