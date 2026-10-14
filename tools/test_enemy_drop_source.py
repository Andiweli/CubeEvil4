#!/usr/bin/env python3
"""Prove the transferred item decision routines retain frozen source bodies."""
import pathlib, re, sys
root=pathlib.Path(__file__).resolve().parents[1]
original=pathlib.Path(sys.argv[1]) if len(sys.argv)>1 else root/'upstream/re4'
src=(original/'src/game/em_sub.cpp').read_text()
port=(root/'app/src/main/cpp/original_enemy_drop.cpp').read_text()
def function(text,signature):
    start=text.index(signature);opening=text.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
for name in ('int GetBulletPoint()', 'void GetDropBullet(',
             'static inline void RandomHandgunAmmo(', 'int RandomItemCk('):
    assert function(src,name)==function(port,name),f'Original body changed: {name}'
print('M37 enemy drop source: four complete frozen function bodies identical (GetBulletPoint/GetDropBullet/RandomHandgunAmmo/RandomItemCk).')
