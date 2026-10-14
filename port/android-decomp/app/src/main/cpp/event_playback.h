#pragma once
#include "generic_event_actor.h"
#include "subscreen_data.h"
#include "sound_music.h"
#include "world_effects.h"
#include "destruction_effects.h"
#include <optional>
namespace re4 {
struct EventCamera {
  std::array<float,3> eye{},target{};
  float roll{},fovRadians{};
  bool valid{};
};
struct EventAudioCue {
  bool stream{};
  unsigned block{},number{},argument{},sourceFrame{};
  bool synchronized{};
  std::string eventName;
};
struct EventFocusState {
  bool nearEnabled{},farEnabled{};
  float nearDepth{},farDepth{},nearLevel{},farLevel{};
};
struct EventSubtitle {
  int message=-1;
  unsigned remainingFrames{};
  std::string tableAsset;
  std::vector<std::uint16_t> words;
  std::vector<SubscreenGlyph> glyphs;
};
// Original EVD packets, models and FCVs. Prepare on the room loader worker;
// ticks perform no filesystem reads and use only the source 30-Hz clock.
class OriginalEvent {
 public:
  OriginalEvent(const std::string &discFiles,const std::string &eventName,
      unsigned language=1);
  ~OriginalEvent();
  OriginalEvent(const OriginalEvent &)=delete;
  OriginalEvent &operator=(const OriginalEvent &)=delete;
  void tick30Hz(bool advance=true);
  void reset();
  void finish();
  bool finished() const {return finished_;}
  unsigned sourceFrame() const {return totalFrame_;}
  unsigned sourceCut() const {return cut_;}
  unsigned cutFrame() const {return frame_;}
  unsigned totalFrames() const {return data_.totalFrames();}
  const EventCamera &camera() const {return camera_;}
  unsigned fadeAlpha() const;
  bool drawsRoomBackground() const;
  std::vector<unsigned> hiddenRoomObjects() const;
  const EventFocusState &focus() const {return focus_;}
  void prepareRoomEffectModels(const Smd &room,const Smd *common=nullptr);
  const std::map<unsigned,StaticScene> &effectModels() const {return effectModels_;}
  const std::vector<std::unique_ptr<GenericEventActor>> &actors() const {return actors_;}
  const EventSubtitle &subtitle() const {return subtitle_;}
  const SubscreenFont &font() const {return font_;}
  std::vector<EventAudioCue> takeAudioCues();
  std::shared_ptr<const SoundStream> audioStream(const EventAudioCue &cue) const;
  std::shared_ptr<const RoomLighting> lighting() const {if(posedLighting_)return posedLighting_;return lighting_;}
  const WorldEffectsData &effectsData() const {return effectsData_;}
  const WorldEffectsState &effectsState() const {return effectsState_;}
  const DestructionEffectsData &fragmentData() const {return fragmentData_;}
  std::vector<DestructionInstance> fragments() const {return fragments_?fragments_->instances():std::vector<DestructionInstance>{};}
  // EventMgr::GetZeroPartsWorldPos before its room-SAT floor ray. r100s03
  // then overrides this with its scenario placement; s20 forbids replacing PL.
  std::optional<SmdPlacement> endPlayerPlacement() const;
  const EventData &data() const {return data_;}
  const std::vector<std::string> &unsupported() const {return unsupported_;}
  std::string describe() const;
 private:
  void execute(const EventPacket &packet);
  void prepareActors(const std::string &discFiles,const RoomArchive &room);
  void evaluate();
  void preflight();
  void tickEffects();
  void roomCallback();
  void initCarActors();
  void setFade(bool fadeIn,unsigned frames);
  void sampleFocus();
  void updateRoomLights();
  GenericEventActor &actor(const std::string &name);
  EventData data_;
  std::map<std::string,std::vector<std::uint8_t>> external_;
  std::vector<std::unique_ptr<GenericEventActor>> actors_;
  std::map<std::string,GenericEventActor*> actorNames_;
  std::map<std::string,std::shared_ptr<const EventCurve>> curves_;
  std::map<std::string,std::shared_ptr<const RoomLighting>> lights_;
  std::shared_ptr<const EventCurve> cameraCurve_;
  std::shared_ptr<const RoomLighting> lighting_;
  std::shared_ptr<RoomLighting> posedLighting_;
  Matrix34 cameraBase_{1,0,0,0,0,1,0,0,0,0,1,0};
  EventCamera camera_;
  EventFocusState focus_;
  std::map<std::string,std::vector<std::uint8_t>> focusAssets_;
  const std::vector<std::uint8_t> *focusData_{};
  unsigned fadeStart_{},fadeFrames_{};
  bool fadeIn_{},fadeActive_{};
  unsigned callbackCut_=~0u,callbackFrame_=~0u;
  EventSubtitle subtitle_;
  SubscreenFont font_;
  std::vector<EventAudioCue> audio_;
  std::map<std::pair<unsigned,unsigned>,std::shared_ptr<const SoundStream>> streams_;
  std::vector<std::string> unsupported_;
  std::string messageTable_;
  std::vector<GenericEventActor*> effectActors_; // Original SetOm index order.
  std::map<unsigned,unsigned> packetEffectTargets_;
  std::vector<unsigned> pendingEffectTargets_;
  DestructionEffectsData fragmentData_;
  std::unique_ptr<DestructionEffectsSimulation> fragments_;
  std::unique_ptr<Sat> effectSat_;
  std::unique_ptr<SatCollision> effectFloor_;
  WorldEffectsData effectsData_;
  std::map<unsigned,StaticScene> effectModels_;
  EnvironmentData effectSimulationData_;
  std::unique_ptr<EnvironmentSimulation> effectSimulation_;
  WorldEffectsState effectsState_;
  bool effectsEnabled_{},effectsCutPending_{};
  unsigned language_{},cut_{},frame_{},totalFrame_{},cameraStart_{};
  std::size_t nextPacket_{};
  bool finished_{};
};
}
