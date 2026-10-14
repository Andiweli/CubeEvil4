// Actual SceneGl shader/depth/texture path with the original CORE target image.
#define main previous_camera_test_main
#include "test_follow_camera_gl.cpp"
#undef main
#include "archive.h"
#include "laser_sight.h"
#include "room_lighting.h"
int main(int argc, char **argv) {
  try {
    if (argc != 2 && argc != 3) throw std::runtime_error("Usage: laser_gl_check CORE.das [R101.dat]");
    Egl egl;
    const auto core = re4::RoomArchive::LoadDrs(argv[1]);
    const auto *eff = core.find("EFF");
    check(eff != nullptr, "CORE effect exists");
    const auto data = std::make_shared<const re4::LaserSightData>(
        re4::BuildLaserSightData(core.copy(eff - core.entries().data())));
    auto scene = scenery(fixture(5000, 0));
    if (argc == 3) scene.lighting = re4::ReadRoomLighting(re4::RoomArchive::Load(argv[2]));
    scene.laserSight = data;
    re4::CreateSceneGl(scene);
    re4::SceneFollowHeading(0);
    re4::SceneSeedFollowView(0, 0);
    re4::SceneProbe probe;
    probe.showBody = false;
    probe.aimPoint = {0, 1200, 1000};
    re4::DrawSceneGl(width, height, 0, 0, 0, false, &probe, nullptr, 0);
    const auto background = pixels();
    probe.showAimPoint = true;
    re4::DrawSceneGl(width, height, 0, 0, 0, false, &probe, nullptr, 0);
    const auto dot = pixels();
    check(difference(background, dot) > 3, "Original red target texture is visible");
    unsigned redPixels=0;
    for(std::size_t i=0;i<dot.size();i+=4)
      if(!std::equal(dot.begin()+i,dot.begin()+i+3,background.begin()+i) &&
          dot[i]>2*unsigned(dot[i+1]) && dot[i]>2*unsigned(dot[i+2])) ++redPixels;
    check(redPixels>3,"Original target pixels remain red under actual room light/fog");
    re4::DrawSceneGl(width, height, 0, 0, 0, false, &probe, nullptr, 0);
    check(pixels() == dot, "Target dot does not flicker during pause");
    GLboolean writes = GL_FALSE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &writes);
    check(writes && glIsEnabled(GL_DEPTH_TEST) && !glIsEnabled(GL_POLYGON_OFFSET_FILL),
          "Target restores depth writes and polygon state");
    re4::DestroySceneGl();
    scene = scenery(fixture(-700, 0));
    if (argc == 3) scene.lighting = re4::ReadRoomLighting(re4::RoomArchive::Load(argv[2]));
    scene.laserSight = data;
    re4::CreateSceneGl(scene);
    re4::SceneFollowHeading(0);
    re4::SceneSeedFollowView(0, 0);
    probe.showAimPoint = false;
    re4::DrawSceneGl(width, height, 0, 0, 0, false, &probe, nullptr, 0);
    const auto wall = pixels();
    probe.showAimPoint = true;
    re4::DrawSceneGl(width, height, 0, 0, 0, false, &probe, nullptr, 0);
    check(pixels() == wall, "Target dot does not show through an opaque wall");
    re4::DestroySceneGl();
    std::cout << "M28 production laser GLES checks passed: " << checks << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n'; return 1;
  }
}
