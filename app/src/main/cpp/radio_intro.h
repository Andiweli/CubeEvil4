#pragma once
#include "generic_event_actor.h"
#include "leon_preview.h"
namespace re4 {
// Source OpeSetOpenTerm's PL00 ARC79 motion and ARC77/78 radio prop.
class RadioIntro {
public:
  explicit RadioIntro(const std::string &playerDrs);
  void install(LeonPreview &player);
  void begin(LeonPreview &player,WalkProbe &probe);
  void tick30Hz(LeonPreview &player,WalkProbe &probe);
  bool finished(const LeonPreview &player) const;
  void end(LeonPreview &player);
  bool visible() const {return ticks_>=22;}
  const GenericEventActor &actor() const {return *actor_;}
  Matrix34 matrix(const LeonPreview &player) const;
private:
  std::unique_ptr<GenericEventActor> actor_;
  AnimationBytes motion_;
  LeonPreview *owner_{};
  unsigned action_{},ticks_{};
};
}
