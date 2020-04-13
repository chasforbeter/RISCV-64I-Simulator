#include "elfio/elfio.hpp"
#include "Simulation.h"
using namespace ELFIO;

// http://elfio.sourceforge.net/elfio.pdf
void printElfInfo(elfio &reader)
{
  std::cout << "ELF file class : ";
  if (reader.get_class() == ELFCLASS32)
    std::cout << "ELF32" << std::endl;
  else
    std::cout << "ELF64" << std::endl;

  std::cout << "ELF file encoding : ";
  if (reader.get_encoding() == ELFDATA2LSB)
    std::cout << "Little endian" << std::endl;
  else
    std::cout << "Big endian" << std::endl;
  Elf_Half sec_num = reader.sections.size();
  std::cout << "Number of sections: " << sec_num << std::endl;
  for (int i = 0; i < sec_num; ++i)
  {
    const section *psec = reader.sections[i];
    std::cout << " [" << i << "] "
              << psec->get_name() //节名
              << "\t"
              << psec->get_size() //节大小
              << std::endl;
    // Access section's data
    // const char *p = reader.sections[i]->get_data();
  }
  // Print ELF file segments info
  Elf_Half seg_num = reader.segments.size();
  std::cout << "Number of segments: " << seg_num << std::endl;
  for (int i = 0; i < seg_num; ++i)
  {
    const segment *pseg = reader.segments[i];
    std::cout << " [" << i << "] 0x" << std::hex
              << pseg->get_flags()
              << "\t0x"
              << pseg->get_virtual_address() //本段在内存的开始地址
              << "\t0x"
              << pseg->get_file_size() //elf文件这部分大小，已知
              << "\t0x"
              << pseg->get_memory_size() //这个段的大小，尾部0
              << std::endl;
    // Access segments's data
    // const char *p = reader.segments[i]->get_data(); 连续分布
  }
}

void loader(elfio &reader, Memory &memory)
{
  Elf_Half n = reader.segments.size();
  for (int i = 0; i < n; i++)
  {
    const segment *pseg = reader.segments[i];
    uint32_t data_size = pseg->get_file_size(), seg_size = pseg->get_memory_size(), seg_start = pseg->get_virtual_address();
    for (uint32_t p = seg_start; p < seg_start + seg_size; p += 4096)
    {
      if (!memory.isValidAddr(p))
        memory.alloc(p);
    }
    for (uint32_t p = seg_start; p < seg_start + data_size; p++)
      memory.store_byte(p, pseg->get_data()[p - seg_start]);
  }
}