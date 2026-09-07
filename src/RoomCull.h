// RoomCull.h -- defeat TR4's portal culling so head-tracked views are not empty.
//
// The engine draws only the rooms in a list built by portal traversal. Rooms
// reached through no camera-facing portal are never submitted, so turning your
// head far enough off the game camera's axis finds nothing to draw.
//
// The exe cannot fix this: it contains no culling code at all, and widening the
// projection it hands the game was measured to change nothing (64.4 -> 103.1
// degrees, zero extra geometry). The traversal lives in tomb4.dll:
//
//   FUN_18002ea30  GetRoomBounds -- portal traversal, fills the draw list
//   DAT_18063dc60  the draw list (room indices, 206 slots)
//   DAT_18063ddfc  how many are in it
//   DAT_180660810  room count
//   DAT_180663fc8  rooms array (stride 0x130)
//   room+0x4C/4E/50/52  left/right/top/bottom clip rect, shorts
//   room+0x4B bit 0     "already in the list"
//
// The engine already does exactly what we need for a special case: after
// traversal it appends any room whose flags have bit 0x40000, bypassing portals
// entirely. We do the same for every room, and set the clip rects to full screen
// so the appended rooms are not clipped away by the bounds left over from the
// previous frame's reset.
//
// The cost is real: every room in the level is drawn every frame, with no
// visibility culling at all. TR4 rooms are small, but a large level will cost
// frame rate. Off by default.
#pragma once

namespace tr {

// Install the hook once tomb4.dll is loaded. Cheap and idempotent; call once
// per frame. Does nothing unless the config asks for it and TR4 is running.
void RoomCullUpdate();

// Remove the hook.
void RoomCullShutdown();

} // namespace tr
