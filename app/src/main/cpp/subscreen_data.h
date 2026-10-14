#pragma once
#include "smd.h"
#include "scene.h"
#include "texture.h"
#include "room_state.h"
#include <array>
#include <string>
#include <vector>

namespace re4 {
// Original SS archives are indexed big-endian offset tables, not tagged room
// archives. Offsets stay immutable; none is relocated to an ARM pointer.
std::vector<std::uint8_t>
CopySubscreenEntry(const std::vector<std::uint8_t> &archive, unsigned index);

struct SubscreenTexture {
  unsigned id{}, frame{};
  Texture image;
  unsigned animationFrames{1}; // TexAnm::numTex; a TPL can also contain effect maps.
};
std::vector<SubscreenTexture>
DecodeSubscreenIdTextures(const std::vector<std::uint8_t> &pack);
struct SubscreenQuad {
  unsigned mark{}, unit{}, parent{255}, kind{}, texture{}, mask{255}, anchor{};
  unsigned blend{}, flags{}, group{};
  std::array<float, 3> position{}, rotation{};
  std::array<float, 8> vertices{}; // x/y pairs, original centre origin, y up.
  std::array<float, 4> color{};
  // Source animations are retained as metadata. The static view uses the last
  // authored scale/colour key; this does not emulate all ID widget animations.
  bool hasPath{}, hasAnimation{};
};
std::vector<SubscreenQuad>
DecodeSubscreenLayout(const std::vector<std::uint8_t> &table,
                      unsigned group = 0);
enum class SubscreenArtKind { Common = 0, Case = 1, Keys = 2, Files = 3, Map = 4 };
struct SubscreenArt {
  std::vector<SubscreenTexture> textures;
  std::vector<SubscreenQuad> quads;
  const Texture *texture(unsigned id, unsigned frame = 0) const;
  std::string layoutJson() const;
};
// Screen-space original case model, projected with sscrnCameraInit and the
// caseModelMove transform. No shipped raster art is required.
Texture LoadOriginalCaseBoardImage(const std::string &discFiles,
                                  unsigned columns, unsigned rows);
// Frozen original subscreen pose. Must be built on the simulation/GL owner
// thread because the hosted original Motion engine is not reentrant.
Texture LoadOriginalMenuPortraitImage(const std::string &discFiles,
                                     unsigned weaponItemId);
struct OriginalMenuModel {
  StaticScene scene;
  Matrix34 placement;
  std::shared_ptr<const RoomLighting> lighting;
  // Independent SS skeleton: its loop continues while room simulation pauses.
  std::vector<std::shared_ptr<class GenericEventActor>> actors;
  std::vector<std::vector<Matrix34>> previousBones;
  std::vector<Matrix34> previousPlacements;
  double renderFraction{1};
  void advance(unsigned ticks);
  std::vector<Matrix34> renderBones(unsigned actor) const;
  Matrix34 renderPlacement(unsigned actor) const;
};
OriginalMenuModel LoadOriginalMenuPortrait(const std::string &discFiles,
                                         unsigned weaponItemId,bool animated=false);
SubscreenArt LoadSubscreenArt(const std::string &discFiles,
                              SubscreenArtKind kind,
                              const std::string &language = "eng");

// Messages store font glyph indices, not Unicode. Returning original words
// prevents an invented transcription of the source document.
std::vector<std::uint16_t>
ReadSubscreenMessage(const std::vector<std::uint8_t> &table,
                     unsigned message, unsigned language = 1);
struct SubscreenGlyph {
  unsigned glyph{}, line{}, color{};
};
std::vector<SubscreenGlyph>
DecodeSubscreenGlyphs(const std::vector<std::uint16_t> &words,bool choiceMarkers=false);
struct SubscreenFont {
  std::vector<Texture> sheets;
  std::vector<std::array<std::int8_t, 2>> bearings;
  unsigned cellWidth{}, cellHeight{};
};
SubscreenFont DecodeSubscreenFont(const std::vector<std::uint8_t> &bytes,
                                 unsigned cellWidth = 32,
                                 unsigned cellHeight = 32);
struct OriginalFilePage {
  unsigned number{}, page{}, pages{}, message{}, picture{};
  std::string picturePath;
};
unsigned OriginalFileNumber(unsigned itemId);
unsigned OriginalFileItem(unsigned number);
OriginalFilePage OriginalFilePageInfo(unsigned number, unsigned page,
                                      const std::string &language = "eng");
unsigned OriginalKeyItemTextureFrame(unsigned itemId);
std::vector<SubscreenGlyph> LoadOriginalFileGlyphs(const std::string &discFiles,
                                                  unsigned number,
                                                  unsigned page,
                                                  const std::string &language = "eng");
Texture LoadOriginalFilePicture(const std::string &discFiles, unsigned number,
                                unsigned page,
                                const std::string &language = "eng");
SubscreenFont LoadOriginalMenuFont(const std::string &discFiles);
std::shared_ptr<const RoomLighting>
LoadOriginalExamLighting(const std::string &discFiles, unsigned light);
std::vector<SubscreenQuad> LoadOriginalExamLayout(const std::string &discFiles);
SubscreenArt LoadOriginalExamArt(const std::string &discFiles);

struct OriginalItemModel {
  SmdModel mesh;
  std::vector<Texture> textures;
  std::array<float, 3> rotationDegrees{};
  float viewRatio{1};
  unsigned light{};
  std::uint32_t sourceFlags{};
};
struct OriginalExamProfile {
  std::array<float, 3> rotationDegrees{};
  float scale{1}, fovy{30}, aspect{4.f / 3.f};
  unsigned light{}, inventoryAxis{}, pickupAxis{};
  bool authored{};
};
OriginalExamProfile OriginalItemExamProfile(unsigned itemId,
                                           bool inventory = true);
OriginalItemModel LoadOriginalItemModel(const std::string &discFiles,
                                        unsigned itemId,
                                        bool examine = true, const RoomArchive *room = nullptr);
StaticScene LoadOriginalItemScene(const std::string &discFiles,
                                  unsigned itemId, bool examine = true, const RoomArchive *room = nullptr);
struct SubscreenSpecular {
  bool enabled{}, bump{};
  unsigned type{};
  float scale{};
  std::array<float,3> rgb{};
  Texture image;
};
// One record per LoadOriginalItemScene.draws entry, in identical source order.
// Normal-coordinate environment textures and constants come from the original
// primitive header / CORE palette. Bump variants are retained as metadata.
std::vector<SubscreenSpecular>
LoadOriginalItemSpecular(const std::string &discFiles, unsigned itemId, bool examine = true, const RoomArchive *room = nullptr);
Texture LoadOriginalItemIcon(const std::string &discFiles, unsigned itemId);
// Exact CORE_MESSAGE_3 item-name glyphs, cropped transparent original-font
// image. This is deliberately not an invented Unicode transcription.
Texture LoadOriginalItemName(const std::string &discFiles, unsigned itemId);
// Message::WidthCk/QueSet, CORE_MESSAGE_0 plus name/number/choice slots.
Texture LoadOriginalPickupMessage(const std::string &discFiles,unsigned itemId,
                                  unsigned message,unsigned count,unsigned selection,bool caseMessage=false,std::vector<std::array<int,4>> *choiceBounds=nullptr);
Texture LoadOriginalFilePage(const std::string &discFiles, unsigned fileItemId,
                             unsigned page);
// All models/floors of the source village map room. Coordinates are the
// authored mini-map model coordinates, not world-room collision coordinates.
std::vector<SmdModel>
ReadOriginalVillageMapRoom(const std::vector<std::uint8_t> &mapArchive,
                           unsigned room);
// Authored BIN map models; paired SAT is used only to locate the player.
// Campaign progress supplies the source mapColor state. An inspector may
// omit it, in which case visited/locked states remain explicitly unknown.
std::string OriginalMapSnapshotJson(const std::string &discFiles, unsigned room,
                                    const std::array<double, 3> &player,
                                    float heading, int stage = -1,
                                    const ScenarioState *scenario = nullptr,
                                    const std::map<std::uint16_t,std::uint8_t> *passed = nullptr,
                                    const std::string &language = "eng");
} // namespace re4
