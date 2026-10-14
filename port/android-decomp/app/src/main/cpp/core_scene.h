#pragma once
// Core mode (armeabi-v7a): the original game core (port/whole, libre4_main.so)
// runs the whole game; this renderer draws its state with the app's GLES3
// parts. Per frame the core publishes a snapshot (port/whole/runtime/re4_core.h):
//   - the room id: the room geometry comes from the app's own room loader
//     (SMD/SMX/LIT/blocks of the decoded room archive, RoomRepository in Java),
//   - pG->Camera (eye, target, roll, fov),
//   - every live model with the world matrices of its parts and the disc
//     source of its BIN/TPL: the same data is read from the imported disc and
//     drawn as a generic skinned actor posed with the core's matrices.
// Before the first room (title, menus) the original title picture is shown.
// The core's own drawing (GX) is not used; effects, 2D menus and the HUD are
// missing until the GX path exists.
#include "re4_core.h"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace re4 {

// One published core frame, copied out of the core's buffer (the buffer goes
// back to the core right away).
struct CoreFrame {
  bool valid{};
  std::uint32_t serial{}, frame{};
  std::array<std::uint8_t, 4> step{};
  std::uint16_t room{};
  int disc = -1;
  bool cameraValid{};
  std::array<float, 3> eye{}, target{};
  float roll{}, fovy{};
  std::uint32_t screen{};  // RE4_CORE_SCREEN_*
  std::vector<Re4CoreModel> models;
  std::vector<Re4CorePart> parts;
};
void CopyCoreFrame(const Re4CoreSnapshot &snapshot, CoreFrame &out);

// Path of `source` inside the imported disc (case-insensitive below files/,
// the core names files in lower case); empty when it does not exist.
std::string ResolveCoreDiscPath(const std::string &gameRoot, int disc,
                                const std::string &file);
class RoomArchive;
// BIN/TPL bytes (big-endian, as on the disc) of a core source, from
// innerOffset to the end of its archive entry. A Yz2-packed source (room
// archive, fileOffset 0xFFFFFFFF) comes from `packed`, the decoded archive
// the app prepared for that room; without it, it is refused.
std::vector<std::uint8_t> ReadCoreSource(const std::string &gameRoot, int disc,
                                         const Re4CoreSource &source,
                                         const RoomArchive *packed = nullptr);
// Models the app draws itself: the room's scenery objects (ObjMgr id 2,
// cObjScr: the SMD models of the room and its blocks, part of the static
// room scene).
bool CoreModelIsScenery(const Re4CoreModel &model);

class GenericEventActor;

class CoreScene {
 public:
  // gameRoot: the app's import folder (disc1/, disc2/ with files/).
  explicit CoreScene(std::string gameRoot);
  ~CoreScene();
  CoreScene(const CoreScene &) = delete;
  CoreScene &operator=(const CoreScene &) = delete;

  // GL thread: a new GL context; names of the old one are gone.
  void contextCreated();
  // Any thread: the decoded room archive for `room` is ready (or failed).
  void roomPrepared(std::uint16_t room, const std::string &decodedPath,
                    const std::string &error);
  // Any thread: the room the current frame is in (Java prepares its archive),
  // -1 while the game is not in a room.
  int wantedRoom() const;
  // GL thread: draws the frame into the bound framebuffer.
  void draw(const CoreFrame &frame, int width, int height, double seconds);
  // Any thread: a few lines for the diagnostic overlay.
  std::string status() const;

 private:
  struct Actor;
  struct RoomState;
  Actor *actorFor(const Re4CoreModelInfo &info, int disc);
  void buildRoom(std::uint16_t room, const std::string &path);
  void drawTitle(int width, int height);
  void drawActors(const CoreFrame &frame, const float *camera,
                  const std::array<float, 3> &center, float scale,
                  bool normalView);

  std::string gameRoot_;
  mutable std::mutex lock_;
  // shared with other threads (lock_)
  int wantedRoom_ = -1;
  std::uint16_t preparedRoom_{};
  std::string preparedPath_, preparedError_;
  bool preparedPending_{};
  std::string status_;
  // GL thread only
  std::unique_ptr<RoomState> room_;
  std::unique_ptr<RoomArchive> roomArchive_;  // decoded archive of room_ (packed model sources)
  std::string roomArchiveName_;               // its name on the disc, e.g. "st1/r100.das"
  std::map<std::string, std::unique_ptr<Actor>> actors_;
  // last known disc source per (model, info): the core loses some while a
  // sub screen has memory swapped out
  std::map<std::uint64_t, Re4CoreModelInfo> knownSources_;
  // per-info poses (rigid models, moved infos), reused across frames
  std::vector<std::array<float, 12>> rigidPose_, movedPose_;
  std::vector<std::array<float, 3>> rigidBind_;
  std::vector<std::uint8_t> coreEff_;
  bool coreLoaded_{};
  unsigned titleTexture_{}, titleProgram_{}, titleVao_{}, titleVbo_{};
  unsigned titleWidth_{}, titleHeight_{};
  bool titleTried_{};
  std::string roomError_;
  unsigned drawnModels_{}, missingModels_{}, sceneryModels_{};
};

}  // namespace re4
