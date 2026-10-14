#!/usr/bin/env python3
"""Verify that test decisions use the frozen source's complete function bodies."""
import pathlib, sys
root=pathlib.Path(__file__).resolve().parents[1]
original=pathlib.Path(sys.argv[1]) if len(sys.argv)>1 else root/'upstream/re4'
src=(original/'src/game/sce_at.cpp').read_text()
oracle=(root/'tests/source_item_effect_oracle.inc').read_text()
def function(text,signature):
    start=text.index(signature);opening=text.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
for name in ('int sceAtCheckItemEffectCol(', 'int sceAtCheckSaveItem('):
    assert function(src,name)==function(oracle,name),f'Frozen oracle changed: {name}'
print('M37 item-effect source: whole sceAtCheckItemEffectCol/sceAtCheckSaveItem oracle bodies identical to frozen source.')
