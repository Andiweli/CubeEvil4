#include "scene.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
int main() {
  std::array<std::uint8_t,32> material{};material[11]=6;material[15]=2;material[16]=30;material[17]=28;material[18]=25;material[21]=150;
  re4::SceneDraw window;re4::SetSceneSpecular(window,material,0);
  assert(window.specularEnabled&&!window.specularBump&&!window.specularLocal&&window.specularImage==2&&window.specularType==0&&window.specularScale==1.5f);
  re4::SetSceneSpecular(window,material,0x20000000);assert(window.specularScale==.75f);
  material[11]|=0x80;re4::SetSceneSpecular(window,material,0);assert(window.specularBump&&!window.specularEnabled);
  re4::SmdPlacement p;
  p.scale = {2, 3, 4};
  p.position = {10, 20, 30};
  p.rotation = {0, 0, 1.57079632679f};
  auto m = re4::PlacementMatrix(p);
  auto v = re4::TransformPoint(m, {1, 2, 3});
  assert(std::abs(v[0] - 4) < 1e-5 && std::abs(v[1] - 22) < 1e-5 &&
         std::abs(v[2] - 42) < 1e-5);
  p.rotation = {1.57079632679f, 0, 0};
  p.scale = {1, 1, 1};
  p.position = {0, 0, 0};
  v = re4::TransformPoint(re4::PlacementMatrix(p), {0, 1, 0});
  assert(std::abs(v[2] - 1) < 1e-5);
  re4::SmdModel model;
  model.joints = {{0, 255, {1, 2, 3}}, {1, 0, {4, 5, 6}}, {2, 1, {-1, 0, 2}}};
  auto centers = re4::RestJointCenters(model);
  assert((centers[2] == std::array<float, 3>{4, 7, 11}));
  model.joints[1].parent = 1;
  bool failed = false;
  try {
    (void)re4::RestJointCenters(model);
  } catch (const std::exception &) {
    failed = true;
  }
  assert(failed);
  p.scale[0] = std::numeric_limits<float>::infinity();
  failed = false;
  try {
    (void)re4::PlacementMatrix(p);
  } catch (const std::exception &) {
    failed = true;
  }
  assert(failed);
  re4::Matrix34 scaled{2, 0, 0, 90, 0, 3, 0, 80, 0, 0, 4, 70};
  auto normal = re4::TransformNormal(re4::NormalMatrix(scaled), {1, 1, 1});
  assert(std::abs(normal[0] - .5f) < 1e-6 &&
         std::abs(normal[1] - 1.f / 3) < 1e-6 &&
         std::abs(normal[2] - .25f) < 1e-6);
  scaled[0] = -2;
  assert(re4::TransformNormal(re4::NormalMatrix(scaled), {1, 0, 0})[0] == -.5f);
  scaled[0] = 0;
  assert((re4::TransformNormal(re4::NormalMatrix(scaled), {1, 1, 1}) ==
          std::array<float, 3>{0, 0, 0}));
  // Construct a source-free minimal empty SMD, then insert a rigid triangle.
  std::vector<std::uint8_t> bytes(16);
  bytes[0] = 64;
  re4::Smd s(bytes);
  p = {};
  p.scale = {1, 1, 1};
  p.motion = 255;
  s.placements.push_back(p);
  model = {};
  model.parts = 1;
  model.joints = {{0, 255, {2, 0, 0}}};
  model.normals = {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}};
  model.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  re4::GxPrimitive primitive;
  primitive.vertices = {{0, 0, 0, 0}, {1, 0, 0, 0}, {2, 0, 0, 0}};
  primitive.triangles = {{0, 1, 2}};
  model.texcoords = {{0, 0}, {.25f, .5f}, {1, 1}};
  primitive.vertices[0].texcoord = 2;
  primitive.vertices[1].texcoord = 0;
  primitive.vertices[2].texcoord = 1;
  model.primitives.push_back(primitive);
  model.primitives[0].vertices[0].normal = 2;
  model.primitives[0].vertices[1].normal = 0;
  model.primitives[0].vertices[2].normal = 1;
  s.models.push_back(model);
  auto scene = re4::BuildStaticScene(s);
  assert(scene.included == 1 && scene.triangles == 1 &&
         scene.vertices[0] == 2 && scene.maximum[0] == 3);
  assert(scene.uv == std::vector<float>({1, 1, 0, 0, .25f, .5f}));
  assert(scene.normals == std::vector<float>({0, 1, 0, 0, 0, 1, 1, 0, 0}));
  assert(scene.zeroNormals == 0 && scene.singularNormalPlacements == 0);
  assert(scene.draws.size() == 1 && scene.draws[0].count == 3);
  assert(scene.colors == std::vector<std::uint8_t>(12, 255));
  s.models[0].colored = true;
  s.models[0].vertexAmbient = true;
  s.models[0].colors = {{255, 0, 0, 255}, {0, 128, 0, 64}, {0, 0, 255, 0}};
  s.models[0].primitives[0].vertices[0].color = 2;
  s.models[0].primitives[0].vertices[1].color = 0;
  s.models[0].primitives[0].vertices[2].color = 1;
  auto colorScene = re4::BuildStaticScene(s);
  assert(
      colorScene.colors ==
      std::vector<std::uint8_t>({0, 0, 255, 0, 255, 0, 0, 255, 0, 128, 0, 64}));
  assert(colorScene.vertexColorTriangles == 1 &&
         colorScene.draws[0].vertexColors);
  s.models[0].vertexAmbient = false;
  auto registerScene = re4::BuildStaticScene(s);
  assert(registerScene.colors == std::vector<std::uint8_t>(12, 255) &&
         !registerScene.vertexColorTriangles);
  s.models[0].vertexAmbient = true;
  s.models[0].primitives[0].vertices[0].color = 3;
  failed = false;
  try {
    (void)re4::BuildStaticScene(s);
  } catch (const std::exception &) {
    failed = true;
  }
  assert(failed);
  s.models[0] = model;
  std::vector<std::uint8_t> smxBytes(16 + 144 * 2);
  smxBytes[0] = 16;
  smxBytes[1] = 2;
  smxBytes[16] = 0;
  smxBytes[28] = 128;
  smxBytes[29] = 64;
  smxBytes[30] = 32;
  smxBytes[31] = 4;
  smxBytes[160] = 0;
  smxBytes[172] = 255; // Duplicate: first match wins.
  re4::Smx smx(smxBytes);
  assert(smx.find(0) == &smx.records[0] && !smx.find(254) && !smx.find(250) &&
         !smx.find(253));
  re4::ApplySceneSmx(smx, scene);
  assert(
      (scene.draws[0].materialRGB == std::array<std::uint8_t, 3>{128, 64, 32}));
  assert(scene.materialColorTriangles == 1 && scene.smxDraws == 1);
  assert(smx.records[0].material ==
         0x80402004); // Low byte is blend mode, not alpha.
  assert(scene.draws[0].blendMode == 4);
  smx.records[0].specular=0x12345604;
  re4::ApplySceneSmx(smx,scene);
  assert((scene.draws[0].specularRGB==std::array<std::uint8_t,3>{0x12,0x34,0x56}));
  smx.records[0].specular=4;scene.draws[0].specularRGB={30,28,25};
  re4::ApplySceneSmx(smx,scene);
  assert((scene.draws[0].specularRGB==std::array<std::uint8_t,3>{30,28,25}));
  // Original SMX alpha-omit flag overrides BIN reference without changing its
  // blend mode. OT ties reverse whole objects while keeping display lists.
  smx.records[0].flags |= 8;
  scene.draws[0].alphaTest = true;
  re4::ApplySceneSmx(smx, scene);
  assert(scene.draws[0].alphaReference == 128 && scene.draws[0].blendMode == 4);
  scene.draws[0].alphaTest = false;
  std::vector<re4::SceneDraw> ordered(8);
  for (unsigned i = 0; i < ordered.size(); ++i) {
    ordered[i].authored = true;
    ordered[i].object = i;
    ordered[i].orderType = i;
    ordered[i].sortCenter = {0, 0, 100};
  }
  assert(re4::SceneDrawOrder(ordered, {0, 0, 0}, {0, 0, 1}) ==
         std::vector<std::size_t>({2, 3, 7, 4, 0, 5, 1, 6}));
  ordered.resize(3);
  for (auto &d : ordered) {
    d.orderType = 0;
    d.sortCenter = {0, 0, 100};
  }
  ordered[0].sortCenter[2] = 200;
  assert(re4::SceneDrawOrder(ordered, {0, 0, 0}, {0, 0, 1}) ==
         std::vector<std::size_t>({0, 2, 1}));
  smxBytes[28] = smxBytes[29] = smxBytes[30] = 0;
  re4::Smx zeroRGB(smxBytes);
  re4::ApplySceneSmx(zeroRGB, scene);
  assert((scene.draws[0].materialRGB ==
          std::array<std::uint8_t, 3>{255, 255, 255}));
  smxBytes[152] = 0x7f;
  smxBytes[153] = 0x80; // Infinite UV speed.
  failed = false;
  try {
    re4::Smx invalid(smxBytes);
  } catch (const std::exception &) {
    failed = true;
  }
  assert(failed);
  smxBytes.resize(16 + 143);
  failed = false;
  try {
    re4::Smx truncated(smxBytes);
  } catch (const std::exception &) {
    failed = true;
  }
  assert(failed);
  // Minimal source-free RGB565 TPL; two draws share one decoded texture.
  std::vector<std::uint8_t> tpl(96);
  auto put = [&](std::size_t p, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
      tpl[p + i] = v >> (24 - i * 8);
  };
  put(0, 0x0020af30);
  put(4, 1);
  put(8, 12);
  put(12, 20);
  tpl[21] = 4;
  tpl[23] = 4;
  put(24, 4);
  put(28, 64);
  put(32, 2);
  put(36, 0);
  for (unsigned i = 64; i < 96; i += 2) {
    tpl[i] = 0xf8;
    tpl[i + 1] = 0;
  }
  s.palettes.push_back(tpl);
  scene.draws.push_back(scene.draws[0]);
  re4::AttachSceneTextures(s, scene);
  assert(scene.textures.size() == 1 && scene.texturedTriangles == 2 &&
         scene.fallbackTriangles == 0);
  assert(scene.textures[0].rgba[0] == 255 && scene.textures[0].wrapS == 2);
  scene.draws[1].repeat = true;
  re4::AttachSceneTextures(s, scene);
  assert(scene.textures.size() == 2 && scene.textures[1].wrapS == 1 &&
         scene.textures[1].wrapT == 1);
  scene.draws[1].image =
      1; // Additional/common resource is unresolved, no invented mapping.
  re4::AttachSceneTextures(s, scene);
  assert(scene.textures.size() == 1 && scene.texturedTriangles == 1 &&
         scene.fallbackTriangles == 1);
  scene.draws[0].alphaTest = true;
  scene.draws[0].alphaImage = 0;
  scene.draws[0].alphaReference = 127;
  re4::AttachSceneTextures(s, scene);
  assert(scene.alphaTriangles == 1 && scene.unresolvedAlphaTriangles == 0);
  assert(scene.draws[0].texture == scene.draws[0].alphaTexture);
  scene.draws[0].alphaImage = 0xf8;
  re4::AttachSceneTextures(s, scene);
  assert(scene.alphaTriangles == 0 && scene.unresolvedAlphaTriangles == 1 &&
         scene.draws[0].alphaTexture == -1);
  std::array<std::uint8_t, 32> alphaHeader{};
  alphaHeader[11] = 4;
  alphaHeader[14] = 3;
  alphaHeader[20] = 255;
  s.models[0].materialHeaders = {alphaHeader};
  auto alphaScene = re4::BuildStaticScene(s);
  assert(alphaScene.draws[0].alphaTest && alphaScene.draws[0].alphaImage == 3 &&
         alphaScene.draws[0].alphaReference == 255);
  re4::Smd common(bytes);
  auto blue = tpl;
  for (unsigned i = 64; i < 96; i += 2) {
    blue[i] = 0;
    blue[i + 1] = 31;
  }
  common.palettes.push_back(blue);
  scene.draws[0].alphaImage = 1;
  scene.draws[1].image = 1;
  re4::AttachSceneTextures(s, scene, &common);
  assert(scene.texturedTriangles == 2 && scene.fallbackTriangles == 0);
  assert(scene.commonTextureTriangles == 1 && scene.commonAlphaTriangles == 1);
  assert(scene.textures.at(scene.draws[0].texture).rgba[0] == 255);
  assert(scene.textures.at(scene.draws[0].alphaTexture).rgba[2] == 255);
  assert(scene.textures.at(scene.draws[1].texture).rgba[2] == 255);
  assert(scene.draws[0].texture != scene.draws[0].alphaTexture);
  scene.draws[1].image = 2;
  re4::AttachSceneTextures(s, scene, &common);
  assert(scene.fallbackTriangles == 1); // Past appended descriptors.
  scene.draws[0].alphaImage = 0xf8;
  re4::AttachSceneTextures(s, scene, &common);
  assert(scene.unresolvedAlphaTriangles == 1); // No render-target guess.
  scene.draws[0].commonModel = true;
  scene.draws[0].image = scene.draws[0].alphaImage = 0;
  re4::AttachSceneTextures(s, scene, &common);
  assert(scene.textures.at(scene.draws[0].texture).rgba[2] == 255);
  common.models = s.models;
  s.placements[0].flags |= 0x10;
  auto missingCommon = re4::BuildStaticScene(s);
  assert(missingCommon.skipped == 1 &&
         missingCommon.missingCommonPlacements == 1);
  auto commonScene = re4::BuildStaticScene(s, -1, &common);
  assert(commonScene.included == 1 && commonScene.commonPlacements == 1);
  assert(commonScene.draws[0].commonModel && commonScene.draws[0].palette == 0);
  re4::AttachSceneTextures(s, commonScene, &common);
  assert(commonScene.texturedTriangles == 1 &&
         commonScene.commonTextureTriangles == 1);
  // Empty local TPL is valid: common starts at original image index zero.
  auto empty = tpl;
  empty[7] = 0;
  s.palettes[0] = empty;
  s.placements[0].flags &= ~0x10u;
  s.models[0].materialHeaders.clear();
  auto appendedScene = re4::BuildStaticScene(s);
  re4::AttachSceneTextures(s, appendedScene, &common);
  assert(appendedScene.commonTextureTriangles == 1 &&
         appendedScene.texturedTriangles == 1);
  assert(appendedScene.textures[0].rgba[2] == 255);
  s.palettes[0] = tpl;
  assert(s.commonRequirements().palettes == 1);
  s.placements[0].flags |= 0x50;
  s.placements[0].bin = 3;
  s.placements[0].motion = 2;
  auto required = s.commonRequirements();
  assert(required.bins == 4 && required.motions == 3 && required.palettes == 1);
  s.placements[0].flags = 0;
  s.placements[0].bin = 0;
  s.placements[0].motion = 255;
  s.palettes[0][1] = 0;
  failed = false;
  try {
    re4::AttachSceneTextures(s, scene);
  } catch (const std::exception &) {
    failed = true;
  }
  assert(failed);
  assert(re4::BuildStaticScene(s, 0).included == 1);
  assert(re4::BuildStaticScene(s, 1).vertices.empty());
  s.placements[0].motion = 0;
  scene = re4::BuildStaticScene(s);
  assert(scene.vertices.empty() && scene.skipped == 1);
  s.placements[0].motion = 255;
  s.models[0].weightPalette = 2;
  scene = re4::BuildStaticScene(s);
  assert(scene.vertices.empty() && scene.skipped == 1);
  std::cout << "Scene matrices, UV seams, base textures, deduplication, wrap "
               "and fallback PASS\n";
}
