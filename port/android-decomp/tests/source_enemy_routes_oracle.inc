static int rckLineHitCheck(Vec* from, Vec* to, int attr, int mode)
{
    Vec pa;
    Vec pb;
    int mask;

    pa = *from;
    pb = *to;
    if (pG->debug_mode == 8) {
        Draw_line3d(&pa, &pb, 0xFFFF0000, 0);
    }
    attr |= 0x4000;
    mask = 0x383070;
    if (mode & 2) {
        mask = 0x383078;
    }
    if (!(mode & 4)) {
        mask |= 0x40000;
    }
    return SatMgr.hitCheck(&pa, &pb, NULL, NULL, attr, mask);
}

s8 getNearPoint(Vec* pPos, int mode, int flag)
{
    f32 dist[10];
    int idx[10];
    Vec p2;
    RTP* rtp;
    RTP_POINT* pt;
    int* ip;
    int n;
    int m;
    int i;
    int j;
    int k;
    f32 d;

    rtp = rtpData();
    if (rtp == NULL) {
        return -1;
    }
    n = rtp->nPoint;
    if (n == 0) {
        return -1;
    }
    m = n;
    if (m > 10) {
        m = 10;
    }
    for (i = 0; i < m; i++) {
        dist[i] = 1.0e16f;
    }
    pt = rtpPoint(rtpData());
    for (i = 0; i < n; i++) {
        d = (pPos->x - pt->pos.x) * (pPos->x - pt->pos.x) + (pPos->y - pt->pos.y) * (pPos->y - pt->pos.y) +
            (pPos->z - pt->pos.z) * (pPos->z - pt->pos.z);
        for (j = m; j > 0; j--) {
            if (d > dist[j - 1]) {
                break;
            }
        }
        if (j < m) {
            for (k = m - 1; k > j; k--) {
                idx[k] = idx[k - 1];
                dist[k] = dist[k - 1];
            }
            idx[j] = i;
            dist[j] = d;
        }
        pt++;
    }
    if (mode != 0) {
        return idx[0];
    }
    pt = rtpPoint(rtpData());
    p2 = *pPos;
    p2.y += 500.0f;
    for (i = 0, ip = idx; i < m; i++, ip++) {
        if (rckLineHitCheck(&p2, &pt[*ip].pos, flag, 0) == 0) {
            return *ip;
        }
    }
    return -1;
}

int getNearInfo(cEm* pEm, int mode, int flag)
{
    if (pEm->RckStat & 1) {
        return pEm->RckNear;
    }
    pEm->RckStat |= 1;
    return getNearPoint(&pEm->pos, mode, flag);
}

