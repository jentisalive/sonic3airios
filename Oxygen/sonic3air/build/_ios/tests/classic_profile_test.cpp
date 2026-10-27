#include "sonic3air/data/ClassicModeRules.h"
#include "oxygen/simulation/PersistentData.h"
#include "oxygen/helper/ScaledScreenRect.h"
#include <filesystem>
#include <cassert>
#include <iostream>

int main(int argc, char** argv)
{
    assert(argc == 2);
    FTX::FileSystem.createDefault();
    const std::wstring root = std::filesystem::path(argv[1]).wstring() + L"/";
    const auto normal = ClassicModeRules::saveRoot(root, false);
    const auto classic = ClassicModeRules::saveRoot(root, true);
    assert(normal == root && classic != normal);
    assert(ClassicModeRules::gameSize(true) == Vec2i(320,240));
    assert(ClassicModeRules::gameSize(false) == Vec2i(400,224));
    using S = SharedDatabase::Setting;
    uint32 value = 99;
    for (uint32 key : {S::SETTING_SMOOTH_ROTATION, S::SETTING_EXTENDED_HUD,
                      S::SETTING_SPEEDUP_AFTERIMGS, S::SETTING_SUPERFAST_RUNANIM,
                      S::SETTING_GFX_ANTIFLICKER}) {
        assert(ClassicModeRules::forcedSetting(key, value) && value == 0);
    }
    assert(ClassicModeRules::forcedSetting(S::SETTING_LIVES_DISPLAY, value) && value == 2);
    assert(!ClassicModeRules::forcedSetting(S::SETTING_INFINITE_LIVES, value));

    // Exercise the actual persistent storage, including a process-style reload.
    PersistentData data;
    const uint64 file = rmx::getMurmur2_64("s3air_savedata");
    const uint64 key = rmx::getMurmur2_64("slot1");
    data.loadFromBasePath(normal + L"storage/");
    data.setData("s3air_savedata", "slot1", {1,2,3});
    data.updatePersistentData();
    data.loadFromBasePath(classic + L"storage/");
    assert(data.getData(file, key).empty());
    data.setData("s3air_savedata", "slot1", {7,8,9});
    data.updatePersistentData();
    for (int repeat = 0; repeat < 3; ++repeat) {
        data.loadFromBasePath(normal + L"storage/");
        assert(data.getData(file, key) == std::vector<uint8>({1,2,3}));
        data.loadFromBasePath(classic + L"storage/");
        assert(data.getData(file, key) == std::vector<uint8>({7,8,9}));
    }
    // Clearing a profile must also discard pending writes, even for matching hashes.
    data.setData("s3air_savedata", "slot1", {99});
    data.clear();
    data.loadFromBasePath(normal + L"storage/");
    data.updatePersistentData();
    assert(data.getData(file, key) == std::vector<uint8>({1,2,3}));
    assert(std::filesystem::is_regular_file(normal + L"storage/s3air_savedata.bin"));
    assert(std::filesystem::is_regular_file(classic + L"storage/s3air_savedata.bin"));

    // Square-pixel classic output: horizontal and vertical scales must match.
    for (Vec2i source : {Vec2i(320,240), Vec2i(400,224)}) {
        ScaledScreenRect viewport;
        viewport.setResolution(source);
        viewport.buildRectAspectFit(Recti(0,0,1920,1080));
        const Recti rect = viewport.getRectOnScreen();
        assert(std::abs(float(rect.width)/source.x - float(rect.height)/source.y) < .01f);
        Vec2f center = viewport.getInnerPositionFromScreen(Vec2f(rect.x + rect.width*.5f, rect.y + rect.height*.5f));
        assert(std::abs(center.x - source.x*.5f) < .01f);
        assert(std::abs(center.y - source.y*.5f) < .01f);
        if (source.x == 320) assert(rect.width == 1440 && rect.height == 1080);
        else assert(rect.width > 1900); // Menus retain width rather than being squeezed into 4:3.
    }
    std::cout << "Classic profile tests passed: locked presets, independent real save files, reload, square pixels and touch mapping\n";
}
