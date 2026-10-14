#pragma once
#include "event_playback.h"
#include "r100_scenario.h"
#include "leon_preview.h"
namespace re4 {
class EnemyEncounter;
RoomDoor OriginalR100CorpseArea(const Matrix34 &corpsePlacement);
EventCamera OriginalR100CorpseCamera(const Matrix34 &playerPlacement,
    const std::vector<Matrix34> &playerParts,const Matrix34 &corpsePlacement,
    const std::vector<Matrix34> &corpseParts);
// Original CameraControl::HermiteExport / CameraMotion for a forced room
// CAM cut. This owns the actual B404 records, never a fabricated camera.
class R100RoomCamera {
public:
  R100RoomCamera(const std::vector<std::uint8_t> &cam,unsigned cut);
  void reset();
  void tick30Hz(bool advance=true);
  const EventCamera &camera() const {return camera_;}
  unsigned frame() const {return frame_;}
  unsigned maximumFrame() const {return curve_->maxFrame();}
  bool finished() const {return finished_;}
  unsigned lightArea() const {return lightArea_;}
private:
  void sample();
  std::unique_ptr<EventCurve> curve_;
  EventCamera camera_;
  unsigned frame_{};
  unsigned lightArea_{};
  bool finished_{};
};
struct R100RoomSound {
  enum Kind { Sample, RoomStreamStart, RoomStreamStop, StreamFade, RoomStreamVolume, RoomStreamReset } kind=Sample;
  unsigned block{},number{},frames{},volume{};
  CollisionVec position{};
};
struct R100Threat { bool foundPlayer{};double distanceSquared{}; };
// Original cObjWep::drawLaserSight + R100Init's room AtEffInfo table.
bool OriginalR100DontFire(unsigned hitResult,std::uint32_t attributes,
    bool targetDontFire,double distanceSquared);
struct R100RoomMessage {
  unsigned number{},attributes{},x{},y{},font{};
  unsigned fontWidth{},fontHeight{},lineGap{};
  int charGap{};
  std::size_t pageStart{};
  std::vector<std::uint16_t> words;
  std::vector<SubscreenGlyph> glyphs;
};
// Source Message::move for the control codes present in actual r100 MDT.
// It preserves glyph speed, pages, input waits and timed waits at30Hz.
class R100MessageRuntime {
public:
  explicit R100MessageRuntime(R100RoomMessage message);
  void tick30Hz(bool confirmPressed=false,bool advance=true);
  const R100RoomMessage &message() const {return message_;}
  bool finished() const {return finished_;}
  bool waitingInput() const {return inputWait_;}
private:
  void clearPage();
  R100RoomMessage message_;
  std::size_t cursor_{};
  unsigned line_{},color_{},speed_=1,speedCount_{},wait_{};
  bool inputWait_{},endPending_{},instant_{},finished_{};
};
// R100Init's persistent SetObjSmd cars, officer bodies/addModels and
// region-specific hanging corpse. Source ARC numbers include four header
// words; every resource binding checks the actual BIN/TPL/FCV tag.
class R100Secondary {
public:
  R100Secondary(const RoomArchive &room,const std::string &em12Drs,
      const EnemySpawns &spawns,const RoomSaveState &state,unsigned country=1);
  const std::vector<std::unique_ptr<GenericEventActor>> &actors() const {return actors_;}
  const EnemySpawn &houseSpawn() const {return houseSpawn_;}
  void installPlayerMotion(LeonPreview &player);
  void beginRadioIntro(LeonPreview &player,WalkProbe &probe,unsigned callNo);
  void tickRadioIntro30Hz(LeonPreview &player,WalkProbe &probe);
  void finishRadioIntro(LeonPreview &player);
  bool radioIntroFinished(const LeonPreview &player) const;
  bool radioHandVisible() const {return radioIntroTicks_>=22;}
  unsigned radioIntroFrame() const {return radioIntroTicks_;}
  const GenericEventActor &radioActor() const {return *radioActor_;}
  Matrix34 radioMatrix(const LeonPreview &player) const;
  void beginHouse(LeonPreview &player,WalkProbe &probe);
  void finishHouse(LeonPreview &player);
  void beginCamera(unsigned cut);
  void beginCorpseCamera(const LeonPreview &player,const EnemyEncounter &corpse);
  bool corpseNoSuspend() const {return corpseNoSuspend_;}
  std::optional<R100SecondaryOffer> corpseOffer(const RoomSaveState &state,
      const EnemyEncounter &corpse,const CollisionVec &feet,double heading,
      bool pressed,const std::optional<CollisionVec> &front={}) const;
  void endCamera();
  bool cameraActive() const {return activeCamera_!=nullptr || corpseCameraActive_;}
  std::optional<unsigned> cameraCut() const;
  bool cameraFinished() const {return corpseCameraActive_ || (activeCamera_ && activeCamera_->finished());}
  const EventCamera &camera() const;
  unsigned cameraFrame() const;
  void tickCamera30Hz(bool advance=true);
  void setAfterTruck(bool after);
  bool afterTruck() const {return afterTruck_;}
  bool roomObjectVisible(unsigned work) const;
  // Literal SetNoSuspend ownership from the r100 scene tasks; this also
  // selects which persistent scroll models survive ModelTrans's event gate.
  void beginSourceEvent(unsigned phase,unsigned cameraCut=~0u);
  void endSourceEvent(unsigned phase);
  bool actorVisibleDuringSuspend(const GenericEventActor &actor) const;
  void tickSuspended30Hz();
  std::uint32_t effectDisplayMask() const;
  void prepareMessages(const std::string &discFiles,unsigned language=1);
  const SubscreenFont &messageFont() const {return messageFont_;}
  // Source regular MesSet. English room messages remain enabled; only
  // Event::MesSet's movie captions use the language1 suppression.
  R100RoomMessage roomMessage(unsigned number,unsigned attributes=0x52,
      unsigned x=100,unsigned y=0) const;
  // One original world tick. dontFire is objWep's real laser prohibition
  // (not an invented radius around the car); house/D areas use the AEV.
  void tick30Hz(RoomSaveState &state,const CollisionVec &feet,double heading,
      bool actionPressed,bool dontFire,bool eventActive,
      const std::vector<R100Threat> &ambush={});
  std::vector<R100RoomSound> takeSounds();
  bool takeOfficerMessage(); // original R100Main message0x33
  unsigned officerTalkRemaining() const {return talkRemaining_;}
private:
  const R100Scenario scenario_;
  std::vector<std::unique_ptr<GenericEventActor>> actors_;
  std::array<GenericEventActor*,2> cops_{};
  GenericEventActor *car_{},*carSub_{};
  EnemySpawn houseSpawn_;
  AnimationBytes playerMotion_;
  AnimationBytes radioIntroMotion_;
  std::unique_ptr<GenericEventActor> radioActor_;
  unsigned radioIntroTicks_{};
  std::vector<std::uint8_t> messageTable_;
  SubscreenFont messageFont_;
  unsigned messageLanguage_=1;
  std::optional<unsigned> playerMotionIndex_;
  std::optional<unsigned> radioIntroMotionIndex_;
  const LeonPreview *playerMotionOwner_{};
  std::map<unsigned,std::unique_ptr<R100RoomCamera>> cameras_;
  R100RoomCamera *activeCamera_{};
  EventCamera corpseCamera_;
  bool corpseCameraActive_{};
  bool corpseNoSuspend_{};
  std::vector<R100RoomSound> sounds_;
  unsigned country_{},talkRemaining_{},villageTimer_{},streamCooldown_{};
  bool afterTruck_{},officerMessage_{},streamPlaying_{},streamArmed_=true,streamHold_{},wasFirstLook_{};
  bool skullDownInside_{},skullUpInside_{};
  std::array<bool,4> noSuspend_{}; // car, carSub, cop0, cop1
  std::uint16_t randomState_=0x1000;
  unsigned random();
  void startCop(unsigned cop,unsigned clip,unsigned blend);
};
}
