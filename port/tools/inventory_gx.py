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
        **{n:'Originaltabellen/SMX: 3 Cull-, 5 Blendmodi und LEQUAL + Depth-Write im statischen SMD-Pfad; allgemeiner GX-Entry offen.' for n in ['GXSetCullMode','GXSetZMode','GXSetBlendMode']},
        **{n:'Strikter nativer Capture-Entry für originale materialSetup/alphaSetup-Bodies; alle 8 Vergleiche/4 Alpha-Logikoperationen, ausschließlich später Tiefentest. Kein allgemeiner Engine-Entry.' for n in ['GXSetAlphaCompare','GXSetZCompLoc']},
        **{n:'Strikter nativer Capture-Entry + zustandsabhängiger Vulkan-Shader: bis 16 Stages, ADD/SUB, Bias/Scale/Clamp, 4 Register. Vergleichs-TEV explizit offen; kein allgemeiner Engine-Entry.' for n in ['GXSetTevOrder','GXSetTevColorIn','GXSetTevColorOp','GXSetTevAlphaIn','GXSetTevAlphaOp','GXSetNumTevStages']},
        **{n:'Capture für originale TextureBlend-Bodies: K0..K3 RGB bzw. Alpha. Andere Konst-Selektoren explizit offen.' for n in ['GXSetTevKColor','GXSetTevKColorSel','GXSetTevKAlphaSel']},
        **{n:'Capture für TEX0 + MTX2x4: Identität oder originale UV-Scale/Translation. Normal-/Position-/Postmatrix-Generatoren explizit offen.' for n in ['GXSetTexCoordGen2','GXSetNumTexGens','GXLoadTexMtxImm']},
        'GXSetNumIndStages':'Capture akzeptiert nur 0; indirekte Texturzugriffe werden explizit abgewiesen.',
        **{n:'Originale LightSetModel-/Typ-Callbacks im Capture-Pfad, 8 SDK-Lichtobjekte, RGB-Kanal mit Diffuse-Clamp/Spotattenuation, unbeleuchtetes Alpha. Allgemeiner GX-Entry und lebende Modell-Eltern/SAR offen.' for n in ['GXSetNumChans','GXSetChanCtrl','GXSetChanAmbColor','GXSetChanMatColor','GXLoadLightObjImm','GXInitLightColor','GXInitLightPos','GXInitLightDir','GXInitLightAttn','GXInitLightSpot','GXInitLightDistAttn','GXInitLightAttnK']},
        'GXSetFog':'LIT-Cut-Zustand an Vulkan-Shader: aus/linear/exp/exp2/rev-exp/rev-exp2. Originales FogInit/setEnv und GX-BP-Koeffizienten-/Range-Adjust noch nicht komplett angebunden.',
    }
    lines=['# GX-Referenzinventar Phase 5a','',
           'Automatisch mit `port/tools/inventory_gx.py` erzeugt. Erfasst benannte GX-Referenzen einschließlich Deklarationen in den drei angeforderten Originaleinheiten, keine behauptete Laufzeitabdeckung. Native Diagnose-Äquivalente sind **keine** bereits eingebundenen allgemeinen GX-Funktionen.', '',
           'Der originale `commonModelTrans`-Body läuft noch nicht nativ. Originale OT-/Material-/Licht-Bodies laufen in einem abgegrenzten Capture-Kontext. Render liefert seine tatsächliche ExecOt-Reihenfolge; zusätzliche Render-Pässe bleiben offen. Unbekannte Display-List-Opcodes, VATs, Materialflags und Ressourcenreferenzen werden mit einem Fehler beendet. Es gibt keine leeren GX-Erfolgs-Stubs.', '',
           '| Originalname | Diagnose-/Integrationsstatus | Quellstellen |','|---|---|---|']
    for name,locations in sorted(calls.items()):
        lines.append('| `'+name+'` | '+equivalent.get(name,'Offen: kein GPU-Adapter/kein generischer nativer Entry in Phase 5a.')+' | '+', '.join('`'+p+'`' for p in locations)+' |')
    lines+=['',f'{len(calls)} unterschiedliche benannte GX-Referenzen. Die bereits vorhandenen CPU-Initialisierungsadapter für Message/TPL und GXSetCopyClear bleiben zusätzlich erhalten; sie beweisen keine allgemeine Geometrie- oder TEV-Ausführung.','']
    return '\n'.join(lines)

if __name__=='__main__':
    root=Path(__file__).resolve().parents[2]
    (root/'port/docs/phase5a-gx-inventory.md').write_text(generate(root),encoding='utf-8')
