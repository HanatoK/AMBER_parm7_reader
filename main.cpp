#include "ReadAmberParm.h"

#include <cstring>
#include <string>
#include <fstream>

int main(int argc, char* argv[]) {
  if (argc < 2) return 1;
  AmberParm7Reader::Ambertoppar data = AmberParm7Reader::readparm(argv[1]);
  if (data.HasData) {
    const std::string output_file = std::string(argv[1]) + ".out";
    std::ofstream ofs_out(output_file.c_str());
    dump_summary(data, ofs_out);
    dump_atoms(data, ofs_out);
    dump_cmap(data, ofs_out);
  }
  return 0;
} 
