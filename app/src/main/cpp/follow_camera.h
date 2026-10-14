#pragma once
#include "collision_query.h"
#include "model_skin.h"
namespace re4 {
struct ActionCameraPose {
  CollisionVec eye{}, target{};
  double roll{}, fovDegrees{50};
};
enum class ObjectLadderCamera { Climb, Tip, Reset };
// obj13.cpp: the climb camera is in animated part0 space; tipping/reset
// cameras are in actor space. Caller supplies the corresponding world matrix.
ActionCameraPose PoseObjectLadderCamera(const Matrix34 &worldMatrix,
                                        ObjectLadderCamera action);
// A swept camera sphere against walls, slopes AND floor, using SAT_TYPE_SEE.
// Returns an offset from the follow target; owns no frame or GL state.
CollisionVec FollowCameraOffset(const SatCollision &world,
                                const CollisionVec &target,
                                const CollisionVec &desired,
                                double radius = 120);
// Horizontal shoulder offset keeps the eye high even after a wall sweep.
CollisionVec ShoulderCameraOffset(const SatCollision *world,
                                  const CollisionVec &feet, double height,
                                  double yaw, double distance);

// CameraQuasiFPS::calcDepressionRatio, stepped at the game's 30 Hz. These are
// player-local look offsets, not an independently integrated orbit camera.
// Plain values can be copied with the room/view checkpoint, including a
// partially elapsed tick. InputMapper supplies the corrected RS axes.
struct FollowCameraState {
  double yawRatioRadians{};
  double verticalRatio{};
  double accumulator{};
  // One-shot visual carry when an interrupted draw or holster returns to
  // footwork before its residual waist/camera yaw has reached the body. This
  // replaces the missing full CameraSmooth transition in this bounded adapter.
  double releaseYawRadians{};
  // calcDepressionRatio's target and CameraSmooth's visible result are
  // separate. 0.8 retention per original 30-Hz frame is applied continuously
  // to these player-relative offsets, preserving shoulder composition and
  // querying wall clearance after smoothing rather than lagging through it.
  double desiredYaw{}, desiredVertical{}, desiredReleaseYaw{};
  bool smoothingInitialized{};
};
struct FollowCameraPose {
  CollisionVec eye{}, target{}, look{}, right{}, up{};
  double fovDegrees{50};
  double distance{}; // Horizontal eye-to-Leon distance; diagnostics only.
};
void AdvanceFollowCamera(FollowCameraState &state, double mappedRightX,
                         double mappedRightY, double seconds);
// Uses the exact Leon transition up/mid/down table from cam_qfps.cpp for every
// supported action, as requested for consistent movement/weapon framing.
// During aiming the caller provides the bounded player waist yaw and weapon
// pitch, never the animated muzzle orientation. During draw the caller may
// pass the original remaining camera yaw as aimYaw to preserve the handover.
// Static SAT uses the original three near-frustum probes and camera mask;
// the caller's viewport aspect must match its projection (default GameCube).
FollowCameraPose PoseFollowCamera(const FollowCameraState &state,
                                  const SatCollision *world,
                                  const CollisionVec &feet,
                                  double actorHeading, bool aiming = false,
                                  double aimYaw = 0, double aimRate = 0,
                                  double aspect = 4. / 3);
} // namespace re4
