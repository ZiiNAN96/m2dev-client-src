#pragma once
#include "EterBase/Singleton.h"
#include <memory>

class Win32MoviePlayer;
// ZiiNAN: Platform abstraction
class CMovieMan : public CSingleton<CMovieMan>
{
public:
    CMovieMan();
    ~CMovieMan();
    void ClearToBlack();
    void PlayLogo(const char* name);
    void PlayIntro();
    int PlayTutorial(long index);
private:
    std::unique_ptr<Win32MoviePlayer> m_player;
};
