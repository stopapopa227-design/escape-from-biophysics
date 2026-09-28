#pragma once
constexpr GLenum TextureArray=0x8C1A,Texture0=0x84C0;
GLuint materialTextures[3]{};
GLuint emblemTexture=0;
std::filesystem::path materialDirectory() {
    wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
    std::filesystem::path dir=std::filesystem::path(exe).parent_path();
    for(int i=0;i<5;++i) {
        auto candidate=dir/L"assets"/L"materials";
        if(std::filesystem::exists(candidate/L"beige_wall_001-Diffuse.png"))return candidate;
        dir=dir.parent_path();
    }
    throw std::runtime_error("Missing assets/materials. Keep the assets folder next to the game or in the project directory.");
}
void loadMaterials(GLuint shader) {
    using Microsoft::WRL::ComPtr;
    HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(init)&&init!=RPC_E_CHANGED_MODE)throw std::runtime_error("Could not initialize Windows image decoder.");
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(FAILED(hr))throw std::runtime_error("Windows Imaging Component is unavailable.");
    auto dir=materialDirectory();
    const wchar_t* assets[]={L"beige_wall_001",L"terrazzo_tiles",L"wood_table_001"};
    const wchar_t* maps[]={L"Diffuse",L"nor_gl",L"Rough"};
    const char* uniforms[]={"uAlbedoMaps","uNormalMaps","uRoughMaps"};
    glGenTextures(3,materialTextures);
    for(int map=0;map<3;++map) {
        glActiveTexture(Texture0+map);glBindTexture(TextureArray,materialTextures[map]);
        glTexImage3D(TextureArray,0,GL_RGBA,1024,1024,3,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        for(int layer=0;layer<3;++layer) {
            auto path=dir/(std::wstring(assets[layer])+L"-"+maps[map]+L".png");
            ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
            hr=factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);
            if(FAILED(hr))throw std::runtime_error("Could not open material image: "+path.string()+" HRESULT "+std::to_string(static_cast<unsigned long>(hr)));
            hr=decoder->GetFrame(0,&frame);if(FAILED(hr))throw std::runtime_error("Could not read material frame.");
            UINT w=0,h=0;frame->GetSize(&w,&h);
            if(w!=1024||h!=1024)throw std::runtime_error("Material textures must be 1024 x 1024.");
            hr=factory->CreateFormatConverter(&converter);if(FAILED(hr))throw std::runtime_error("Image converter failed.");
            hr=converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
            if(FAILED(hr))throw std::runtime_error("RGBA image conversion failed.");
            std::vector<uint8_t> rgba(size_t(w)*h*4);
            hr=converter->CopyPixels(nullptr,w*4,UINT(rgba.size()),rgba.data());
            if(FAILED(hr))throw std::runtime_error("Could not decode material pixels.");
            glTexSubImage3D(TextureArray,0,0,0,layer,1024,1024,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
        }
        glTexParameteri(TextureArray,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(TextureArray,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(TextureArray,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(TextureArray,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glGenerateMipmap(TextureArray);glUniform1i(glGetUniformLocation(shader,uniforms[map]),map);
    }
    {
        auto path=dir.parent_path()/L"signage"/L"faculty-emblem.png";
        ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
        if(FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&frame)))throw std::runtime_error("Could not load faculty emblem.");
        UINT w=0,h=0;frame->GetSize(&w,&h);
        if(FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))throw std::runtime_error("Could not convert emblem.");
        std::vector<uint8_t> rgba(size_t(w)*h*4);
        if(FAILED(converter->CopyPixels(nullptr,w*4,UINT(rgba.size()),rgba.data())))throw std::runtime_error("Could not decode emblem pixels.");
        glGenTextures(1,&emblemTexture);glActiveTexture(Texture0+3);glBindTexture(GL_TEXTURE_2D,emblemTexture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
        glGenerateMipmap(GL_TEXTURE_2D);glUniform1i(glGetUniformLocation(shader,"uEmblem"),3);
    }
    factory.Reset();if(SUCCEEDED(init))CoUninitialize();glActiveTexture(Texture0);
}
