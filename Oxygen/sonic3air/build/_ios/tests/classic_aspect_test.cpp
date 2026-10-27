#include "oxygen/helper/ScaledScreenRect.h"
#include "oxygen/rendering/utils/RenderUtils.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    // The actual viewport and coordinate conversion used by GameView.
    for (const Recti outer : {Recti(0,0,896,414), Recti(0,0,1792,828),
                             Recti(0,0,1024,768), Recti(0,0,414,896),
                             Recti(20,30,800,600)}) {
        const Recti rect = RenderUtils::getLetterBoxRect(outer, 4.0f / 3.0f);
        assert(rect.width <= outer.width && rect.height <= outer.height);
        assert(std::abs(rect.width * 3 - rect.height * 4) <= 4);
        assert(std::abs((rect.x - outer.x) * 2 + rect.width - outer.width) <= 1);
        assert(std::abs((rect.y - outer.y) * 2 + rect.height - outer.height) <= 1);
        for (const Vec2i size : {Vec2i(320,224), Vec2i(400,224)}) {
            ScaledScreenRect viewport;
            viewport.setResolution(size);
            viewport.setRectOnScreen(rect);
            const Vec2f center = viewport.getInnerPositionFromScreen(
                Vec2f(rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f));
            assert(std::abs(center.x - size.x * 0.5f) < 0.01f);
            assert(std::abs(center.y - size.y * 0.5f) < 0.01f);
            const Vec2f source(81, 67);
            const Vec2f mapped = viewport.getInnerPositionFromScreen(viewport.getScreenPositionFromInner(source));
            assert(std::abs(source.x - mapped.x) < 0.01f);
            assert(std::abs(source.y - mapped.y) < 0.01f);
            const Vec2f outside = viewport.getInnerPositionFromScreen(Vec2f(rect.x - 2, rect.y));
            assert(outside.x < 0);
        }
    }
    const Recti phone = RenderUtils::getLetterBoxRect(Recti(0,0,896,414), 4.0f/3.0f);
    assert(phone.x == 172 && phone.width == 552 && phone.height == 414);
    std::cout << "4:3 viewport and touch mapping passed: phone, Retina, tablet, portrait, offset; gameplay and menus\n";
}
