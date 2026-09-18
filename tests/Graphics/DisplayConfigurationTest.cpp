#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../../src/UserInterface/DisplayConfiguration.h"
#include <iostream>
#include <stdexcept>

void Check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
int main()
{
    try
    {
        using namespace DisplayConfiguration;
        Monitor m; m.desktop = {2560,1440}; m.safeWindow = {1920,1080}; m.modes = {{800,600},{1280,720},{1920,1080}};
        Graphics::GraphicsSettings s; s.resolutionWidth = 1280; s.resolutionHeight = 720;
        Check(Normalize(s,m,false), "OS supported window accepted");
        s.resolutionWidth = 1234;
        Check(!Normalize(s,m,false), "invented live mode rejected");
        Check(Normalize(s,m,true) && s.resolutionWidth == 1920 && s.displayMode == Graphics::DisplayMode::Windowed, "startup resolution fallback");
        s.displayMode = Graphics::DisplayMode::Exclusive;
        Check(!Normalize(s,m,false), "unsupported exclusive rejected");
        Check(Normalize(s,m,true) && s.displayMode == Graphics::DisplayMode::Windowed, "startup mode fallback");
        s.displayMode = Graphics::DisplayMode::Borderless; s.resolutionWidth = 2560; s.resolutionHeight = 1440;
        Check(Normalize(s,m,false), "desktop borderless accepted");
        m.desktop = {1920,1080};
        Check(Normalize(s,m,true) && s.displayMode == Graphics::DisplayMode::Windowed, "disconnected larger monitor falls back");
        const auto actual = Query(nullptr);
        Check(!actual.modes.empty() && actual.desktop.first && actual.desktop.second, "live monitor enumeration");
        Check(std::adjacent_find(actual.modes.begin(), actual.modes.end()) == actual.modes.end(), "duplicate refresh variants removed");
        std::cout << "PASS DisplayConfiguration desktop=" << actual.desktop.first << 'x' << actual.desktop.second << " modes=" << actual.modes.size() << '\n';
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
