"""Exhaustive named GX references in the three requested original render units."""
import re
from pathlib import Path

def generate(root):
    calls={}
    for path in ['src/game/trans.cpp','src/game/model.cpp','src/game/TexRender.cpp']:
        for line,text in enumerate((root/path).read_text().splitlines(),1):
            for name in re.findall(r'\b(GX\w+)\s*\(',text):
                calls.setdefault(name,[]).append(f'{path}:{line}')
    equivalent={
        'GXCallDisplayList':'BIN-Adapter: NOP und VAT0 INDEX16 Quads/Triangles/Strips; kein allgemeiner GX-Entry.',
        **{n:'BIN-Adapter übernimmt ausschließlich die feste commonModelTrans-VAT0-Konfiguration; allgemeiner GX-Entry offen.' for n in ['GXClearVtxDesc','GXSetVtxDesc','GXSetVtxAttrFmt','GXSetArray','GXSetCurrentMtx']},
        **{n:'Diagnosematrizen aus Original-SDK/Modell/Kamera; allgemeiner GX-Entry offen.' for n in ['GXLoadPosMtxImm','GXLoadNrmMtxImm','GXSetProjection']},
        **{n:'TPL-Decoder/Upload/Sampler und Material-ID-Bindung im Diagnosepfad; allgemeiner GX-Entry offen.' for n in ['GXInitTexObj','GXInitTexObjLOD','GXLoadTexObj','GXInitTexObjCI','GXInitTlutObj','GXLoadTlut']},
        **{n:'Diagnosesubset aus Originaltabellen/SMX: 3 Cull-, 5 Blendmodi, LEQUAL + Depth-Write, Alpha-Greater; allgemeiner GX-Entry offen.' for n in ['GXSetCullMode','GXSetZMode','GXSetBlendMode','GXSetAlphaCompare','GXSetZCompLoc']},
        **{n:'Nur festes Diagnosematerial (Textur × SMX-Farbe + Alpha-Maske); kein generischer TEV-Auswerter und kein nativer Entry.' for n in ['GXSetTevOrder','GXSetTevColorIn','GXSetTevColorOp','GXSetTevAlphaIn','GXSetTevAlphaOp']},
    }
    lines=['# GX-Referenzinventar Phase 5a','',
           'Automatisch mit `port/tools/inventory_gx.py` erzeugt. Erfasst benannte GX-Referenzen einschließlich Deklarationen in den drei angeforderten Originaleinheiten, keine behauptete Laufzeitabdeckung. Native Diagnose-Äquivalente sind **keine** bereits eingebundenen allgemeinen GX-Funktionen.', '',
           'Der originale `commonModelTrans`-Body läuft noch nicht nativ. Unbekannte Display-List-Opcodes, VATs, Materialflags und Ressourcenreferenzen werden im Decoder mit einem Fehler beendet. Es gibt keine leeren GX-Erfolgs-Stubs.', '',
           '| Originalname | Diagnose-/Integrationsstatus | Quellstellen |','|---|---|---|']
    for name,locations in sorted(calls.items()):
        lines.append('| `'+name+'` | '+equivalent.get(name,'Offen: kein GPU-Adapter/kein generischer nativer Entry in Phase 5a.')+' | '+', '.join('`'+p+'`' for p in locations)+' |')
    lines+=['',f'{len(calls)} unterschiedliche benannte GX-Referenzen. Die bereits vorhandenen CPU-Initialisierungsadapter für Message/TPL und GXSetCopyClear bleiben zusätzlich erhalten; sie beweisen keine allgemeine Geometrie- oder TEV-Ausführung.','']
    return '\n'.join(lines)

if __name__=='__main__':
    root=Path(__file__).resolve().parents[2]
    (root/'port/docs/phase5a-gx-inventory.md').write_text(generate(root),encoding='utf-8')
