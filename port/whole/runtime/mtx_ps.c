/* PS* matrix/vector/quaternion functions of the Dolphin SDK. On the GameCube
 * they are paired-single assembly (src/lib/mtx.c ...); the port runs the SDK's
 * own C_* reference versions instead, compiled unchanged from the same files
 * (whole_prepare.py: sdk_c/). Results can differ in the last bit where the
 * assembly used the approximate reciprocal square root. */
#include <dolphin/mtx.h>

void PSMTXIdentity(Mtx m) { C_MTXIdentity(m); }
void PSMTXCopy(const Mtx src, Mtx dst) { C_MTXCopy(src, dst); }
void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab) { C_MTXConcat(a, b, ab); }
void PSMTXConcatArray(const Mtx a, const Mtx* srcBase, Mtx* dstBase, u32 count) { C_MTXConcatArray(a, srcBase, dstBase, count); }
void PSMTXTranspose(const Mtx src, Mtx xPose) { C_MTXTranspose(src, xPose); }
u32 PSMTXInverse(const Mtx src, Mtx inv) { return C_MTXInverse(src, inv); }
u32 PSMTXInvXpose(const Mtx src, Mtx invX) { return C_MTXInvXpose(src, invX); }
void PSMTXRotRad(Mtx m, char axis, f32 rad) { C_MTXRotRad(m, axis, rad); }
void PSMTXRotTrig(Mtx m, char axis, f32 sinA, f32 cosA) { C_MTXRotTrig(m, axis, sinA, cosA); }
void PSMTXRotAxisRad(Mtx m, const Vec* axis, f32 rad) { C_MTXRotAxisRad(m, axis, rad); }
void PSMTXTrans(Mtx m, f32 xT, f32 yT, f32 zT) { C_MTXTrans(m, xT, yT, zT); }
void PSMTXTransApply(const Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT) { C_MTXTransApply(src, dst, xT, yT, zT); }
void PSMTXScale(Mtx m, f32 xS, f32 yS, f32 zS) { C_MTXScale(m, xS, yS, zS); }
void PSMTXScaleApply(const Mtx src, Mtx dst, f32 xS, f32 yS, f32 zS) { C_MTXScaleApply(src, dst, xS, yS, zS); }
void PSMTXQuat(Mtx m, const Quaternion* q) { C_MTXQuat(m, q); }
void PSMTXReflect(Mtx m, const Vec* p, const Vec* n) { C_MTXReflect(m, p, n); }

void PSMTXMultVec(const Mtx m, const Vec* src, Vec* dst) { C_MTXMultVec(m, src, dst); }
void PSMTXMultVecArray(const Mtx m, const Vec* srcBase, Vec* dstBase, u32 count) { C_MTXMultVecArray(m, srcBase, dstBase, count); }
void PSMTXMultVecSR(const Mtx m, const Vec* src, Vec* dst) { C_MTXMultVecSR(m, src, dst); }
void PSMTXMultVecArraySR(const Mtx m, const Vec* srcBase, Vec* dstBase, u32 count) { C_MTXMultVecArraySR(m, srcBase, dstBase, count); }

