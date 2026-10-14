// Frozen game/item.cpp::itemInfo, native fields and no GC pointers.
#include "item_rules.h"
namespace re4 {
unsigned OriginalMoneyValue(unsigned id, unsigned count) {
    switch (id) {
    case 0x78: return count ? count * 10 : 300;
    case 0x79: return count ? count * 100 : 3000;
    case 0x71: return count ? count * 1000 : 5000;
    default: return 0;
    }
}
ItemInfo OriginalItemInfo(unsigned id)
{
    ItemInfo result; auto *info=&result;
    switch (id) {
    case 0x03:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x36:
    case 0x37:
    case 0x3E:
    case 0x40:
    case 0x51:
    case 0x52:
    case 0x6B:
    case 0x6C:
    case 0x94:
    case 0x99:
    case 0xAB:
        info->type = 1;
        info->defNum = 0;
        info->maxNum = 0;
        break;
    case 0x01:
    case 0x02:
    case 0x0E:
    case 0x17:
    case 0x35:
    case 0x38:
    case 0x6D:
        info->type = 3;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    case 0x04:
        info->type = 2;
        info->defNum = 10;
        info->maxNum = 50;
        break;
    case 0x00:
        info->type = 2;
        info->defNum = 5;
        info->maxNum = 10;
        break;
    case 0x18:
        info->type = 2;
        info->defNum = 10;
        info->maxNum = 15;
        break;
    case 0x07:
        info->type = 2;
        info->defNum = 5;
        info->maxNum = 10;
        break;
    case 0x6A:
        info->type = 2;
        info->defNum = 50;
        info->maxNum = 100;
        break;
    case 0x20:
        info->type = 2;
        info->defNum = 50;
        info->maxNum = 100;
        break;
    case 0x1A:
        info->type = 2;
        info->defNum = 10;
        info->maxNum = 10;
        break;
    case 0x46:
        info->type = 2;
        info->defNum = 1;
        info->maxNum = 5;
        break;
    case 0xA0:
        info->type = 2;
        info->defNum = 5;
        info->maxNum = 10;
        break;
    case 0x72:
        info->type = 2;
        info->defNum = 5;
        info->maxNum = 20;
        break;
    case 0x3F:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0xAA:
    case 0xC5:
        info->type = 9;
        info->defNum = 0;
        info->maxNum = 0;
        break;
    case 0x05:
    case 0x08:
    case 0x09:
    case 0x0A:
        info->type = 6;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    case 0x06:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x19:
    case 0x1C:
    case 0x95:
    case 0x97:
    case 0xA8:
        info->type = 6;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    case 0x56:
    case 0x57:
    case 0x5F:
    case 0x60:
    case 0x61:
    case 0x77:
    case 0xA1:
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xC7:
    case 0xC8:
    case 0xC9:
    case 0xD2:
    case 0xD3:
    case 0xD4:
        info->type = 12;
        info->defNum = 1;
        info->maxNum = 999;
        break;
    case 0x1B:
    case 0x58:
    case 0x59:
    case 0x5A:
    case 0x5B:
    case 0x5C:
    case 0x5D:
    case 0x5E:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
    case 0x68:
    case 0x70:
    case 0x89:
    case 0x8A:
    case 0x8F:
    case 0x90:
    case 0x91:
    case 0x93:
    case 0x96:
    case 0x98:
    case 0x9A:
    case 0x9B:
    case 0x9C:
    case 0x9D:
    case 0x9E:
    case 0x9F:
    case 0xA2:
    case 0xB8:
    case 0xBC:
    case 0xBD:
    case 0xBE:
    case 0xBF:
    case 0xC0:
    case 0xC1:
    case 0xC2:
    case 0xC6:
    case 0xCA:
    case 0xCB:
    case 0xCC:
    case 0xCD:
    case 0xCE:
    case 0xCF:
    case 0xD0:
    case 0xD1:
    case 0xD5:
    case 0xD6:
    case 0xD7:
    case 0xD8:
    case 0xD9:
    case 0xDA:
    case 0xDB:
        info->type = 5;
        info->defNum = 1;
        info->maxNum = 999;
        break;
    case 0x78:
        info->type = 8;
        info->defNum = 30;
        info->maxNum = 999;
        break;
    case 0x79:
        info->type = 8;
        info->defNum = 30;
        info->maxNum = 999;
        [[fallthrough]];
    case 0x71:
        info->type = 8;
        info->defNum = 5;
        info->maxNum = 999;
        break;
    case 0x73:
        info->type = 8;
        info->defNum = 30;
        info->maxNum = 999;
        break;
    case 0x75:
        info->type = 8;
        info->defNum = 30;
        info->maxNum = 999;
        break;
    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x4B:
    case 0x4C:
    case 0x4D:
    case 0x4E:
    case 0x4F:
    case 0x50:
    case 0xAC:
    case 0xAD:
    case 0xAE:
    case 0xAF:
    case 0xB0:
    case 0xB1:
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB5:
    case 0xB6:
    case 0xB7:
    case 0xF4:
    case 0xF5:
    case 0xF6:
    case 0xF7:
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFD:
        info->type = 10;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    case 0x54:
    case 0x55:
    case 0x7C:
    case 0x7D:
    case 0x7E:
    case 0x7F:
    case 0xA9:
    case 0xFE:
        info->type = 11;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    case 0xDC:
    case 0xDD:
    case 0xDE:
    case 0xDF:
    case 0xE0:
    case 0xE1:
    case 0xE2:
    case 0xE3:
    case 0xE4:
    case 0xE5:
    case 0xE6:
    case 0xE7:
    case 0xE8:
    case 0xE9:
    case 0xEA:
    case 0xEB:
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xEF:
    case 0xF0:
    case 0xF1:
    case 0xF2:
    case 0xF3:
        info->type = 13;
        info->defNum = 1;
        info->maxNum = 999;
        break;
    case 0x0C:
        info->type = 14;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    default:
        info->type = 7;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    }
    return result;
}
} // namespace re4
