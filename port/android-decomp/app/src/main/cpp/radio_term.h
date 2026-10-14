#pragma once
#include "generic_event_actor.h"
#include "r100_secondary.h"
#include "sound_music.h"
namespace re4 {
struct RadioTermAudio {
  enum Kind { Sample, StreamRequest, StreamStart, StreamStop } kind=Sample;
  unsigned block{},number{};
};
struct RadioTermSequence { unsigned flags{};int number{},frame{},message{},timer{}; };
// Immutable source ss_term/ss_oc101/102/op01 and original DSP call streams.
// No Motion64 state is constructed by the asynchronous room loader.
class RadioTermData {
public:
  RadioTermData(const std::string &discFiles,unsigned callNo,unsigned language=1);
  ~RadioTermData();
  unsigned callNo() const;
  unsigned language() const;
  const SubscreenFont &font() const;
  std::shared_ptr<const SoundStream> voice() const;
  const std::vector<RadioTermSequence> &sequence() const;
private:
  struct Impl;std::shared_ptr<const Impl> impl_;
  friend class RadioTermRuntime;
};
class RadioTermRuntime {
public:
  explicit RadioTermRuntime(std::shared_ptr<const RadioTermData> data);
  ~RadioTermRuntime();
  void tick30Hz(bool confirm=false,bool skipDialogue=false,bool close=false,
               bool advance=true,bool streamReady=true);
  bool finished() const;
  bool manual() const;
  unsigned frame() const;
  unsigned sequenceFrame() const;
  unsigned fadeAlpha() const;
  const RadioTermData &data() const;
  const R100RoomMessage &message() const;
  const std::vector<std::unique_ptr<GenericEventActor>> &actors() const;
  const std::vector<SubscreenQuad> &quads() const;
  const std::vector<SubscreenTexture> &textures() const;
  unsigned textureFrame(unsigned group,unsigned unit,bool mask=false) const;
  std::array<unsigned,2> textureEffect(unsigned group,unsigned unit) const;
  std::shared_ptr<const RoomLighting> lighting() const;
  std::vector<RadioTermAudio> takeAudio();
private:
  struct Impl;std::unique_ptr<Impl> impl_;
};
} // namespace re4
