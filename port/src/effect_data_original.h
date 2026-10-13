#pragma once
#include "message_texture_original.h"
#include <array>
namespace re4::port {
void C_MTXIdentity(Mtx);
constexpr u32 NativeEffectNone=210,NativeEffectMax=211,NativeEffectTexMax=500;
struct EffectIds {struct Entry{u16 id{},x2{};u32 x4{};};u32 num{};std::vector<Entry> ent;ResourceLease source;};
struct EffectOffsets {u32 num{};std::vector<ResourceLease> entries;ResourceLease source;};
struct EffectAnimation {u16 Width{},Height{};s16 Cx{},Cy{};u16 Frames{};u8 Xn{},Loop{},Data_num{};ResourceLease source;};
struct EffectModelData {ResourceLease model,tpl,mot,shape;};
struct EffectDataFile {u32 Num{};ResourceLease source;std::array<EffectIds,5> ids;std::array<EffectOffsets,6> tables;
    std::vector<std::shared_ptr<MessageTpl>> textures;std::vector<EffectAnimation> animations;std::vector<EffectModelData> models;};
std::shared_ptr<EffectDataFile> resolveEffectData(ResourceLease);
struct EffectTextureWork {
    MessageTexture *pTex_obj_start{};u16 nTexObj{};MessagePalette tlut;MessageTexHeader *texHdr{};Mtx _Mtx{};
    MessageTpl *Tpl_addr{};EffectAnimation *Anm_addr{};u32 Owner=NativeEffectNone;
};
struct EffectTableWork {EffectOffsets *data{};EffectIds *list{};u32 owner=NativeEffectNone;};
struct EffectModelWork {ResourceLease model,tpl,mot,pShapeHeader;u32 owner=NativeEffectNone;};
struct EffectSystemCpu {
    EffectSprite *EspArray{};u32 nEsp{},ActiveEspNum{};
    EffectTextureWork Esp_tex_tbl[256];EffectModelWork efmWk[256];
    EffectTableWork estTbl[NativeEffectMax],sstTbl[NativeEffectMax],pathTbl[NativeEffectMax];
    u8 ownerCnt[NativeEffectMax]{};MessageTexture texObj[NativeEffectTexMax];u8 TexObj_flg[0x3f]{};
    int GetTexObjFlag(u32);void SetTexObjFlag(u32,int);
};
} // namespace re4::port