void PSMTX44Identity(Mtx44 m) { C_MTX44Identity(m); }
void PSMTX44Copy(const Mtx44 src, Mtx44 dst) { C_MTX44Copy(src, dst); }
void PSMTX44Concat(const Mtx44 a, const Mtx44 b, Mtx44 ab) { C_MTX44Concat(a, b, ab); }
void PSMTX44Transpose(const Mtx44 src, Mtx44 xPose) { C_MTX44Transpose(src, xPose); }
void PSMTX44Trans(Mtx44 m, f32 xT, f32 yT, f32 zT) { C_MTX44Trans(m, xT, yT, zT); }
void PSMTX44TransApply(const Mtx44 src, Mtx44 dst, f32 xT, f32 yT, f32 zT) { C_MTX44TransApply(src, dst, xT, yT, zT); }
void PSMTX44Scale(Mtx44 m, f32 xS, f32 yS, f32 zS) { C_MTX44Scale(m, xS, yS, zS); }
void PSMTX44ScaleApply(const Mtx44 src, Mtx44 dst, f32 xS, f32 yS, f32 zS) { C_MTX44ScaleApply(src, dst, xS, yS, zS); }
void PSMTX44RotRad(Mtx44 m, char axis, f32 rad) { C_MTX44RotRad(m, axis, rad); }
void PSMTX44RotTrig(Mtx44 m, char axis, f32 sinA, f32 cosA) { C_MTX44RotTrig(m, axis, sinA, cosA); }
void PSMTX44RotAxisRad(Mtx44 m, const Vec* axis, f32 rad) { C_MTX44RotAxisRad(m, axis, rad); }
void PSMTX44MultVec(const Mtx44 m, const Vec* src, Vec* dst) { C_MTX44MultVec(m, src, dst); }
void PSMTX44MultVecArray(const Mtx44 m, const Vec* srcBase, Vec* dstBase, u32 count) { C_MTX44MultVecArray(m, srcBase, dstBase, count); }
void PSMTX44MultVecSR(const Mtx44 m, const Vec* src, Vec* dst) { C_MTX44MultVecSR(m, src, dst); }
void PSMTX44MultVecArraySR(const Mtx44 m, const Vec* srcBase, Vec* dstBase, u32 count) { C_MTX44MultVecArraySR(m, srcBase, dstBase, count); }

/* Reordered matrices (ROMtx, 4x3): the columns of a 3x4 matrix, the layout the
 * paired-single code loads in pairs. The SDK has no C_ versions of these. */
void PSMTXReorder(const Mtx src, ROMtx dest)
{
    int row, col;
    for (col = 0; col < 4; col++) {
        for (row = 0; row < 3; row++) {
            dest[col][row] = src[row][col];
        }
    }
}

void PSMTXROMultVecArray(const ROMtx m, const Vec* srcBase, Vec* dstBase, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++) {
        Vec v = srcBase[i];
        dstBase[i].x = m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0];
        dstBase[i].y = m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1];
        dstBase[i].z = m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2];
    }
}

void PSVECAdd(const Vec* a, const Vec* b, Vec* ab) { C_VECAdd(a, b, ab); }
void PSVECSubtract(const Vec* a, const Vec* b, Vec* a_b) { C_VECSubtract(a, b, a_b); }
void PSVECScale(const Vec* src, Vec* dst, f32 scale) { C_VECScale(src, dst, scale); }
void PSVECNormalize(const Vec* src, Vec* dst) { C_VECNormalize(src, dst); }
f32 PSVECSquareMag(const Vec* v) { return C_VECSquareMag(v); }
f32 PSVECMag(const Vec* v) { return C_VECMag(v); }
f32 PSVECDotProduct(const Vec* a, const Vec* b) { return C_VECDotProduct(a, b); }
void PSVECCrossProduct(const Vec* a, const Vec* b, Vec* axb) { C_VECCrossProduct(a, b, axb); }
f32 PSVECSquareDistance(const Vec* a, const Vec* b) { return C_VECSquareDistance(a, b); }
f32 PSVECDistance(const Vec* a, const Vec* b) { return C_VECDistance(a, b); }

void PSQUATAdd(const Quaternion* p, const Quaternion* q, Quaternion* r) { C_QUATAdd(p, q, r); }
void PSQUATSubtract(const Quaternion* p, const Quaternion* q, Quaternion* r) { C_QUATSubtract(p, q, r); }
void PSQUATMultiply(const Quaternion* p, const Quaternion* q, Quaternion* pq) { C_QUATMultiply(p, q, pq); }
void PSQUATScale(const Quaternion* q, Quaternion* r, f32 scale) { C_QUATScale(q, r, scale); }
f32 PSQUATDotProduct(const Quaternion* p, const Quaternion* q) { return C_QUATDotProduct(p, q); }
void PSQUATNormalize(const Quaternion* src, Quaternion* unit) { C_QUATNormalize(src, unit); }
void PSQUATInverse(const Quaternion* src, Quaternion* inv) { C_QUATInverse(src, inv); }
void PSQUATDivide(const Quaternion* p, const Quaternion* q, Quaternion* r) { C_QUATDivide(p, q, r); }
