#!/usr/bin/env python3
"""Original-data pixel QA of every menu frame/header, using Java render plans.

export_menu_art first decodes a supplied Disc into a temporary directory.
Compile GameMenuArtRenderPlan with GameMenuLayout/GameMenuNavigation, then:
  python3 tools/check_menu_art.py assets class-directory --java /path/to/java

Optional --output writes host PNGs; these are not Android/device screenshots.
The check uses the actual decoded ID units, textures, colors, additive blend,
parent visibility, source counts and the production Java nine-slice geometry.
"""
import argparse
import json
from pathlib import Path
import subprocess
from PIL import Image, ImageChops, ImageDraw
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets', type=Path)
    parser.add_argument('classes', type=Path)
    parser.add_argument('--java', default='java')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    checks = 0

    def check(value, message):
        nonlocal checks
        checks += 1
        if not value:
            raise AssertionError(message)

    packs = [json.loads((args.assets / f'{n}.json').read_text()) for n in range(5)]
    images = {}
    for kind, pack in enumerate(packs):
        for texture in pack['textures']:
            key = kind, texture['id'], texture['frame']
            images[key] = Image.frombytes('RGBA', (texture['width'], texture['height']),
                                         (args.assets / f'{kind}_{key[1]}_{key[2]}.rgba').read_bytes())
    common = packs[0]['quads']
    frame = next(q for q in common if q['group'] == 12 and q['mark'] == 255)
    check(frame['texture'] == 102 and images[0, 102, 0].size == (640, 360), 'Original common0xC frame')
    check(images[0, 66, 0].size == (511, 291), 'Original backing plate')
    for digit in range(10):
        check((0, 7, digit) in images, f'Original count digit{digit}')
    for kind in range(5):
        check(len(packs[kind]['quads']) > 0, f'Archive layout{kind} decoded')
    file_select = next(q for q in packs[3]['quads'] if q['group'] == 7 and q['mark'] == 1 and q['kind'] == 0)
    check(file_select['texture'] == 25 and file_select['blend'] == 1, 'Original list additive selection')

    def composite(canvas, texture, box, rgba, additive=False):
        x0, y0, x1, y1 = map(round, box)
        if x1 <= x0 or y1 <= y0:
            return
        im = texture.resize((x1-x0, y1-y0), Image.Resampling.BILINEAR)
        channels = im.split()
        im = Image.merge('RGBA', tuple(channels[n].point(lambda v, m=rgba[n]: int(v*m/255)) for n in range(4)))
        if additive:
            clip = (max(0, x0), max(0, y0), min(canvas.width, x1), min(canvas.height, y1))
            if clip[2] <= clip[0] or clip[3] <= clip[1]:
                return
            patch = im.crop((clip[0]-x0, clip[1]-y0, clip[2]-x0, clip[3]-y0))
            black = Image.new('RGBA', patch.size, 'black')
            black.alpha_composite(patch)
            canvas.paste(ImageChops.add(canvas.crop(clip).convert('RGB'), black.convert('RGB')).convert('RGBA'), clip[:2])
        else:
            canvas.alpha_composite(im, (x0, y0))

    def draw_group(canvas, group, ui, tab, *, count=None, offset=(0, 0), expanded_plate=False):
        used = []
        for q in reversed(common):
            if q['group'] != group or q['kind'] == 1 or (group == 12 and q['mark'] != 255):
                continue
            if expanded_plate and group == 9 and q['texture'] == 66:
                continue
            parents, parent = [], q['parent']
            for _ in range(16):
                if parent == 255:
                    break
                p = next((v for v in common if v['group'] == group and v['unit'] == parent), None)
                if p is None:
                    break
                parents.append(p)
                parent = p['parent']
            if group == 11 and any(0 <= p['mark'] <= 4 and not ui['visible'][tab][p['mark']] for p in parents):
                continue
            rgba = q['color'][:]
            for p in parents:
                rgba = [int(rgba[n]*p['color'][n]/255) for n in range(4)]
            if rgba[3] <= 0:
                continue
            texture_frame = 0
            if group == 7:
                mark = q['mark']
                if 9 <= mark <= 10:
                    continue
                if 1 <= mark <= 8:
                    place = 10**(mark-1)
                    if mark > 1 and 30000 < place:
                        continue
                    texture_frame = 30000//place % 10
            if group == 8 and count is not None:
                digit = q['mark']-16 if 17 <= q['mark'] <= 19 else q['mark']
                if not 1 <= digit <= 3 or (digit > 1 and count < 10**(digit-1)):
                    continue
                texture_frame = count//10**(digit-1) % 10 if q['texture'] == 7 else 0
            x, y = q['x'], q['y']
            for p in parents:
                if p['kind'] != 1:
                    break
                x += p['x']
                y += p['y']
            if group == 7:
                x += ui['width']-640
            elif group == 11:
                owner = -1
                for p in parents:
                    owner = {1: 0, 0: 1, 19: 2, 28: 3, 37: 4}.get(p['unit'], owner)
                if owner >= 0:
                    x += (ui['width']-640)*owner/4
            vertices = q['vertices']
            points = [(320+x+vertices[n*2]+offset[0], 240-y-vertices[n*2+1]+offset[1]) for n in range(4)]
            check(q['rotation'] == 0, 'QA renderer accepts the settled unrotated common IDs')
            texture = images.get((0, q['texture'], texture_frame))
            check(texture is not None, f'Drawn source texture{q["texture"]} frame{texture_frame}')
            box = min(p[0] for p in points), min(p[1] for p in points), max(p[0] for p in points), max(p[1] for p in points)
            composite(canvas, texture, box, rgba, q['blend'] in (1, 2))
            used.append(q['texture'])
        return used

    for pixels in ((640, 480), (1920, 1080), (2400, 1080), (1080, 1920)):
        ui = json.loads(subprocess.check_output([args.java, '-cp', str(args.classes), 'GameMenuArtRenderPlan', *map(str, pixels)], text=True))
        width, height = round(ui['width']), round(ui['height'])
        source_back = Image.new('RGBA', (640, 480), 'black')
        composite(source_back, images[0, 66, 0], (0, 60, 640, 420), [225, 225, 225, 255])
        draw_group(source_back, 9, ui, 0, expanded_plate=True)
        source_frame = Image.new('RGBA', (640, 480))
        draw_group(source_frame, 12, ui, 0)

        def slices(source):
            layer = Image.new('RGBA', (width, height))
            for s in ui['slices']:
                src, dst = tuple(map(round, s[:4])), tuple(map(round, s[4:]))
                layer.alpha_composite(source.crop(src).resize((dst[2]-dst[0], dst[3]-dst[1]), Image.Resampling.BILINEAR), dst[:2])
            return layer

        backing, surround = slices(source_back), slices(source_frame)
        for tab in range(5):
            image = backing.copy()
            if tab == 1:
                model = Image.frombytes('RGBA', (640, 480), (args.assets/'case10.rgba').read_bytes())
                zoom, tx, ty = ui['case']
                image.alpha_composite(model.transform(image.size, Image.Transform.AFFINE, (1/zoom, 0, -tx/zoom, 0, 1/zoom, -ty/zoom), Image.Resampling.BILINEAR))
            image.alpha_composite(surround)
            if tab == 1:
                portrait = Image.frombytes('RGBA', (640, 480), (args.assets/'portrait33.rgba').read_bytes())
                image.alpha_composite(portrait, (width-640, height-480))
                q = ui['caseQuad']
                draw = ImageDraw.Draw(image)
                for line in ui['grid']:
                    draw.line(line, fill=(64, 64, 64, 255), width=1)
                dims = json.loads((args.assets/'icon33.json').read_text())
                icon = Image.frombytes('RGBA', (dims['width'], dims['height']), (args.assets/'icon33.rgba').read_bytes())
                source = [(0, 0), (icon.width, 0), (icon.width, icon.height), (0, icon.height)]
                equations, right = [], []
                for n, (sx, sy) in enumerate(source):
                    x, y = q[n*2:n*2+2]
                    equations.extend([[x, y, 1, 0, 0, 0, -sx*x, -sx*y], [0, 0, 0, x, y, 1, -sy*x, -sy*y]])
                    right.extend([sx, sy])
                inverse = np.linalg.solve(np.array(equations), np.array(right))
                image.alpha_composite(icon.transform(image.size, Image.Transform.PERSPECTIVE, tuple(inverse), Image.Resampling.BILINEAR))
                ImageDraw.Draw(image).line([(q[n*2], q[n*2+1]) for n in range(4)]+[(q[0], q[1])], fill=(112, 112, 64, 255), width=2)
                composite(image, images[0, 12, 0], (q[0]-1, q[1]-14, q[0]+10, q[1]), [255]*4)
                draw_group(image, 8, ui, tab, count=5, offset=(q[4]-322, q[5]-242))
            used = draw_group(image, 11, ui, tab)
            draw_group(image, 7, ui, tab)
            check(all(t in used for t in (4, 8, 9, 69, 70)), f'All source tab captions drawn on tab{tab}')
            check(18 in used and 25 in used, f'Original active selection artwork on tab{tab}')
            for x, y in ((0, 0), (width-1, 0), (0, height-1), (width-1, height-1), (width-20, height//2), (width//2, 10), (width//2, height-10)):
                pixel = image.getpixel((x, y))
                check(pixel[3] == 255 and max(pixel[:3]) > 10, f'No unpainted frame margin/tab{tab}/{pixels}/{x,y}')
            if args.output:
                args.output.mkdir(parents=True, exist_ok=True)
                image.resize(pixels, Image.Resampling.LANCZOS).save(args.output/f'm36-menu-{pixels[0]}x{pixels[1]}-tab{tab}.png')
    print(f'Original ID resources, all-tab frame/header/selection/count pixel QA: {checks} checks PASS')


if __name__ == '__main__':
    main()
