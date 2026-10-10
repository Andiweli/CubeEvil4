#pragma once
#include "re4/port/consumers.h"
#include <array>

namespace re4::port {
// Diagnostic submission data, not a replacement for the original ordering tables.
// Original immutable bytes and IDs remain owned alongside decoded GPU input.
struct GxVertex { float position[3], normal[3], uv[2]; std::array<u8,4> color; };
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
};
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
    std::string report() const;
};
std::vector<GxTexture> decodeGxTpl(ResourceLease);
// GXCallDisplayList subset from commonModelTrans: POS/NRM/CLR0/TEX0 index16, VAT0.
// Unsupported FIFO commands and all invalid references fail, never disappear.
void decodeGxModel(ResourceLease bin, const Mtx world, u32 textureBase,
                   u32 textureCount, GxScene &, GxDraw identity);
// Original Dvd/ReadAreaData/Yz2 loader, CPU model initialization, first actual Cam
// packet of the supplied event, and original CameraMotion sampling / SDK matrices.
GxScene loadDiagnosticScene(EngineLoader &, EngineConsumers &, u16 room,
                            const std::string &eventPath, u32 frame = 0);
} // namespace re4::port
