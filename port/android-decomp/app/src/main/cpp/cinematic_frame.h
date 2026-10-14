#pragma once
namespace re4 {
// game/cinesco.cpp::Draw_cinesco hides rows0..56 and393..449 of
// the authored512x448 frame. Present only that visible picture on Android,
// preserving the source lens and stretching the result to the display.
inline constexpr float OriginalCinematicFrameHeight=448;
inline constexpr float OriginalCinematicFrameTop=56;
inline constexpr float OriginalCinematicFrameBottom=393;
inline constexpr float OriginalCinematicPictureHeight=
    OriginalCinematicFrameBottom-OriginalCinematicFrameTop;
inline void CropOriginalCinematicFrame(float *columnMatrix) {
  constexpr float scale=OriginalCinematicFrameHeight/OriginalCinematicPictureHeight;
  constexpr float offset=(OriginalCinematicFrameTop+OriginalCinematicFrameBottom-
      OriginalCinematicFrameHeight)/OriginalCinematicPictureHeight;
  for(unsigned col=0;col<4;++col)
    columnMatrix[col*4+1]=columnMatrix[col*4+1]*scale+columnMatrix[col*4+3]*offset;
}
} // namespace re4
