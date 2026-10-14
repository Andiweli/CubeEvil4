// Frozen verbatim function bodies and tables from upstream/re4 commit
// 9b76ff003081d6ece8b838453fb8310743a972c7. The test supplies narrow SDK stubs.
static const AtEffInfo r100_eff_info2 = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

static const AtEffInfo r100_eff_info4 = {
    0, {0xD2, 0}, {1, 0xF}, {0, 0xB}, {0, 0xC}, {1, 0xE}, {1, 0xE}, {0, 0x36}, {0, 0x27},
};

static const AtEffInfo r100_eff_info5 = {
    2, {0xD2, 0}, {0xD2, 0}, {0xD2, 0}, {0xD2, 0}, {0xD2, 0}, {0xD2, 0}, {0xD2, 0}, {0xD2, 0},
};

int AtEffInfo::getWepEff(int wepNo, u32* type, u32* id)
{
    int ret = 1;

    switch (wepNo) {
    case 1:  // handguns
    case 2:
    case 3:
    case 4:
        *type = effGun[0];
        *id = effGun[1];
        break;
    case 5:  // shotguns
    case 6:
    case 7:
    case 8:
        *type = effGun[0];
        *id = effGun[1];
        break;
    case 9:  // rifles
    case 10:
    case 11:
    case 12:
        *type = effGun[0];
        *id = effGun[1];
        break;
    case 0x0F:
    case 0x11:
    case 0x21:
        *type = effGun[0];
        *id = effGun[1];
        break;
    case 0x13:
        *type = eff13[0];
        *id = eff13[1];
        break;
    case 0x16:
        *type = eff16[0];
        *id = eff16[1];
        break;
    case 0x17:
        *type = eff17[0];
        *id = eff17[1];
        break;
    case 0x0D:
        *type = eff0D[0];
        *id = eff0D[1];
        break;
    default:
        ret = 0;
        pLog->err(0, 0, "getWepEff() INVALID ARG %d %d %d", wepNo, *type, *id);
        break;
    }
    return ret;
}

int EatGetEffectType(u32 rgba)
{
    int type = 0;

    if (rgba & EAT_ATTR_EFF_BIT0) {
        type = 1;
    }
    if (rgba & EAT_ATTR_EFF_BIT1) {
        type += 2;
    }
    if (rgba & EAT_ATTR_EFF_BIT2) {
        type += 4;
    }
    return type;
}

void cEsp05::move()
{
    ESP05_WK* w = &m_Free;

    if (CommonMove()) {
        m_Pos.x += w->Pow * sinf(w->Theta);
        m_Pos.z += w->Pow * cosf(w->Theta * 1.7f);
        m_Pos.y += w->Pow * 0.3f * cosf(w->Theta * 1.9f);
        w->Theta += w->Spd * (fRand0_1() + 0.2f);
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

int cEsp05::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    ESP05_WK* w = &m_Free;

    w->Pow = pSeq->Vec0.x;
    w->Spd = pSeq->Vec0.y * 0.05f;
    w->Pow += w->Pow * fRand0_1() * (pSeq->Vec0.z * 0.1f);
    w->Spd += w->Spd * fRand0_1() * (pSeq->Vec0.z * 0.1f);
    w->Theta = fRand0_1() * PI * 2.0f;
    return 1;
}

int cEsp40::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    f32 h;

    if (parent != pEffParentWorld && m_Release_time == 0) {
        ApplyMatrix(parent->mat);
        parent = pEffParentWorld;
    }
    if (parent != pEffParentWorld && (m_Release_time == 0xff || m_Release_time <= m_Life_time)) {
        ESP40_WK* w = &m_Free;
        w->Ofs_y = pSeq->Pos.y;
        w->Base_Pos = m_Pos;
        w->Speed_y = m_Speed.y;
        w->Speed_plus_y = m_Speed_plus.y;
    } else if (GetWaterHeight(&m_Pos, &h)) {
        m_Pos.y = h + pSeq->Pos.y;
    }
    return 1;
}
