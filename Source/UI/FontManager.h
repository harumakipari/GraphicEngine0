#pragma once
#include "Font.h"

class FontManager
{
public:
    static void Initialize(ID3D11Device* device, const char* filename)
    {
        uiFont = std::make_unique<Font>(
            device,
            filename,
            1024
        );
    }

    static Font* GetUIFont()
    {
        return uiFont.get();
    }

private:
    static  inline std::unique_ptr<Font> uiFont;
};
