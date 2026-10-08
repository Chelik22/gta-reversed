#include "StdInc.h"

#include "rpanisot.h"

RwInt8 RpAnisotGetMaxSupportedMaxAnisotropy() {
    return *(const RwInt8 *)((const RwUInt8 *)RwD3D9GetCaps() + 0x6C); // 0x6C == offsetof(D3DCAPS9, MaxAnisotropy), читается 1 байт
}


RwTexture* RpAnisotTextureSetMaxAnisotropy(RwTexture* tex, RwInt8 val) {
    static auto &RpAnisotTextureDataOffset = StaticRef<RwInt32>(0xC92478);
    if (tex != nullptr) {
        *(RwInt8 *)((RwUInt8 *)tex + RpAnisotTextureDataOffset) = val;
        return tex;
    }
    return nullptr;
}


RwInt8 RpAnisotTextureGetMaxAnisotropy(RwTexture* tex) {
    static auto &RpAnisotTextureDataOffset = StaticRef<RwInt32>(0xC92478);
    if (tex != nullptr) {
        return *(RwInt8 *)((RwUInt8 *)tex + RpAnisotTextureDataOffset);
    }
    return 0;
}


RwBool RpAnisotPluginAttach() {
    static auto &RpAnisotTextureDataOffset  = StaticRef<RwInt32>(0xC92478);
    static auto &RpAnisotPluginStreamResult = StaticRef<RwInt32>(0xC9247C);

    if (RwEngineRegisterPlugin(0, 0x127,
                               (RwPluginObjectConstructor)0x749000,
                               (RwPluginObjectDestructor)0x749010) < 0) {
        return FALSE;
    }
    RpAnisotTextureDataOffset = RwTextureRegisterPlugin(sizeof(RwInt8), 0x127,
                                                        (RwPluginObjectConstructor)0x749020,
                                                        (RwPluginObjectDestructor)0x749030,
                                                        (RwPluginObjectCopy)0x749040);
    if (RpAnisotTextureDataOffset < 0) {
        return FALSE;
    }
    RpAnisotPluginStreamResult = RwTextureRegisterPluginStream(0x127,
                                                               (RwPluginDataChunkReadCallBack)0x749060,
                                                               (RwPluginDataChunkWriteCallBack)0x7490B0,
                                                               (RwPluginDataChunkGetSizeCallBack)0x7490F0);
    if (RpAnisotPluginStreamResult < 0) {
        return FALSE;
    }
    ((void(__cdecl *)(RwInt32))0x7FDE60)(RpAnisotTextureDataOffset);
    return TRUE;
}

