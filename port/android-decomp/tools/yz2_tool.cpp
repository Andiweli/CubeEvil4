#include "ports/yz2.h"
#include <fstream>
#include <iostream>
int main(int argc, char **argv) {
  try {
    if (argc != 3) {
      std::cerr << "Usage: yz2_tool INPUT.das OUTPUT.dat\n";
      return 2;
    }
    auto result = re4::LoadYz2(argv[1]);
    std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char *>(result.bytes.data()),
              result.bytes.size());
    out.close();
    if (!out)
      throw std::runtime_error("Cannot write output");
    std::cout << result.bytes.size() << " bytes; consumed " << result.consumed
              << "; packed field " << result.packedSize << "\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
