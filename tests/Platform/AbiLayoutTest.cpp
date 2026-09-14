#include "UserInterface/Packet.h"
#include "GameLib/ItemData.h"
#include "PackLib/config.h"

#include <cstddef>
#include <type_traits>

// ZiiNAN: 64-bit safety cleanup
static_assert(std::is_standard_layout_v<TPackFileHeader>);
static_assert(sizeof(TPackFileHeader) == 40);
static_assert(offsetof(TPackFileHeader, data_begin) == 8);
static_assert(offsetof(TPackFileHeader, nonce) == 16);
static_assert(sizeof(TPackFileEntry) == 310);
static_assert(offsetof(TPackFileEntry, offset) == 261);
static_assert(offsetof(TPackFileEntry, nonce) == 286);

static_assert(std::is_standard_layout_v<TItemPos>);
static_assert(sizeof(TItemPos) == 3);
static_assert(offsetof(TItemPos, window_type) == 0);
static_assert(offsetof(TItemPos, cell) == 1);
static_assert(sizeof(TPlayerItemAttribute) == 3);
static_assert(offsetof(TPlayerItemAttribute, sValue) == 1);
static_assert(sizeof(TItemData) == 46);
static_assert(offsetof(TItemData, flags) == 5);
static_assert(offsetof(TItemData, aAttr) == 25);

static_assert(sizeof(TPacketGCPhase) == 5);
static_assert(offsetof(TPacketGCPhase, phase) == 4);
static_assert(sizeof(TPacketGCKeyChallenge) == 72);
static_assert(offsetof(TPacketGCKeyChallenge, server_time) == 68);
static_assert(sizeof(TPacketGCKeyComplete) == 76);
static_assert(offsetof(TPacketGCKeyComplete, nonce) == 52);

static_assert(sizeof(TPacketCGMarkLogin) == 12);
static_assert(offsetof(TPacketCGMarkLogin, handle) == 4);
static_assert(sizeof(TPlayerSkill) == 10);
static_assert(offsetof(TPlayerSkill, tNextRead) == 2);
static_assert(sizeof(TPacketGCTime) == 12);
static_assert(offsetof(TPacketGCTime, time) == 4);

static_assert(sizeof(CItemData::TItemLimit) == 5);
static_assert(offsetof(CItemData::TItemLimit, lValue) == 1);
static_assert(sizeof(CItemData::TItemApply) == 5);
static_assert(offsetof(CItemData::TItemApply, lValue) == 1);
static_assert(sizeof(CItemData::TItemTable) == 236);
static_assert(offsetof(CItemData::TItemTable, aLimits) == 166);
static_assert(offsetof(CItemData::TItemTable, alValues) == 191);
static_assert(offsetof(CItemData::TItemTable, alSockets) == 215);
static_assert(offsetof(CItemData::TItemTable, dwRefinedVnum) == 227);

int main()
{
	return 0;
}
