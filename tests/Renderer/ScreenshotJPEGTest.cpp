#include <windows.h>
#include "EterImageLib/ScreenshotJPEG.h"
#include <stb_image.h>
#include <cstdio>
#include <iostream>
#include <stdexcept>

static void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main()
{
    wchar_t folder[MAX_PATH]{},path[MAX_PATH]{};
    try {
        Check(GetTempPathW(MAX_PATH,folder) && GetTempFileNameW(folder,L"M11",0,path),"temporary screenshot path");
        std::vector<uint8_t> rgb(16*8*3);
        for(size_t i=0;i<rgb.size();i+=3) { rgb[i]=180; rgb[i+1]=75; rgb[i+2]=30; }
        Check(!SaveScreenshotJPEG(nullptr,rgb,16,8) && !SaveScreenshotJPEG(path,rgb,17,8),"invalid screenshot rejected");
        Check(SaveScreenshotJPEG(path,rgb,16,8),"JPEG save");
        FILE* file=_wfopen(path,L"rb"); Check(file!=nullptr,"JPEG exists");
        int w=0,h=0,c=0; auto* decoded=stbi_load_from_file(file,&w,&h,&c,3); fclose(file);
        const bool valid=decoded && w==16 && h==8 && abs(int(decoded[0])-180)<=3 &&
            abs(int(decoded[1])-75)<=3 && abs(int(decoded[2])-30)<=3;
        stbi_image_free(decoded); Check(valid,"JPEG roundtrip extent/RGB");
        DeleteFileW(path); std::cout<<"Screenshot JPEG RGB roundtrip and failure contract: PASS\n"; return 0;
    } catch(const std::exception& error) {
        if(path[0]) DeleteFileW(path); std::cerr<<error.what()<<'\n'; return 1;
    }
}
