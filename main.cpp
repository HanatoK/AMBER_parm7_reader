#include "ReadAmberParm.h"

#include <cstring>

int main(int argc, char* argv[]) {
  if (argc < 2) return 1;
  AmberParm7Reader::Ambertoppar data = AmberParm7Reader::readparm(argv[1]);
  dump_summary(data, std::cout);
  dump_atoms(data, std::cout);
  dump_cmap(data, std::cout);
  return 0;
} 
