#include "StdAfx.h"
#include "ScreenshotJPEG.h"
#include <stb_image_write.h>
#include <limits>

bool SaveScreenshotJPEG(const wchar_t* path,const std::vector<uint8_t>& rgb,uint32_t width,uint32_t height)
{
    if(!path || !width || !height || width>INT_MAX || height>INT_MAX ||
        uint64_t(width)*height*3!=rgb.size()) return false;
    FILE* file=_wfopen(path,L"wb");
    if(!file) return false;
    struct Output { FILE* file; bool failed=false; } output{file};
    const auto write=[](void* context,void* bytes,int size) {
        auto& out=*static_cast<Output*>(context);
        if(size<0 || fwrite(bytes,1,size,out.file)!=size_t(size)) out.failed=true;
    };
    const bool encoded=stbi_write_jpg_to_func(write,&output,int(width),int(height),3,rgb.data(),100)!=0;
    const bool closed=fclose(file)==0;
    return encoded && !output.failed && closed;
}
