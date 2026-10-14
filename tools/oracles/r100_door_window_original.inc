// Literal upstream/re4/src/st1/r100.cpp task bodies; SceSleep uses a host fiber.
static void r100_DoorCk()
{
    cEmDoor* door;
    int found = 0;
    int i;

    if (getRoomEtcDoor(2, &door, 1) == 0) {
        return;
    }
    while (RsfCheck(G_ROOM_ID, 3) == 0) {
        door->setOpenLock(0);
        SceSleep(1);
    }
    pG->Room_flg[0] &= ~0x80000000;
    found = 0;
    while (RsfCheck(G_ROOM_ID, 4) == 0) {
        if (pG->Room_flg[0] & 0x80000000) {
            found = 1;
        }
        if (found == 1) {
            if (pG->Room_flg[2] & 0x80000000) {
                SndCall(6, 0x28, &door->pos, 0, 0, 0);
            }
            break;
        }
        if (RsfCheck(G_ROOM_ID, 10)) {
            cEmGanado* em0 = W->ems[0];
            cEmGanado* em1 = W->ems[1];
            cEmGanado* em2 = W->ems[2];

            if (em0 != errEm && em0->ckFindPL() == 1) {
                found = 1;
            }
            if (em1 != errEm && em1->ckFindPL() == 1) {
                found = 1;
            }
            if (em2 != errEm && em2->ckFindPL() == 1) {
                found = 1;
            }
        }
        door->setOpenLock(0);
        SceSleep(1);
    }
    i = 0;
    while (RsfCheck(G_ROOM_ID, 4) == 0) {
        door->setCloseLock();
        SceSleep(1);
        if (i++ == 19) {
            SceAtSetEnable(0xB, 1);
        }
    }
    SceAtSetEnable(0xB, 0);
    door->setNormal();
}

// The two windows of the first house: fenced while the ambush hunts the player.
static void r100_WindowBreakCk()
{
    cEmWindow* win0;
    cEmWindow* win1;

    if (getRoomEtcWindow(0, &win0, 1) == 0) {
        return;
    }
    if (getRoomEtcWindow(1, &win1, 1) == 0) {
        return;
    }
    while (RsfCheck(G_ROOM_ID, 10) == 0) {
        win0->SetEnableFence(0, 0);
        win1->SetEnableFence(0, 0);
        SceSleep(1);
    }
    win0->SetEnableFence(1, 0);
    win1->SetEnableFence(1, 0);
    while (RsfCheck(G_ROOM_ID, 4) == 0) {
        if (!(pG->Room_flg[2] & 0x80000000)) {
            RsfSet(G_ROOM_ID, 4);
            if (W->ems[0] != errEm) {
                W->ems[0]->flag |= 0x80;
            }
            if (W->ems[1] != errEm) {
                W->ems[1]->flag |= 0x80;
            }
            if (W->ems[2] != errEm) {
                W->ems[2]->flag |= 0x80;
            }
            break;
        }
        SceSleep(1);
    }
}

