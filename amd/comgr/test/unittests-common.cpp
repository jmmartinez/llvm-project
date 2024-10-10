#include "unittests-common.h"

#include <fstream>

std::string ReadFile(const char *Path) {
  std::ifstream file(Path, std::ios_base::binary);
  auto begin = std::istreambuf_iterator<char>(file);
  auto end = std::istreambuf_iterator<char>();
  std::string buf(begin, end);
  return buf;
}
