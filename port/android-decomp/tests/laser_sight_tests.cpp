#include "laser_sight.h"
#include "leon_preview.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
int main() {
  re4::PlayerShot marker;
  marker.from = {10, 20, 30}; marker.to = {10,20,50030};
  auto sight = re4::ResolveLaserSight(marker,nullptr,nullptr,nullptr,nullptr,19000);
  assert(sight.line && !sight.point && sight.target == marker.to);
  assert(sight.lineStart == re4::CollisionVec({10,20,530}));
  assert(sight.lineEnd == re4::CollisionVec({10,20,19530}));
  marker.to = {10,20,529};
  sight = re4::ResolveLaserSight(marker,nullptr,nullptr,nullptr,nullptr,19000);
  assert(!sight.line && !sight.point);
  marker.to = marker.from;
  sight = re4::ResolveLaserSight(marker,nullptr,nullptr,nullptr,nullptr,19000);
  assert(!sight.line && !sight.point);
  marker.to = {310,420,30}; // Exactly 500: no negative or zero-length beam.
  sight = re4::ResolveLaserSight(marker,nullptr,nullptr,nullptr,nullptr,19000);
  assert(!sight.line && !sight.point);
  bool rejected = false;
  try { re4::ResolveLaserSight(marker,nullptr,nullptr,nullptr,nullptr,0); }
  catch (const std::runtime_error &) { rejected = true; }
  assert(rejected);
  marker.from[0] = std::numeric_limits<double>::quiet_NaN(); rejected = false;
  try { re4::ResolveLaserSight(marker,nullptr,nullptr,nullptr,nullptr,19000); }
  catch (const std::runtime_error &) { rejected = true; }
  assert(rejected);
  std::cout << "Laser geometry contracts passed\n";
}
