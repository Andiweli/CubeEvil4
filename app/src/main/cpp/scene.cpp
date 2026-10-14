#include "scene.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <tuple>
namespace re4 {
Matrix3 NormalMatrix(const Matrix34 &m) {
  const double a = m[0], b = m[1], c = m[2], d = m[4], e = m[5], f = m[6],
               g = m[8], h = m[9], i = m[10];
  const std::array<double, 9> cofactors{
      e * i - f * h, f * g - d * i, d * h - e * g, c * h - b * i, a * i - c * g,
      b * g - a * h, b * f - c * e, c * d - a * f, a * e - b * d};
  const double det = a * cofactors[0] + b * cofactors[1] + c * cofactors[2];
  if (!std::isfinite(det))
    throw std::runtime_error("Scene: invalid normal matrix");
  if (det == 0)
    return {};
  Matrix3 result{};
  for (unsigned k = 0; k < 9; ++k) {
    result[k] = static_cast<float>(cofactors[k] / det);
    if (!std::isfinite(result[k]))
      throw std::runtime_error("Scene: Normal matrix overflow");
  }
  return result;
}
std::array<float, 3> TransformNormal(const Matrix3 &m,
                                     const std::array<float, 3> &n) {
  std::array<float, 3> out{};
  for (unsigned row = 0; row < 3; ++row) {
    out[row] =
        m[row * 3] * n[0] + m[row * 3 + 1] * n[1] + m[row * 3 + 2] * n[2];
    if (!std::isfinite(out[row]))
      throw std::runtime_error("Scene: Normal value overflow");
  }
  return out;
}
Matrix34 PlacementMatrix(const SmdPlacement &p) {
  float sx = std::sin(p.rotation[0]), cx = std::cos(p.rotation[0]);
  float sy = std::sin(p.rotation[1]), cy = std::cos(p.rotation[1]);
  float sz = std::sin(p.rotation[2]), cz = std::cos(p.rotation[2]);
  Matrix34 m{cz * cy,
             cz * sx * sy - sz * cx,
             cz * cx * sy + sz * sx,
             p.position[0],
             sz * cy,
             sz * sx * sy + cz * cx,
             sz * cx * sy - cz * sx,
             p.position[1],
             -sy,
             cy * sx,
             cy * cx,
             p.position[2]};
  for (unsigned row = 0; row < 3; ++row)
    for (unsigned col = 0; col < 3; ++col)
      m[row * 4 + col] *= p.scale[col];
  for (auto f : m)
    if (!std::isfinite(f))
      throw std::runtime_error("Scene: invalid matrix");
  return m;
}
std::array<float, 3> TransformPoint(const Matrix34 &m,
                                    const std::array<float, 3> &p) {
  std::array<float, 3> out{};
  for (unsigned row = 0; row < 3; ++row) {
    out[row] = m[row * 4] * p[0] + m[row * 4 + 1] * p[1] +
               m[row * 4 + 2] * p[2] + m[row * 4 + 3];
    if (!std::isfinite(out[row]))
      throw std::runtime_error("Scene: World coordinate overflow");
  }
  return out;
}
std::vector<std::array<float, 3>> RestJointCenters(const SmdModel &m) {
  std::vector<std::array<float, 3>> out;
  for (std::size_t i = 0; i < m.joints.size(); ++i) {
    const auto &joint = m.joints[i];
    auto center = joint.center;
    if (joint.parent != 0xff) {
      if (joint.parent >= i)
        throw std::runtime_error("Scene: Parent follows child or cycle");
      for (unsigned k = 0; k < 3; ++k)
        center[k] += out[joint.parent][k];
    }
    for (auto f : center)
      if (!std::isfinite(f))
        throw std::runtime_error("Scene: Joint coordinate overflow");
    out.push_back(center);
  }
  return out;
}
StaticScene BuildStaticScene(const Smd &s, int placement, const Smd *common) {
  StaticScene scene;
  for (std::size_t i = 0; i < s.placements.size(); ++i) {
    if (placement >= 0 && std::size_t(placement) != i)
      continue;
    const auto &w = s.placements[i];
    if (w.work == 0xff)
      continue;
    if (w.motion != 0xff) {
      ++scene.skipped;
      continue;
    }
    const bool commonModel = (w.flags & 0x10) != 0;
    if (commonModel && !common) {
      ++scene.missingCommonPlacements;
      ++scene.skipped;
      continue;
    }
    const auto &source = commonModel ? *common : s;
    auto id = w.bin == 0xff ? 0u : unsigned(w.bin);
    if (id >= source.models.size())
      throw std::runtime_error("Scene: BIN reference is missing");
    const auto &m = source.models[id];
    // commonModelTrans's original-array rigid branch; no guessed skinning.
    if (m.parts != 1 || m.weightPalette > 1 || m.weightExt > 0xff ||
        m.joints.size() != 1 || m.joints[0].attachment != 0 ||
        m.joints[0].parent != 0xff) {
      ++scene.skipped;
      continue;
    }
    auto centers = RestJointCenters(m);
    auto matrix = PlacementMatrix(w);
    auto normalMatrix = NormalMatrix(matrix);
    bool singularNormal = std::all_of(normalMatrix.begin(), normalMatrix.end(),
                                      [](float f) { return f == 0; });
    std::size_t triangles = 0;
    for (const auto &p : m.primitives)
      triangles += p.triangles.size();
    if (triangles > 1000000 - scene.triangles)
      throw std::runtime_error("Scene: Triangle budget");
    if (!triangles) {
      ++scene.skipped;
      continue;
    }
    if (singularNormal)
      ++scene.singularNormalPlacements;
    const auto lightCenter = TransformPoint(matrix, centers[0]);
    RoomLightVolume lightVolume;
    std::array<float,3> boxCenter{};
    for(unsigned axis=0;axis<3;++axis)boxCenter[axis]=(m.minimum[axis]+m.maximum[axis])*.5f+centers[0][axis];
    lightVolume.center=TransformPoint(matrix,boxCenter);
    for(unsigned axis=0;axis<3;++axis) {
      float scale=0;for(unsigned k=0;k<3;++k)scale+=matrix[k*4+axis]*matrix[k*4+axis];
      scale=std::sqrt(scale);
      lightVolume.size[axis]=(m.maximum[axis]-m.minimum[axis])*.5f*scale;
      for(unsigned k=0;k<3;++k)lightVolume.axes[axis*3+k]=scale?matrix[k*4+axis]/scale:0;
    }
    float lightSize = 0;
    for (const auto &point : m.positions) {
      auto local = point;
      for (unsigned k = 0; k < 3; ++k) local[k] += centers[0][k];
      const auto world = TransformPoint(matrix, local);
      double squared = 0;
      for (unsigned k = 0; k < 3; ++k)
        squared += double(world[k] - lightCenter[k]) * (world[k] - lightCenter[k]);
      lightSize = std::max(lightSize, float(std::sqrt(squared)));
    }
    for (const auto &primitive : m.primitives) {
      if (primitive.triangles.empty())
        continue;
      const auto &header = m.materialHeaders.empty()
                               ? std::array<std::uint8_t, 32>{}
                               : m.materialHeaders.at(primitive.displayList);
      SceneDraw draw;
      draw.first = scene.vertices.size() / 3;
      draw.authored = true;
      draw.cullMode = 0;
      draw.object = static_cast<unsigned>(i);
      draw.sortCenter = lightCenter;
      draw.lightSize = lightSize;
      draw.lightVolume=lightVolume;
      draw.scrollLightAttribute=(w.flags&1)!=0;
      draw.work = w.work;
      draw.count = primitive.triangles.size() * 3;
      draw.commonModel = commonModel;
      draw.palette = commonModel ? 0 : (w.tpl == 0xff ? 0 : w.tpl);
      draw.image = header[12];
      SetSceneSpecular(draw,header,m.sourceFlags);
      // commonModelTrans removes cModel::scale on the LEFT before texgen:
      // (D^-1 M)^-T = D M^-T for the authored diagonal scale D.
      draw.specularNormalScale = w.scale;
      draw.repeat = m.colored;
      draw.vertexColors = m.colored && m.vertexAmbient;
      if (draw.vertexColors)
        scene.vertexColorTriangles += primitive.triangles.size();
      draw.alphaTest = (header[11] & 4) != 0;
      draw.alphaImage = header[14];
      draw.alphaReference = header[20];
      scene.draws.push_back(draw);
      for (const auto &triangle : primitive.triangles)
        for (auto corner : triangle) {
          const auto &reference = primitive.vertices.at(corner);
          const auto normal =
              TransformNormal(normalMatrix, m.normals.at(reference.normal));
          scene.normals.insert(scene.normals.end(), normal.begin(),
                               normal.end());
          if (normal[0] == 0 && normal[1] == 0 && normal[2] == 0)
            ++scene.zeroNormals;
          const auto color =
              draw.vertexColors
                  ? m.colors.at(reference.color)
                  : std::array<std::uint8_t, 4>{255, 255, 255, 255};
          scene.colors.insert(scene.colors.end(), color.begin(), color.end());
          auto uv = m.texcoords.empty() ? std::array<float, 2>{}
                                        : m.texcoords.at(reference.texcoord);
          scene.uv.insert(scene.uv.end(), uv.begin(), uv.end());
          auto id = reference.position;
          auto point = m.positions.at(id);
          for (unsigned k = 0; k < 3; ++k)
            point[k] += centers[0][k];
          auto world = TransformPoint(matrix, point);
          for (unsigned k = 0; k < 3; ++k) {
            if (scene.vertices.empty())
              scene.minimum = scene.maximum = world;
            scene.minimum[k] = std::min(scene.minimum[k], world[k]);
            scene.maximum[k] = std::max(scene.maximum[k], world[k]);
            scene.vertices.push_back(world[k]);
          }
        }
    }
    scene.triangles += triangles;
    ++scene.included;
    if (commonModel)
      ++scene.commonPlacements;
  }
  return scene;
}
void ApplySceneSmx(const Smx &smx, StaticScene &scene) {
  scene.smxDraws = scene.materialColorTriangles = 0;
  for (auto &draw : scene.draws) {
    draw.materialRGB = {255, 255, 255};
    draw.blendMode = 0;
    draw.orderType = 3;
    draw.uvSpeed = {};
    if (const auto *record = smx.find(draw.work)) {
      ++scene.smxDraws;
      draw.materialRGB = record->materialRGB();
      // scroll.cpp::smxSetObj uses nonzero RGB as cModelInfo::color2;
      // otherwise specularSetup retains the BIN material's own colour.
      if(record->specular&0xffffff00u)
        draw.specularRGB={std::uint8_t(record->specular>>24),std::uint8_t(record->specular>>16),std::uint8_t(record->specular>>8)};
      draw.blendMode = record->material & 255;
      draw.orderType = record->order;
      draw.cullMode = record->cull;
      draw.uvSpeed = record->uvSpeed;
      draw.lightMask = record->lightMask;
      draw.noDiffuseLight = (record->flags & 16) != 0;
      draw.scrollLightAttribute=draw.scrollLightAttribute || (record->flags&32)!=0;
      // Three shipped prototype records contain mode 5; the original bl[]
      // has only five entries. Retain that value for diagnostics, use an
      // explicit standard-preview fallback in GLES, never read past bl[].
      if (draw.blendMode > 5 || draw.orderType > 7 || draw.cullMode > 2)
        throw std::runtime_error("SMX: unsupported render state");
      if (draw.alphaTest && (record->flags & 8))
        draw.alphaReference = 128;
      if (draw.materialRGB != std::array<std::uint8_t, 3>{255, 255, 255})
        scene.materialColorTriangles += draw.count / 3;
    }
  }
}
std::vector<std::size_t> SceneDrawOrder(const std::vector<SceneDraw> &draws,
                                        const std::array<double, 3> &eye,
                                        const std::array<double, 3> &look) {
  using Key = std::tuple<unsigned, int, int, std::size_t>;
  std::vector<Key> keys;
  keys.reserve(draws.size());
  for (std::size_t i = 0; i < draws.size(); ++i) {
    const auto &d = draws[i];
    unsigned pass = 0, bucket = 0;
    if (d.authored) {
      switch (d.orderType) {
      case 0:
        pass = 13;
        break;
      case 1:
        pass = 17;
        break;
      case 2:
        pass = 11;
        bucket = 4;
        break;
      case 3:
        pass = 11;
        bucket = 2;
        break;
      case 4:
        pass = 11;
        bucket = 1;
        break;
      case 5:
        pass = 16;
        break;
      case 6:
        pass = 20;
        bucket = 1;
        break;
      case 7:
        pass = 11;
        bucket = 1;
        break; // Material split needs runtime info.
      default:
        throw std::runtime_error("OT: Unknown model type");
      }
      if (d.orderType <= 1) {
        double z = 0;
        for (unsigned k = 0; k < 3; ++k)
          z += (d.sortCenter[k] - eye[k]) * look[k];
        unsigned limit = d.orderType == 0 ? 127 : 1023;
        bucket = unsigned(
            std::clamp(z * (d.orderType == 0 ? .01 : .05), 0., double(limit)));
      }
    }
    keys.emplace_back(pass, -int(bucket), d.authored ? -int(d.object) : 0, i);
  }
  std::vector<std::size_t> order(draws.size());
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(),
                   [&](auto a, auto b) { return keys[a] < keys[b]; });
  return order;
}
std::string StaticScene::describe() const {
  std::ostringstream s;
  s << "Rigid bind pose: " << included << " placements · " << skipped
    << " skipped · " << triangles << " triangles\n";
  if (!vertices.empty())
    s << "World bounds: (" << minimum[0] << ", " << minimum[1] << ", "
      << minimum[2] << ") to (" << maximum[0] << ", " << maximum[1] << ", "
      << maximum[2] << ")\n";
  s << "Common-BIN: " << commonPlacements << " placements · "
    << missingCommonPlacements << " without a resource source\n";
  s << "Rigid geometry preview; no animation, runtime visibility or "
       "game simulation.\n";
  return s.str();
}
} // namespace re4
