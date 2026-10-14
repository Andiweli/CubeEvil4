// Literal upstream game/pl_wep.cpp::PlWepLockRand.
void PlWepLockRand(cModel* pEm, int mflag, f32* ang_x, f32* ang_y)
{
    cPlayer* pl = (cPlayer*) pEm;
    cPlWep* wep = pl->Wep;
    f32 rP;
    f32 rY;
    f32 sP;
    f32 sY;

    *ang_x *= PI / 2.0f;
    wep->m_pWep->getAbility(&rP, &rY, &sP, &sY);
    if (mflag & 1) {
        wep->pitch = *ang_x;
        wep->m_CenterY = *ang_y;
    } else if (mflag & 2) {
        *ang_x = fRand1_1() * rP * lockRandCtr + wep->pitch;
        *ang_y = fRand1_1() * rY * lockRandCtr + wep->m_CenterY;
    } else {
        *ang_x = sP * fRand1_1() + *ang_x;
        *ang_y = sY * fRand1_1() + *ang_y;
        if (*ang_x > wep->pitch + rP) {
            *ang_x = wep->pitch + rP;
        } else if (*ang_x < wep->pitch - rP) {
            *ang_x = wep->pitch - rP;
        }
        if (*ang_y > wep->m_CenterY + rP) {
            *ang_y = wep->m_CenterY + rP;
        } else if (*ang_y < wep->m_CenterY - rP) {
            *ang_y = wep->m_CenterY - rP;
        }
    }
    *ang_x *= 2.0f / PI;
}

