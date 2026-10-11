#pragma once
#include "re4/port/consumers.h"
#include "re4/port/skinning.h"
#include <array>

namespace re4::port {
// Diagnostic submission data, not a replacement for the original ordering tables.
// Original immutable bytes and IDs remain owned alongside decoded GPU input.
struct GxVertex { float position[3], normal[3], uv[2]; std::array<u8,4> color; float rawNormal[3]; };
struct GxMip { u32 width{}, height{}; std::vector<u8> rgba; };
struct GxTexture {
    ResourceLease source;
    u32 format{}, wrapS{}, wrapT{}, minFilter{}, magFilter{};
    float bias{};
    u8 minLod{}, maxLod{};
    std::vector<GxMip> levels;
};
struct GxDraw {
    u32 firstIndex{}, indexCount{}, texture{}, alphaTexture{};
    u8 flags{}, alphaRef{}, cull{};
    u8 blendMode{}, alphaOverride=255, otType=3;
    u32 materialColor=0xffffffff, smxFlags{};
    float uvScrollU{},uvScrollV{}; // Original velocity; diagnostic is frozen at initialization.
    u32 workIndex{}, workId{}, binId{}, tplId{}, partIndex{};
    u32 modelFlags{};
    int blockNo=-1;
    std::array<u8,9> specular{}; // Original part bytes 0x0d,0x0f..0x13,0x15,0x17 and reserved.
    u32 textureBase{}, textureCount{};
    Mtx modelMatrix{}; // commonModelTrans uses m->mat for specular, not the draw-root matrix.
    Vec modelScaleForSpecular{1,1,1};
    u32 modelStateFlags{};
    std::array<u8,4> color2{};
    Vec lightPosition{}, lightSize{};
    Vec modelPosition{},modelAngle{},modelScale{1,1,1},lightOffset{},rootWorld{};
    Mtx rootMatrix{};
    u32 lightSelectMask=0xffffffff;
    u8 attribute{};
    float radius{};
};
struct GxTevStage {
    std::array<s32,4> colorIn{}, alphaIn{}, colorOp{}, alphaOp{}, order{};
    std::array<float,4> uv{0,0,1,1};
    std::array<s32,4> indirect{}; // enabled, source stage, matrix ID, packed bias/wrap flags.
};
struct GxTexGen { std::array<s32,4> meta{}; std::array<std::array<float,4>,3> rows{}; };
struct GxMaterialState {
    std::array<GxTevStage,16> stages{};
    std::array<std::array<s32,4>,4> konst{};
    std::array<s32,4> meta{};
    std::array<s32,4> alpha{};
    std::array<s32,4> raster{255,255,255,255};
    std::array<s32,4> ambient{};
    std::array<float,4> fog{}, fogColor{};
    // Eight loaded GX light objects: view position, direction, A, K, colour.
    std::array<std::array<float,4>,40> lights{};
    std::array<GxTexGen,8> texgens{};
    std::array<std::array<s32,4>,4> indirectOrder{};
    std::array<std::array<s32,4>,6> indirectMatrices{};
    std::array<s32,4> indirectMeta{}; // indirect count, texgen count, hardware cache mask, reserved.
    std::array<u32,8> textures{};
};
struct GxCameraPoint { Vec Campos{}, Target{}; float Roll{}, Fovy{}; };
struct GxCamera {
    Mtx mat{}, view{};
    float projection[4][4]{};
    GxCameraPoint param;
    Vec Up{}, Look{}, Right{};
};
struct GxScene {
    std::vector<GxVertex> vertices;
    std::vector<u32> indices;
    std::vector<GxTexture> textures;
    std::vector<GxDraw> draws;
    std::vector<GxMaterialState> materials;
    std::vector<u32> renderOrder, otExecution;
    std::string lightHash;
    u32 lightingCalls{}, litCut{}, selectedLightRecords{};
    GxCamera camera;
    std::array<float,16> viewProjection{}; // Column-major for SPIR-V.
    std::vector<ResourceLease> owners;
    std::string cameraName, roomHash, smdHash;
    u32 models{}, primitives{}, cameraFrame{};
    u32 skinnedModels{};
    std::string eventModel, eventMotion, skinHash;
    std::vector<std::string> eventHierarchies;
    u32 scrollWorkCount{};
    std::vector<int> activeBlocks;
    std::vector<std::string> blockHashes,scrollFunctions;
    bool materialLightingReady{};
    u32 specularTextureBase{},specularTextureCount{},indirectTextureBase{},indirectTextureCount{},globalTextureBase{},globalTextureCount{};
    std::string report() const;
};
std::vector<GxTexture> decodeGxTpl(ResourceLease);
// GXCallDisplayList subset from commonModelTrans: POS/NRM/CLR0/TEX0 index16, VAT0.
// Unsupported FIFO commands and all invalid references fail, never disappear.
void decodeGxModel(ResourceLease bin, const Mtx world, u32 textureBase,
                   u32 textureCount, GxScene &, GxDraw identity,const SkinningResult *skin=nullptr);
// Original Dvd/ReadAreaData/Yz2 loader, CPU model initialization, first actual Cam
// packet of the supplied event, and original CameraMotion sampling / SDK matrices.
GxScene loadDiagnosticScene(EngineLoader &, EngineConsumers &, u16 room,
                            const std::string &eventPath, u32 frame = 0,const std::string &eventModel="",u32 cut=UINT32_MAX);
GxScene loadOriginalScrollGeometry(EngineLoader &,EngineConsumers &,u16 room); // No camera/light/GPU success claim.
} // namespace re4::port
