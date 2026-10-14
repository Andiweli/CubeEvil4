from pathlib import Path
root=Path(__file__).resolve().parents[1]
def function(s,signature):
    start=s.index(signature+'\n{');pos=s.index('{',start);depth=1;i=pos+1
    while depth:
        depth+=(s[i]=='{')-(s[i]=='}');i+=1
    return s[start:i]
source=(root/'upstream/re4/src/em10/em10.cpp').read_text()
port=(root/'app/src/main/cpp/enemy_awareness.cpp').read_text()
for signature in ['int em10FindCk(cEm10* em, int a)','int em10SomebodyFindNowCk(cEm10* em)','int em10SomebodyDamageNowCk(cEm10* em)']:
    assert function(source,signature)==function(port,signature),signature
source=(root/'upstream/re4/src/game/route_ck.cpp').read_text()
oracle=(root/'tests/source_enemy_routes_oracle.inc').read_text()
signatures=['static int rckLineHitCheck(Vec* from, Vec* to, int attr, int mode)','s8 getNearPoint(Vec* pPos, int mode, int flag)','int getNearInfo(cEm* pEm, int mode, int flag)','int RouteCkToPos(cEm* pMy, Vec* pPos, Vec* pDest, int mode, f32* pMax)','f32 RouteCkGetDist(int n0, int n1)','f32 RouteCkPosToPosDis(Vec* pPos1, Vec* pPos2)']
for signature in signatures:
    assert function(source,signature).replace('(u32)','(std::uintptr_t)')==function(oracle,signature),signature
print('M37 whole original FindCk and two peer checks unchanged; six route oracle bodies exact with only native pointer-width casts: PASS')
