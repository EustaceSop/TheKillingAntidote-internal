#pragma once
// Deprecated — the IsBadWritePtr-based helpers were unsafe (undefined behaviour on
// invalid pointers because the template returns nothing).
// For internal cheats, use direct pointer access via game::Get / game::Set defined in game.h.
