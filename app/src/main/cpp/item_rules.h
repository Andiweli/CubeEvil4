#pragma once
namespace re4 {
struct ItemInfo { unsigned type{}, defNum{}, maxNum{}; };
ItemInfo OriginalItemInfo(unsigned id);
unsigned OriginalMoneyValue(unsigned id, unsigned count);
}
