// re4_format_compare: the Formatumwandler (port/whole/runtime/formats) against
// the checked big-endian readers of RE4-Android-Decomp.
//
//   re4_format_compare                    synthetic samples of every compared
//                                         format (no disc data; cloud test)
//   re4_format_compare --data DIR [--room r100 ...] [-v]
//                                         the disc files: room archives of the
//                                         rooms (St1/r100.das), etc/*.esl,
//                                         debug/roominfo.dat, the rooms' events
//                                         (*/r100*.evd), etc/core.das, em/*.drs, op/*.das
//   re4_format_compare --write-samples DIR
//                                         the synthetic samples as a disc folder
//                                         (the room archive unpacked as
//                                         st1/r100.arc; port/tools/
//                                         test_whole_format_compare.py packs it)
//
// For every file both sides report the fields they read (facts.h): the app
// side with its readers on the big-endian bytes (app_side.cpp), the game side
// after converting a copy to host order, through the original structures
// (game_side.cpp). Every key must have the same value on both sides. A part
// the app's reader rejects is listed as "nicht vergleichbar" (with the
// reader's message); in the synthetic test that is a failure as well.
//
// Exit code: 0 all equal, 1 differences (or an incomplete conversion), 2 usage.
#include "app_side.h"
#include "facts.h"
#include "samples.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace re4cmp;
using namespace re4samples;