int RouteCkToPos(cEm* pMy, Vec* pPos, Vec* pDest, int mode, f32* pMax)
{
    Vec a;
    Vec b;
    Vec c;
    int p;
    int t;
    int next;
    int mask;
    RTP_POINT* pts;
    RTP_POINT* pt;
    s8* tbl;
    f32 d2;
    f32 dmax;

    a = pMy->pos;
    a.y += 500.0f;
    b = *pPos;
    b.y += 500.0f;
    mask = pMy->atari.m_flag;
    if (pMy->id == 3) {
        mode |= 4;
    }
    if (!(mode & 1)) {
        if (pG->Rtp == NULL) {
            *pDest = *pPos;
            if (pMax != NULL) {
                *pMax = a.y - b.y;
                *pMax = fabsf(*pMax);
            }
            return 1;
        }
        if (rckLineHitCheck(&a, &b, mask, mode) == 0) {
            PosToPos(&a, &b, &c, 0.5f);
            if (SatMgr.getFloor(&c, NULL, 600.0f, 100000.0f, 0) > c.y - 2000.0f) {
                *pDest = *pPos;
                if (pMax != NULL) {
                    *pMax = a.y - b.y;
                    *pMax = fabsf(*pMax);
                }
                return 1;
            }
        }
    }
    pMy->RckMy = pMy->RckNear = getNearInfo(pMy, 0, mask);
    pMy->RckTo = getNearPoint(&b, 0, mask);
    p = pMy->RckMy;
    if (p == -1) {
        *pDest = *pPos;
        if (pMax != NULL) {
            *pMax = a.y - b.y;
            *pMax = fabsf(*pMax);
        }
        return 1;
    }
    t = pMy->RckTo;
    if (t == -1) {
        *pDest = *pPos;
        if (pMax != NULL) {
            *pMax = a.y - b.y;
            *pMax = fabsf(*pMax);
        }
        return 1;
    }
    {
        RTP* r = (RTP*) Global.Rtp;
        tbl = (s8*) (r->pNext + (std::uintptr_t) r);
    }
    next = tbl[rtpData()->nPoint * p + t];
    if (next == -1) {
        *pDest = *pPos;
        if (pMax != NULL) {
            *pMax = a.y - b.y;
            *pMax = fabsf(*pMax);
        }
        return 1;
    }
    if ((mode & 1) && next == t) {
        if (fabsf(a.y - b.y) < 2000.0f) {
            if (pG->Rtp == NULL || rckLineHitCheck(&a, &b, mask, mode) == 0) {
                *pDest = *pPos;
                if (pMax != NULL) {
                    *pMax = a.y - b.y;
                    *pMax = fabsf(*pMax);
                }
                return 1;
            }
        }
    }
    mask |= 0x80;
    pts = rtpPoint(rtpData());
    pt = (RTP_POINT*)(pMy->RckMy * sizeof(RTP_POINT) + (std::uintptr_t)pts);
    d2 = (pMy->pos.x - pt->pos.x) * (pMy->pos.x - pt->pos.x) + (pMy->pos.z - pt->pos.z) * (pMy->pos.z - pt->pos.z);
    if (d2 < 62500.0f || (next != pMy->RckMy && rckLineHitCheck(&a, &pts[next].pos, mask, mode) == 0)) {
        pMy->RckMy = next;
    }
    *pDest = rtpPoint(rtpData())[pMy->RckMy].pos;
    if (pMax != NULL) {
        RTP* r;
        int np;
        // The hop loop reuses `next` (one global pseudo, r30) and keeps the table read in the loop
        // test, so the exit test copied to the entry is the pre-loop `tbl[..]`; the struct-view rtp
        // and `np` are the fresh `pG` load and the hoisted nPoint. A `while` whose body is under
        // 30 raw insns would have the whole body up to the `break` rotated instead.
        dmax = a.y - b.y;
        dmax = fabsf(dmax);
        r = (RTP*) pG->Rtp;
        np = r->nPoint;
        next = pMy->RckMy;
        while ((next = tbl[np * next + pMy->RckTo]) != -1) {
            f32 d = fabsf(a.y - rtpPoint(r)[next].pos.y);
            if (d > dmax) {
                dmax = d;
            }
            if (next == pMy->RckTo) {
                break;
            }
        }
        *pMax = dmax;
    }
    return 0;
}

f32 RouteCkGetDist(int n0, int n1)
{
    f32 d = 0.0f;
    int next;
    RTP_POINT* pt;
    RTP_POINT* np;
    s8* tbl;
    Vec tmp;

    if (n0 == n1) {
        return d;
    }
    {
        RTP* r = (RTP*) Global.Rtp;
        tbl = (s8*) (r->pNext + (std::uintptr_t) r);
    }
    pt = &rtpPoint(rtpData())[n0];
    do {
        next = tbl[rtpData()->nPoint * n0 + n1];
        if (next == -1) {
            PSVECSubtract(&rtpPoint(rtpData())[n0].pos, &rtpPoint(rtpData())[n1].pos, &tmp);
            return PSVECMag(&tmp);
        }
        np = &rtpPoint(rtpData())[next];
        PSVECSubtract(&pt->pos, &np->pos, &tmp);
        d += PSVECMag(&tmp);
        n0 = next;
        pt = np;
    } while (n0 != n1);
    return d;
}

f32 RouteCkPosToPosDis(Vec* pPos1, Vec* pPos2)
{
    Vec a;
    Vec b;
    Vec c;
    int p;
    int t;

    a = *pPos1;
    b = *pPos2;
    a.y += 500.0f;
    b.y += 500.0f;
    if (pG->Rtp != NULL) {
        // COMPILER-DIFF: candidate (reload_cse register table): the original keeps `mr r3,r31; mr r4,r29`
        // for the from/to arguments although r3/r4 still hold them since the entry copies; ours deletes
        // the two copies in reload_cse_regs. The volatile asm forgets the table (a label or call would too).
        asm volatile("" : : : "memory");
        if (rckLineHitCheck(pPos1, pPos2, 0, 0) == 0) {
            PosToPos(&a, &b, &c, 0.5f);
            if (SatMgr.getFloor(&c, NULL, 600.0f, 100000.0f, 0) > c.y - 2000.0f) {
                goto direct;
            }
        }
        p = getNearPoint(pPos1, 0, 0);
        if (p == -1) {
            goto direct;
        }
        t = getNearPoint(pPos2, 0, 0);
        if (t == -1) {
            goto direct;
        }
        return RouteCkGetDist(p, t);
    }
direct:
    return VEC_DISTXZ(pPos1, pPos2);
}
