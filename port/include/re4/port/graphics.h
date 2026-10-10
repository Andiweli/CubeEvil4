#pragma once
#include "re4_runtime_types.h"
#include <array>
#include <memory>
#include <thread>
namespace re4::port {
struct GraphicsClear { std::array<u8,4> rgba{}; u32 depth=0xffffff; };
class GraphicsPresenter {
public:
    virtual ~GraphicsPresenter()=default;
    virtual void present(const GraphicsClear &,bool black)=0;
};
// GXSetCopyClear changes BP clear registers. It does not itself clear the EFB.
class GraphicsState final {
public:
    void setCopyClear(GXColor,u32); // Source-guarded original GX register packing.
    void writeRas(u32);
    void checkOutsidePrimitive() const;
    void setBlack(int);
    void attach(std::shared_ptr<GraphicsPresenter>);
    void present();
    GraphicsClear clear() const;
    bool black() const { return black_; }
    bool bound() const { return bool(presenter_); }
    u64 presented() const { return presented_; }
    u8 bpSentNot{};
private:
    const std::thread::id owner_=std::this_thread::get_id();
    std::array<u32,3> registers_{0x4f000000,0x50000000,0x51ffffff};
    bool black_=true;
    u64 presented_{};
    std::shared_ptr<GraphicsPresenter> presenter_;
    void check() const;
};
} // namespace re4::port
