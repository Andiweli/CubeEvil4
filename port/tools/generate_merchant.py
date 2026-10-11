#!/usr/bin/env python3
"""Complete original merchant boot/stock/tune bodies, guarded by source hashes."""
import argparse
import json
import re
from pathlib import Path
from generate_runtime import digest, function


def generate(root, output):
    hashes = json.loads((root/'port/config/merchant-sources.json').read_text())
    for path, expected in hashes.items():
        if digest(root/path) != expected:
            raise ValueError('Original merchant fingerprint mismatch: '+path)
    output.mkdir(parents=True, exist_ok=True)
    (output/'include').mkdir(exist_ok=True)
    header = (root/'include/merchant.h').read_text()
    types = ['#pragma once', '#include "re4_id_save_types.h"', 'namespace re4::port {']
    for name in ['PRICE_INFO', 'LEVEL_PRICE', 'MERCHANT_INFO']:
        types.append(re.search(r'struct '+name+r' \{.*?\n\};', header, re.S)[0])
    char = re.search(r'class MerchantCharacter \{.*?\n\};', header, re.S)[0]
    char = re.sub(r'(\w+\*\s+\w+);', r'\1{};', char)
    types += [char, 'static_assert(sizeof(MERCHANT_DATA)==0x304, "original pointer-free merchant save block");',
              '} // namespace re4::port']
    (output/'include/re4_merchant_types.h').write_text('\n'.join(types)+'\n')
    source = (root/'src/game/merchant.cpp').read_text()
    tables = source[source.index('MERCHANT_INFO merchant_info_A'):source.index('MerchantCharacter merchantChar;')]
    cpp = ['#include "merchant_original.h"', 'namespace re4::port {', tables]
    entries = []
    sigs = ['static inline u32 chkFlag6C(u32 b)', 'void merchant_stage1_full()',
            'void merchant_stage2_full()', 'void merchant_stage3_full()', 'void MerchantGameInit()',
            'void Merchant2ndRoundInit()', 'void stockDataInit(MERCHANT_DATA* p_data)',
            'void add_stock(StockEntry* dst, StockEntry* src)', 'void stockDataAdd(MERCHANT_DATA* d, StockEntry* tbl)',
            'void levelDataInit(MERCHANT_DATA* p_data)', 'void levelDataAdd(MERCHANT_DATA* d, LevelEntry* tbl)',
            'void MerchantDataSave(void* dst)', 'void MerchantDataLoad(void* src)',
            'void MerchantCharacter::setChar(MERCHANT_INFO* info, MERCHANT_DATA* data, PRICE_INFO* sell, PRICE_INFO* exer, LEVEL_PRICE* lvup)']
    for sig in sigs:
        body, line = function(source, sig)
        changes = []
        if not sig.startswith('void MerchantCharacter::'):
            out = re.sub(r'^(?:static inline )?(\w+) (\w+)\(', r'\1 MerchantDomain::\2(', sig)
            changes.append((sig, out))
        if sig == 'void MerchantGameInit()':
            changes.append(('pG->stage_no', '(pG->room_id >> 8)'))
        for old, new in changes:
            if old not in body:
                raise ValueError('Merchant adaptation changed: '+old)
            body = body.replace(old, new)
        cpp += [f'#line {line} "{(root/"src/game/merchant.cpp").as_posix()}"', body]
        entries.append(dict(source='src/game/merchant.cpp', signature=sig, line=line,
                            adaptations=[old for old, _ in changes]))
    cpp.append('} // namespace re4::port')
    (output/'merchant_control.cpp').write_text('\n\n'.join(cpp)+'\n')
    (output/'merchant-manifest.json').write_text(json.dumps(dict(sources=hashes, functions=entries), indent=2)+'\n')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    generate(a.root.resolve(), a.output.resolve())
