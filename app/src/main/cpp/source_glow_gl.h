#pragma once
#include "world_effects.h"
namespace re4 {
// Filter03SetParam/Trans: preserve request order and the source locked priority.
void BeginSourceGlowFrameGl(const std::vector<WorldEffectGlowRequest> &requests);
bool SourceEffectWritesAlphaGl(unsigned flags);
void RestoreSourceGlowAlphaDraw2Gl(bool enabled);
void DrawSourceGlowGl(int width,int height);
void DestroySourceGlowGl();
void ForgetSourceGlowGl();
}
