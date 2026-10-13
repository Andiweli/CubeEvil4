#include "re4/port/graphics.h"
#include "re4/port/game_runtime.h"
#include <stdexcept>
namespace re4::port {
void GraphicsState::check() const {
    if(owner_!=std::this_thread::get_id()) throw std::logic_error("graphics frame owner");
}
void GraphicsState::checkOutsidePrimitive() const { check(); }
void GraphicsState::writeRas(u32 r) {
    check(); const auto id=r>>24;
    if(id<0x4f || id>0x51) throw std::invalid_argument("unsupported GX BP bootstrap command");
    registers_[id-0x4f]=r;
}
GraphicsClear GraphicsState::clear() const {
    check(); return {{{u8(registers_[0]),u8(registers_[1]>>8),u8(registers_[1]),u8(registers_[0]>>8)}},registers_[2]&0xffffff};
}
void GraphicsState::setBlack(int black) {
    check();
    if(!presenter_) throw MissingGameConsumer("systemVISetBlack/presentation adapter");
    black_=black==1;
}
void GraphicsState::attach(std::shared_ptr<GraphicsPresenter> p) { check(); presenter_=std::move(p); }
void GraphicsState::present() {
    check(); if(!presenter_) throw MissingGameConsumer("VI/frame presentation backend");
    presenter_->present(clear(),black_); ++presented_;
}
} // namespace re4::port
