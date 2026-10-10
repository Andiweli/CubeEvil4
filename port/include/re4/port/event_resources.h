#pragma once
#include "re4/port/engine_loader.h"
#include "re4_runtime_types.h"
#include <array>
#include <map>
#include <string>
#include <vector>

namespace re4::port {
struct EventPackageView;
// Native pointers; this is not the 0x3c-byte on-disc/GameCube slot layout.
struct DatTblWork {
    char Name[0x30]{};
    u8 FlagBe8{}, Etc{};
    u16 Count{};
    void *Dat{}, *dat2{};
};
class NativeDatTbl {
public:
    explicit NativeDatTbl(int count);
    NativeDatTbl(const NativeDatTbl &)=delete;
    NativeDatTbl &operator=(const NativeDatTbl &)=delete;
    int NumDatTbl;
    DatTblWork *pWork;
    int SetDat(const char *, void *, u8, void *, u8, int *);
    int GetDat(void **, u8 *, const char *, int *);
    int ChkDat(const char *);
    int DelDat(const char *);
    int DelDatWkNo(int);
    bool contains(const void *) const;
    void clear();
private:
    std::vector<DatTblWork> storage_;
};
// Original SetBin/GetBin/SetEvd algorithms over checked immutable leases.
class EventResources {
public:
    EventResources();
    int SetBin(const char *, void *, void *, int);
    int GetBin(void **, const char *, int);
    int SetEvd(const char *, void *, void *, int);
    void registerBin(const std::string &, ResourceLease);
    void registerPackage(const std::string &, const EventPackageView &);
    void bindPlayerArchive(ResourceLease body);
    ResourceLease bin(const std::string &);
    bool valid(const void *) const;
    void clear(); // Only after the consumer's task borrowers have quiesced.
    NativeDatTbl BinTbl{0x140}, EvdTbl{0x20};
private:
    std::map<std::string, ResourceLease> leases_;
    std::map<std::string, std::shared_ptr<EventPackageView>> packages_;
};
} // namespace re4::port
