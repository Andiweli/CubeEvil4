#include "archive.h"
#include "texture.h"
#include <iomanip>
#include <iostream>
int main(int argc, char **argv) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: archive_tool DECODED.dat\n";
      return 2;
    }
    auto arc = re4::RoomArchive::Load(argv[1]);
    for (std::size_t i = 0; i < arc.entries().size(); i++) {
      const auto &e = arc.entries()[i];
      std::string tag(reinterpret_cast<const char *>(e.tag.data()), 3);
      if (arc.find(tag, static_cast<int>(e.occurrence)) != &e)
        throw std::runtime_error("Lookup mismatch");
      std::cout << i << '\t';
      for (auto c : e.tag)
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << unsigned(c);
      unsigned images = 0;
      if (tag == "TPL") {
        auto raw = arc.copy(i);
        if (raw.size() < 12)
          throw std::runtime_error("Short TPL block");
        images = (std::uint32_t(raw[4]) << 24) | (std::uint32_t(raw[5]) << 16) |
                 (std::uint32_t(raw[6]) << 8) | raw[7];
        if (!images || images > 4096)
          throw std::runtime_error("Invalid TPL image count");
        for (unsigned image = 0; image < images; image++)
          (void)re4::DecodeTpl(raw, image);
      }
      std::cout << std::dec << '\t' << e.occurrence << '\t' << e.offset << '\t'
                << e.extent << '\t' << images << '\n';
    }
    if (arc.find("???", 0) || arc.find("TPL", -1))
      throw std::runtime_error("Missing lookup mismatch");
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