namespace {

enum Kind { kArchive, kCodec, kEsl, kEvd, kRoomInfo };

struct Input {
    std::string label;
    Kind kind;
    Bytes bytes;
    std::string error;  // the file could not be prepared (missing, container)
    bool required;      // a missing room archive fails the comparison
};

struct Result {
    std::size_t equal = 0;
    std::vector<std::string> diffs;
    std::vector<std::string> skipped;
    std::vector<std::string> incomplete;
    std::map<std::string, std::string> entries;  // archive entry prefix -> position, size, status
    std::string appError;
    bool gameRejected = false;
};

void sinkList(void* ctx, const char* key, const char* value)
{
    static_cast<FactList*>(ctx)->emplace_back(key, value);
}

const char* kindName(Kind k)
{
    switch (k) {
    case kArchive:
        return "Archiv";
    case kCodec:
        return "Funkarchiv";
    case kEsl:
        return "Gegnerliste";
    case kEvd:
        return "Ereignis";
    case kRoomInfo:
        return "Raumsprungtabelle";
    }
    return "?";
}

bool startsWith(const std::string& s, const std::string& p)
{
    return s.compare(0, p.size(), p) == 0;
}

Result compare(const Input& in)
{
    Result r;
    FactList app, game;
    try {
        switch (in.kind) {
        case kArchive:
            appArchive(in.bytes, app);
            break;
        case kCodec:
            appArchive(Bytes(in.bytes.begin() + 0x400, in.bytes.end()), app);
            break;
        case kEsl:
            appEsl(in.bytes, app);
            break;
        case kEvd:
            appEvd(in.bytes, app);
            break;
        case kRoomInfo:
            appRoomInfo(in.bytes, app);
            break;
        }
    } catch (const std::exception& e) {
        r.appError = e.what();
        return r;
    }
    Re4Facts facts = { sinkList, &game };
    int ok = 0;
    switch (in.kind) {
    case kArchive:
        ok = re4cmp_game_archive(in.bytes.data(), unsigned(in.bytes.size()), in.label.c_str(), &facts);
        break;
    case kCodec:
        ok = re4cmp_game_codec(in.bytes.data(), unsigned(in.bytes.size()), in.label.c_str(), &facts);
        break;
    case kEsl:
        ok = re4cmp_game_esl(in.bytes.data(), unsigned(in.bytes.size()), in.label.c_str(), &facts);
        break;
    case kEvd:
        ok = re4cmp_game_evd(in.bytes.data(), unsigned(in.bytes.size()), in.label.c_str(), &facts);
        break;
    case kRoomInfo:
        ok = re4cmp_game_roominfo(in.bytes.data(), unsigned(in.bytes.size()), in.label.c_str(), &facts);
        break;
    }
    r.gameRejected = ok == 0;
    std::map<std::string, std::string> a, g;
    for (const auto& kv : app) {
        if (!a.emplace(kv).second) {
            r.diffs.push_back(kv.first + ": doppelt auf der App-Seite");
        }
    }
    for (const auto& kv : game) {
        if (kv.first.find("!incomplete") != std::string::npos) {
            r.incomplete.push_back(kv.first.substr(0, kv.first.find("!incomplete")));
            continue;
        }
        if (kv.first.find("!entry.") != std::string::npos) {
            std::size_t at = kv.first.find("!entry.");
            r.entries[kv.first.substr(0, at) + kv.first.substr(at + 7)] = kv.second;
            continue;
        }
        if (!g.emplace(kv).second) {
            r.diffs.push_back(kv.first + ": doppelt auf der Kern-Seite");
        }
    }
    // parts the app's reader rejected: left out on both sides
    std::vector<std::string> prefixes;
    for (const auto& kv : a) {
        if (kv.first.size() >= 4 && kv.first.compare(kv.first.size() - 4, 4, "skip") == 0) {
            std::string prefix = kv.first.substr(0, kv.first.size() - 4);
            prefixes.push_back(prefix);
            r.skipped.push_back((prefix.empty() ? std::string("(Datei)") : prefix) + " (App: " + kv.second + ")");
        }
    }
    auto skipped = [&](const std::string& key) {
        for (const auto& p : prefixes) {
            if (startsWith(key, p)) {
                return true;
            }
        }
        return false;
    };
    for (auto it = a.begin(); it != a.end();) {
        it = skipped(it->first) ? a.erase(it) : std::next(it);
    }
    for (auto it = g.begin(); it != g.end();) {
        it = skipped(it->first) ? g.erase(it) : std::next(it);
    }
    for (const auto& kv : a) {
        auto it = g.find(kv.first);
        if (it == g.end()) {
            r.diffs.push_back(kv.first + ": App " + kv.second + " · Kern (fehlt)");
        } else if (it->second != kv.second) {
            r.diffs.push_back(kv.first + ": App " + kv.second + " · Kern " + it->second);
        } else {
            r.equal++;
        }
    }
    for (const auto& kv : g) {
        if (!a.count(kv.first)) {
            r.diffs.push_back(kv.first + ": App (fehlt) · Kern " + kv.second);
        }
    }
    return r;
}

// ---- synthetic samples: samples.h ------------------------------------------------

const char* const kMapSample = "synthetisch: Kartenraum";

std::vector<Input> syntheticInputs()
{
    std::vector<Input> in;
    in.push_back({ "synthetisch: Raumarchiv",
                   kArchive,
                   makeArchive({ { "SMD", makeSmd() },
                                 { "SMX", makeSmx() },
                                 { "SAT", makeSatFile(0) },
                                 { "EAT", makeEat() },
                                 { "AEV", makeAev() },
                                 { "LIT", makeLit() },
                                 { "CAM", makeCam() },
                                 // no data of its own: same offset as the BIN after it
                                 { "FCV", Bytes() },
                                 { "BIN", makeBin(true, false, true) },
                                 { "BIN", makeBin(false, true, false) },
                                 { "FCV", makeCurve(30) } }),
                   "",
                   true });
    // the map screen's room data (SS/cmn/map_objNN.dat); its own archive: the
    // room archive above doubles as the codec sample, whose readers split an
    // even entry count in halves
    in.push_back({ kMapSample, kArchive, makeArchive({ { "MAP", makeMapRoom() } }), "", true });
    in.push_back({ "synthetisch: Gegnerliste", kEsl, makeEsl(), "", true });
    in.push_back({ "synthetisch: Ereignis", kEvd, makeEvd(), "", true });
    in.push_back({ "synthetisch: Raumsprungtabelle", kRoomInfo, makeRoomInfo(), "", true });
    return in;
}

// ---- disc files ------------------------------------------------------------------

bool readFile(const std::string& path, Bytes& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

bool isDir(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::vector<std::string> listDir(const std::string& dir)
{
    std::vector<std::string> names;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            if (std::strcmp(e->d_name, ".") != 0 && std::strcmp(e->d_name, "..") != 0) {
                names.push_back(e->d_name);
            }
        }
        closedir(d);
    }
    std::sort(names.begin(), names.end());
    return names;
}

// Case-insensitive path below `root` ("st1/r100.das"); empty when missing.
std::string findPath(const std::string& root, const std::string& rel)
{
    std::string path = root;
    std::size_t start = 0;
    while (start <= rel.size()) {
        std::size_t slash = rel.find('/', start);
        std::string part = rel.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        std::string found;
        for (const auto& name : listDir(path)) {
            if (strcasecmp(name.c_str(), part.c_str()) == 0) {
                found = name;
                break;
            }
        }
        if (found.empty()) {
            return "";
        }
        path += "/" + found;
        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }
    return path;
}

bool endsWithNoCase(const std::string& s, const char* ext)
{
    std::size_t n = std::strlen(ext);
    return s.size() >= n && strcasecmp(s.c_str() + s.size() - n, ext) == 0;
}

// The files folder of the first disc: <dir>/disc0/files (port layout),
// <dir>/disc1/files (app layout) or <dir>/files.
std::string filesFolder(const std::string& dir)
{
    for (const char* sub : { "/disc0/files", "/disc1/files", "/files" }) {
        if (isDir(dir + sub)) {
            return dir + sub;
        }
    }
    return "";
}

// DVD container (.drs / core.das): the first part at 0x400 is the archive
// (archive.cpp RoomArchive::LoadDrs).
bool drsBody(const Bytes& file, Bytes& out, std::string& error)
{
    auto word = [&](std::size_t p) {
        return std::uint32_t(file[p]) << 24 | std::uint32_t(file[p + 1]) << 16 | std::uint32_t(file[p + 2]) << 8 |
               file[p + 3];
    };
    if (file.size() < 0x400) {
        error = "DVD-Container zu kurz";
        return false;
    }
    std::uint32_t length = word(0x24), offset = word(0x2C);
    if (word(0x20) != 0 || offset != 0x400 || length < 16 || length > file.size() - offset) {
        error = "kein DVD-Container mit Archiv an 0x400";
        return false;
    }
    out.assign(file.begin() + offset, file.begin() + offset + length);
    return true;
}

void collect(const std::string& dir, const std::string& rel, int depth, std::vector<std::string>& out,
             bool (*want)(const std::string&))
{
    for (const auto& name : listDir(dir)) {
        std::string path = dir + "/" + name;
        std::string r = rel.empty() ? name : rel + "/" + name;
        if (isDir(path)) {
            if (depth < 4) {
                collect(path, r, depth + 1, out, want);
            }
        } else if (want(r)) {
            out.push_back(r);
        }
    }
}

std::vector<std::string> g_rooms;

bool wantEvd(const std::string& rel)
{
    std::string base = rel.substr(rel.rfind('/') == std::string::npos ? 0 : rel.rfind('/') + 1);
    if (!endsWithNoCase(base, ".evd")) {
        return false;
    }
    for (const auto& room : g_rooms) {
        if (strncasecmp(base.c_str(), room.c_str(), room.size()) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<Input> discInputs(const std::string& dir)
{
    std::vector<Input> in;
    std::string files = filesFolder(dir);
    if (files.empty()) {
        in.push_back({ dir, kArchive, {}, "kein Ordner disc0/files, disc1/files oder files gefunden", true });
        return in;
    }
    for (const auto& room : g_rooms) {
        unsigned id = unsigned(std::strtoul(room.c_str() + 1, nullptr, 16));
        std::string rel = "st" + std::to_string(id >> 8) + "/" + room + ".das";
        std::string path = findPath(files, rel);
        Input i{ rel, kArchive, {}, "", true };
        Bytes file;
        if (path.empty() || !readFile(path, file)) {
            i.error = "Datei fehlt";
        } else {
            try {
                i.bytes = appUnpackYz2(file);
            } catch (const std::exception& e) {
                i.error = std::string("Yz2 (App): ") + e.what();
            }
        }
        in.push_back(i);
    }
    for (const auto& name : listDir(findPath(files, "etc"))) {
        if (endsWithNoCase(name, ".esl")) {
            Input i{ "etc/" + name, kEsl, {}, "", false };
            if (!readFile(findPath(files, "etc/" + name), i.bytes)) {
                i.error = "nicht lesbar";
            }
            in.push_back(i);
        }
    }
    // models the game reads one by one, outside an archive (formats.cpp: the
    // pickup screen's item models, the inventory's examine view, the costume
    // models of r300): each as the only entry of an archive, so that both
    // sides read it as the BIN of an archive
    auto standaloneBins = [&](const std::string& dir, const char* prefix) {
        std::string path = findPath(files, dir);
        if (path.empty()) {
            return;
        }
        for (const auto& name : listDir(path)) {
            if (!endsWithNoCase(name, ".bin") ||
                (prefix && strncasecmp(name.c_str(), prefix, std::strlen(prefix)) != 0)) {
                continue;
            }
            Input i{ path.substr(files.size() + 1) + "/" + name, kArchive, {}, "", false };
            Bytes file;
            if (!readFile(path + "/" + name, file)) {
                i.error = "nicht lesbar";
            } else {
                i.bytes = makeArchive({ { "BIN", file } });
            }
            in.push_back(i);
        }
    };
    standaloneBins("ss/cmn", "itm");
    standaloneBins("ss/item", nullptr);
    standaloneBins("etc", nullptr);
    {
        Input i{ "debug/roominfo.dat", kRoomInfo, {}, "", false };
        std::string path = findPath(files, "debug/roominfo.dat");
        if (path.empty() || !readFile(path, i.bytes)) {
            i.error = "Datei fehlt";
        }
        in.push_back(i);
    }
    std::vector<std::string> evds;
    collect(files, "", 0, evds, wantEvd);
    for (const auto& rel : evds) {
        Input i{ rel, kEvd, {}, "", false };
        if (!readFile(files + "/" + rel, i.bytes)) {
            i.error = "nicht lesbar";
        }
        in.push_back(i);
    }
    std::vector<std::string> containers;
    if (!findPath(files, "etc/core.das").empty()) {
        containers.push_back(findPath(files, "etc/core.das"));
    }
    std::string em = findPath(files, "em");
    for (const auto& name : listDir(em)) {
        if (endsWithNoCase(name, ".drs")) {
            containers.push_back(em + "/" + name);
        }
    }
    // codec messages (ss_term.cpp opArc): the archive starts 0x400 bytes into the part
    std::string op = findPath(files, "op");
    std::size_t codecFrom = containers.size();
    for (const auto& name : listDir(op)) {
        if (endsWithNoCase(name, ".das")) {
            containers.push_back(op + "/" + name);
        }
    }
    for (std::size_t c = 0; c < containers.size(); c++) {
        const std::string& path = containers[c];
        Input i{ path.substr(files.size() + 1), c >= codecFrom ? kCodec : kArchive, {}, "", false };
        Bytes file;
        if (!readFile(path, file)) {
            i.error = "nicht lesbar";
        } else if (drsBody(file, i.bytes, i.error) && c >= codecFrom && i.bytes.size() <= 0x420) {
            i.error = "Funkarchiv zu kurz";
        }
        in.push_back(i);
    }
    // the map screen's rooms (Sscrn ss_map.cpp DvdReadN without a part table):
    // the files are the archives
    std::string cmn = findPath(files, "ss/cmn");
    for (const auto& name : listDir(cmn)) {
        if (strncasecmp(name.c_str(), "map_obj", 7) == 0 && endsWithNoCase(name, ".dat")) {
            Input i{ cmn.substr(files.size() + 1) + "/" + name, kArchive, {}, "", false };
            if (!readFile(cmn + "/" + name, i.bytes)) {
                i.error = "nicht lesbar";
            }
            in.push_back(i);
        }
    }
    return in;
}

bool writeFile(const std::string& path, const Bytes& bytes)
{
    for (std::size_t i = 1; i < path.size(); i++) {
        if (path[i] == '/') {
            mkdir(path.substr(0, i).c_str(), 0755);
        }
    }
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    return bool(f);
}

int writeSamples(const std::string& dir)
{
    std::string files = dir + "/disc0/files/";
    bool ok = true;
    for (const auto& in : syntheticInputs()) {
        switch (in.kind) {
        case kArchive:
            if (in.label == kMapSample) {
                ok = writeFile(files + "SS/cmn/map_obj1.dat", in.bytes) && ok;
                break;
            }
            ok = writeFile(files + "st1/r100.arc", in.bytes) && ok;
            ok = writeFile(files + "em/pl00.drs", makeDrs(in.bytes)) && ok;
            {
                Bytes codec(0x400, 0);  // op/opNN.das: the archive 0x400 into the part
                codec.insert(codec.end(), in.bytes.begin(), in.bytes.end());
                ok = writeFile(files + "op/op01.das", makeDrs(codec)) && ok;
            }
            break;
        case kEsl:
            ok = writeFile(files + "etc/emleon00.esl", in.bytes) && ok;
            break;
        case kEvd:
            ok = writeFile(files + "Evd/r100s03.evd", in.bytes) && ok;
            break;
        case kRoomInfo:
            ok = writeFile(files + "debug/roominfo.dat", in.bytes) && ok;
            break;
        case kCodec:
            break;
        }
    }
    // a model the game reads on its own (the pickup screen's item model)
    ok = writeFile(files + "SS/cmn/itm1a.bin", makeBin(false, true, false)) && ok;
    std::printf("%s: %s\n", dir.c_str(), ok ? "samples written" : "write failed");
    return ok ? 0 : 1;
}

int usage()
{
    std::fprintf(stderr,
                 "usage: re4_format_compare                      synthetic samples\n"
                 "       re4_format_compare --data DIR [--room r100 ...] [-v]\n"
                 "       re4_format_compare --write-samples DIR\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv)
{
    std::string data;
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            data = argv[++i];
        } else if (std::strcmp(argv[i], "--room") == 0 && i + 1 < argc) {
            g_rooms.push_back(argv[++i]);
        } else if (std::strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (std::strcmp(argv[i], "--write-samples") == 0 && i + 1 < argc) {
            return writeSamples(argv[i + 1]);
        } else {
            return usage();
        }
    }
    if (g_rooms.empty()) {
        g_rooms.push_back("r100");
    }
    for (auto& room : g_rooms) {
        std::transform(room.begin(), room.end(), room.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    }
    bool synthetic = data.empty();
    std::vector<Input> inputs = synthetic ? syntheticInputs() : discInputs(data);
    std::printf("Formatvergleich: App-Leser (port/android-decomp) gegen Kern-Umwandler (port/whole/runtime/formats)\n");
    if (!synthetic) {
        std::printf("Disc-Daten: %s\n", data.c_str());
    }
    std::size_t files = 0, failed = 0, notComparable = 0, values = 0;
    for (const auto& in : inputs) {
        if (!in.error.empty()) {
            std::printf("  FEHLT    %s: %s\n", in.label.c_str(), in.error.c_str());
            notComparable++;
            if (synthetic || in.required) {
                failed++;
            }
            continue;
        }
        Result r = compare(in);
        files++;
        if (!r.appError.empty()) {
            std::printf("  N/V      %s (%s): App-Leser lehnt die Datei ab: %s\n", in.label.c_str(), kindName(in.kind),
                        r.appError.c_str());
            notComparable++;
            if (synthetic || in.required) {
                failed++;
            }
            continue;
        }
        bool bad = !r.diffs.empty() || !r.incomplete.empty() || r.gameRejected || (synthetic && !r.skipped.empty());
        values += r.equal;
        std::printf("  %-8s %s (%s): %zu Werte gleich, %zu abweichend, %zu Teile nicht vergleichbar\n",
                    bad ? "FEHLER" : "OK", in.label.c_str(), kindName(in.kind), r.equal, r.diffs.size(),
                    r.skipped.size());
        if (r.gameRejected) {
            std::printf("           Kern: Datei nicht angenommen\n");
        }
        for (const auto& p : r.incomplete) {
            std::printf("           Kern: Umwandlung unvollständig: %s\n", p.empty() ? "(Datei)" : p.c_str());
        }
        std::size_t shown = 0;
        for (const auto& d : r.diffs) {
            if (!verbose && shown++ >= 25) {
                std::printf("           ... %zu weitere (alle mit -v)\n", r.diffs.size() - 25);
                break;
            }
            std::printf("           %s\n", d.c_str());
        }
        for (const auto& s : r.skipped) {
            std::printf("           nicht vergleichbar: %s\n", s.c_str());
        }
        // the archive entries behind the differences: position, size, converter status
        std::set<std::string> touched;
        for (const auto& kv : r.entries) {
            for (const auto& d : r.diffs) {
                if (startsWith(d, kv.first)) {
                    touched.insert(kv.first);
                    break;
                }
            }
            for (const auto& p : r.incomplete) {
                if (startsWith(p, kv.first)) {
                    touched.insert(kv.first);
                }
            }
        }
        shown = 0;
        for (const auto& k : touched) {
            if (!verbose && shown++ >= 40) {
                std::printf("           ... %zu weitere Einträge (alle mit -v)\n", touched.size() - 40);
                break;
            }
            std::printf("           Eintrag %s %s\n", k.c_str(), r.entries[k].c_str());
        }
        if (bad) {
            failed++;
        }
    }
    std::printf("Ergebnis: %zu Dateien verglichen, %zu Werte gleich, %zu mit Fehlern, %zu nicht vergleichbar -> %s\n",
                files, values, failed, notComparable, failed ? "FEHLER" : "OK");
    return failed ? 1 : 0;
}
