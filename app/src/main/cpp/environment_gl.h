#pragma once
#include "environment.h"
namespace re4 {
void CreateEnvironmentGl(const EnvironmentData &data);
void DestroyEnvironmentGl();
void ForgetEnvironmentGl();
void DrawEnvironmentTargets(double frames);
void DrawEnvironmentGridWater(const float *columnCamera,
                             const std::array<float,3> &worldCenter,
                             float worldScale,
                             const std::array<float,3> &cameraTarget,
                             int width,int height);
unsigned EnvironmentTexture(unsigned target);
EnvironmentMatrix EnvironmentSkyMatrix(unsigned sky);
std::shared_ptr<const EnvironmentState> SaveEnvironmentState();
void RestoreEnvironmentState(
    const std::shared_ptr<const EnvironmentState> &state);
// Copies the completed room image through the original GX copy filter and
// half-size resolve, before refraction draws. A source Esp18 pass can force
// the same copy even when this room has no water material.
// Keeps caller framebuffers/program/VAO/render state; binds the result on
// texture unit2 and returns to texture unit0 for the material-refraction pass.
unsigned CaptureEnvironmentScreen(int width, int height, bool force = false);
} // namespace re4
