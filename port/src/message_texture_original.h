#pragma once
#include "re4/port/messages.h"
namespace re4::port {
struct MessageTexHeader {
    u16 height{}, width{};
    u32 format{}, wrapS{}, wrapT{}, minFilter{}, magFilter{};
    f32 LODBias{};
    u8 edgeLODEnable{}, minLOD{}, maxLOD{}, unpacked{};
    ResourceLease data;
};
struct MessageClutHeader { u16 numEntries{}; u8 unpacked{}; u32 format{}; ResourceLease data; };
struct MessageTexDescriptor { MessageTexHeader *textureHeader{}; MessageClutHeader *CLUTHeader{}; };
struct MessageTpl {
    u32 version{}, numDescriptors{};
    MessageTexDescriptor *descriptorArray{};
    ResourceLease source;
    std::vector<MessageTexHeader> textures;
    std::vector<MessageClutHeader> cluts;
    std::vector<MessageTexDescriptor> descriptors;
};
std::shared_ptr<MessageTpl> resolveMessageTpl(ResourceLease,u32 capacity);
void requireMessageTpl(MessageTpl*);
inline MessageTexDescriptor *TEXGet(MessageTpl *p,u32 n) {
    requireMessageTpl(p);
    if(n>=p->numDescriptors) throw std::out_of_range("TEXGet descriptor");
    return p->descriptorArray+n;
}
void GXInitTexObj(MessageTexture*,ResourceLease,u16,u16,u32,u32,u32,int);
void GXInitTexObjCI(MessageTexture*,ResourceLease,u16,u16,u32,u32,u32,int,u32);
void GXInitTlutObj(MessagePalette*,ResourceLease,u32,u16);
} // namespace re4::port
