#include "archive.h"
#include "smd.h"
#include <iomanip>
#include <iostream>
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      return 2;
    auto arc = re4::RoomArchive::Load(argv[1]);
    std::cout << std::setprecision(9);
    for (int occurrence = 0; const auto *e = arc.find("SMD", occurrence);
         ++occurrence) {
      auto bytes = arc.copy(static_cast<std::size_t>(e - arc.entries().data()));
      if (bytes.size() < 16)
        throw std::runtime_error("Short SMD");
      if (bytes[0] != 0x40) {
        std::cout << "U\t" << occurrence << '\t' << unsigned(bytes[0]) << '\n';
        continue;
      }
      re4::Smd s(bytes);
      (void)s.describe();
      std::cout << "H\t" << occurrence << '\t' << unsigned(s.version) << '\t'
                << unsigned(s.flags) << '\t' << s.workOffset << '\t'
                << s.logicalWorkCount << '\t' << s.groups.size() << '\t'
                << s.placements.size() << '\n';
      for (std::size_t i = 0; i < s.groups.size(); ++i)
        std::cout << "G\t" << occurrence << '\t' << i << '\t' << s.groups[i]
                  << '\n';
      for (std::size_t i = 0; i < s.placements.size(); ++i) {
        const auto &w = s.placements[i];
        std::cout << "W\t" << occurrence << '\t' << i;
        for (auto *v : {&w.position, &w.rotation, &w.scale})
          for (auto f : *v)
            std::cout << '\t' << f;
        std::cout << '\t' << unsigned(w.bin) << '\t' << unsigned(w.tpl) << '\t'
                  << unsigned(w.motion) << '\t' << unsigned(w.work) << '\t'
                  << w.flags << '\n';
      }
      unsigned type = 0;
      for (auto *t : {&s.binOffsets, &s.tplOffsets, &s.motionOffsets}) {
        for (std::size_t i = 0; i < t->size(); ++i)
          std::cout << "T\t" << occurrence << '\t' << type << '\t' << i << '\t'
                    << (*t)[i] << '\n';
        ++type;
      }
      for (std::size_t i = 0; i < s.models.size(); ++i) {
        const auto &m = s.models[i];
        std::cout << "B\t" << occurrence << '\t' << i << '\t' << m.offset
                  << '\t' << m.vertices << '\t' << m.displayLists << '\t'
                  << unsigned(m.shift) << '\t' << m.version << '\t'
                  << unsigned(m.parts) << '\t' << m.streamBytes;
        for (auto *v : {&m.minimum, &m.maximum})
          for (auto f : *v)
            std::cout << '\t' << f;
        std::cout << '\n';
      }
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
