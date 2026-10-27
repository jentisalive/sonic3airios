#pragma once
#include <rmxbase.h>

namespace ClassicScreenLayout
{
    constexpr int GAME_WIDTH = 320;
    constexpr int GAME_HEIGHT = 224; // The following emulated RAM belongs to dynamic objects.
    inline Vec2i constrain(bool classic, Vec2i requested)
    {
        if (!classic) return requested;
        return Vec2i(requested.x == 400 ? 400 : GAME_WIDTH, GAME_HEIGHT);
    }
    inline bool isMenu(bool classic, Vec2i source, bool menuActive = false)
    {
        return classic && (menuActive || source == Vec2i(400, GAME_HEIGHT));
    }
    inline Vec2i compositionSize(bool classic, Vec2i source, bool menuActive = false)
    {
        // Native menus use a taller presentation canvas, never a taller simulation.
        return isMenu(classic, source, menuActive) ? Vec2i(400, 300) : source;
    }
}
