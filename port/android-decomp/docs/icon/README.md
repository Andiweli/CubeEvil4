# App icon and TV banner of CubeEvil4

Own drawing (18.10.2026), not Capcom artwork: a wire-frame cube with wings, side hooks, a
point and a long blade in the style of a cult emblem, on a red, slightly grainy ground. It
replaced the Los Illuminados emblem of the game (`icon_illuminati.jpg`), together with the
app's new name (decision of the project owner, 18.10.2026).

- `make_icon.py OUT.svg` writes the icon as SVG (with `common.inc`: background, grain,
  rough edges); `cubeevil4.svg` is its output.
- `render.sh IN.svg OUT.png 512` renders it with the headless Chromium of the cloud
  container; the app uses the 512 x 512 PNG
  (`app/src/main/res/drawable-nodpi/icon_cubeevil4.png`, inset by `ic_launcher.xml`).
- `tv_banner.html`, rendered at 640 x 360 and scaled to 320 x 180, is the Android TV banner
  (`drawable-nodpi/tv_banner.png`).

Everything that matters stays inside the circle a round launcher mask keeps.
