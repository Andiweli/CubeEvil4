// Frozen source bodies: game/sce_at.cpp. Test adapters provide itemInfo.
int sceAtCheckItemEffectCol(ITEM_ID item_id)
{
    ITEM_INFO info;

    if (item_id == 0x8C) {
        return 8;
    }
    itemInfo(item_id, &info);
    switch (info.type) {
    case 1:
    case 2:
    case 3:
    case 4:
        return 5;
    case 6:
        return 4;
    case 0:
    case 5:
    case 7:
        return 2;
    case 8:
        return 3;
    case 0xC:
        return 3;
    default:
        return 3;
    }
}

int sceAtCheckSaveItem(u16 id)
{
    ITEM_INFO info;

    itemInfo(id, &info);
    if (info.type == 5 || info.type == 7) {
        return 1;
    }
    return 0;
}
