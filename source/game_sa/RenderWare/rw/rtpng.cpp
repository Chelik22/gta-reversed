#include "StdInc.h"

#include "rtpng.h"

#include <setjmp.h>

typedef void* (*pngCreateWriteStruct2Fn)(const char*, void*, void*, void*, void*,
                                         void(__cdecl*)(void*, const char*),
                                         void(__cdecl*)(void*, const char*));
typedef void* (*pngCreateInfoStructFn)(void*);
typedef void  (*pngDestroyWriteStructFn)(void**, void**);
typedef void  (*pngSetWriteFnFn)(void*, void*, void(__cdecl*)(void*, unsigned char*, size_t),
                                 void(__cdecl*)(void*));
typedef void  (*pngSetIhdrFn)(void*, void*, RwUInt32, RwUInt32, RwInt32, RwInt32,
                              RwInt32, RwInt32, RwInt32);
typedef void  (*pngSetPlteFn)(void*, void*, void*, RwUInt32);
typedef void  (*pngSetTrnsFn)(void*, void*, void*, RwUInt32, void*);
typedef void  (*pngWriteInfoFn)(void*, void*);
typedef void  (*pngSetPackingFn)(void*);
typedef void  (*pngWriteImageFn)(void*, void**);
typedef void  (*pngWriteEndFn)(void*, void*);

RwImage* RtPNGImageWrite(RwImage* image, const RwChar* imageName) {
    if (imageName == NULL || image == NULL) {
        return image;
    }

    RwStream* stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMWRITE, imageName);
    if (stream == NULL) {
        return NULL;
    }

    void* png    = NULL;
    void* info   = NULL;
    void* bgrBuf = NULL;
    void* alphaBuf = NULL;

    png = ((pngCreateWriteStruct2Fn)0x7D03B0)("1.0.12", NULL, NULL, NULL, NULL,
                                              (void(__cdecl*)(void*, const char*))0x7CF940,
                                              (void(__cdecl*)(void*, const char*))0x7CF960);
    if (png == NULL) {
        RwStreamClose(stream, 0);
        return NULL;
    }
    info = ((pngCreateInfoStructFn)0x7D2E80)(png);
    if (info == NULL) {
        RwStreamClose(stream, 0);
        ((pngDestroyWriteStructFn)0x7D0A40)(&png, 0);
        return NULL;
    }

    if (setjmp(*reinterpret_cast<jmp_buf*>(png)) != 0) {
        RwStreamClose(stream, 0);
        ((pngDestroyWriteStructFn)0x7D0A40)(&png, 0);
        return NULL;
    }

    ((pngSetWriteFnFn)0x7D2C20)(png, stream,
                                (void(__cdecl*)(void*, unsigned char*, size_t))0x7CF980,
                                (void(__cdecl*)(void*))0x7CF9A0);

    const RwInt32 depth  = image->depth;    /* +0x0C */
    const RwInt32 height = image->height;   /* +0x08 */

    RwInt32 bitDepth  = 0;
    RwInt32 colorType = 0;
    RwRGBA* palette   = NULL;
    if (depth == 4) {
        palette   = image->palette;         /* +0x18 */
        bitDepth  = 4;
        colorType = 3;
    } else if (depth == 8) {
        bitDepth  = 8;
        palette   = image->palette;
        colorType = 3;
    } else if (depth == 0x20) {
        bitDepth  = 8;
        colorType = 6;
        palette   = NULL;
    }

    ((pngSetIhdrFn)0x7D1D60)(png, info, (RwUInt32)image->width, (RwUInt32)height,
                             bitDepth, colorType, 0, 0, 0);

    if (colorType == 3) {
        const RwInt32 entries = 1 << (depth & 0x1f);
        bgrBuf   = (*reinterpret_cast<RwMemoryFunctions*>(
            reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x134)).rwmalloc(
                (size_t)(entries * 3), 0x3010D);
        alphaBuf = (*reinterpret_cast<RwMemoryFunctions*>(
            reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x134)).rwmalloc(
                (size_t)entries, 0x3010D);
        const RwUInt8* pal = reinterpret_cast<const RwUInt8*>(palette);
        RwUInt8* bgr   = (RwUInt8*)bgrBuf;
        RwUInt8* alpha = (RwUInt8*)alphaBuf;
        for (RwInt32 k = 1; k <= entries; k++) {
            // RGBA палитры RW -> RGB-тройки PNG + альфа-таблица tRNS
            bgr[0]   = pal[k * 4 - 4];
            bgr[1]   = pal[k * 4 - 3];
            bgr[2]   = pal[k * 4 - 2];
            alpha[0] = pal[k * 4 - 1];
            bgr   += 3;
            alpha += 1;
        }
        ((pngSetPlteFn)0x7D2130)(png, info, bgrBuf, (RwUInt32)entries);
        ((pngSetTrnsFn)0x7D2630)(png, info, alphaBuf, (RwUInt32)entries, 0);
    }

    ((pngWriteInfoFn)0x7CFFA0)(png, info);
    ((pngSetPackingFn)0x7D1340)(png);

    void** rowPtrs = static_cast<void**>((*reinterpret_cast<RwMemoryFunctions*>(
        reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x134)).rwmalloc(
            (size_t)(height << 2), 0x3010D));
    if (rowPtrs != NULL) {
        RwUInt8* row = image->cpPixels;     /* +0x14 */
        const RwInt32 stride = image->stride;   /* +0x10 */
        for (RwInt32 i = 0; i < height; i++) {
            rowPtrs[i] = row;
            row += stride;
        }
        ((pngWriteImageFn)0x7D06C0)(png, rowPtrs);
        ((pngWriteEndFn)0x7D0250)(png, info);
        if (bgrBuf != NULL) {
            (*reinterpret_cast<void(__cdecl**)(void*)>(
                reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x138))(bgrBuf);
            (*reinterpret_cast<void(__cdecl**)(void*)>(
                reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x138))(alphaBuf);
        }
        (*reinterpret_cast<void(__cdecl**)(void*)>(
            reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x138))(rowPtrs);
        ((pngDestroyWriteStructFn)0x7D0A40)(&png, &info);
        RwStreamClose(stream, 0);
        return image;
    }

    // декомпил: bgrBuf/alphaBuf на этом пути НЕ освобождаются - смирено
    ((pngDestroyWriteStructFn)0x7D0A40)(&png, 0);
    RwStreamClose(stream, 0);
    return NULL;
}


typedef void*       (*pngCreateReadStruct2Fn)(const char*, void*, void*, void*, void*,
                                              void(__cdecl*)(void*, const char*),
                                              void(__cdecl*)(void*, const char*));
typedef void*       (*pngCreateInfoStructFn)(void*);
typedef void        (*pngDestroyReadStructFn)(void**, void**, void**);
typedef void        (*pngSetReadFnFn)(void*, void*, void(__cdecl*)(void*, unsigned char*, size_t));
typedef void        (*pngReadInfoFn)(void*, void*);
typedef void        (*pngGetIhdrFn)(void*, void*, RwUInt32*, RwUInt32*, RwInt32*, RwInt32*,
                                    void*, RwInt32*, RwInt32*);
typedef void        (*pngSetStrip16Fn)(void*);
typedef void        (*pngSetGray124To8Fn)(void*);
typedef void        (*pngSetFillerFn)(void*, RwUInt16, RwInt32);
typedef void        (*pngSetPackingFn)(void*);
typedef void        (*pngSetGrayToRgbFn)(void*);
typedef RwUInt32    (*pngGetValidFn)(void*, void*, RwUInt32);
typedef RwUInt32    (*pngGetPlteFn)(void*, void*, void**, RwUInt32*);
typedef RwUInt32    (*pngGetTrnsFn)(void*, void*, void**, RwUInt32*, void**);
typedef void        (*pngReadImageFn)(void*, void**);
typedef void        (*pngReadEndFn)(void*, void*);

RwImage* RtPNGImageRead(const RwChar* imageName) {
    if (imageName == NULL) {
        RwError rwError;
        rwError.pluginID = 0x10D;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000016);
        RwErrorSet(&rwError);
        return NULL;
    }

    RwStream* stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, imageName);
    if (stream == NULL) {
        return NULL;
    }

    void* png     = NULL;
    void* info    = NULL;
    void* rowPtrs = NULL;

    png = ((pngCreateReadStruct2Fn)0x7D36D0)("1.0.12", NULL, NULL, NULL, NULL,
                                             (void(__cdecl*)(void*, const char*))0x7CF940,
                                             (void(__cdecl*)(void*, const char*))0x7CF960);
    if (png == NULL) {
        RwStreamClose(stream, 0);
        return NULL;
    }
    info = ((pngCreateInfoStructFn)0x7D2E80)(png);
    if (info == NULL) {
        RwStreamClose(stream, 0);
        ((pngDestroyReadStructFn)0x7D4660)(&png, 0, 0);
        return NULL;
    }

    if (setjmp(*reinterpret_cast<jmp_buf*>(png)) != 0) {
        if (rowPtrs != NULL) {
            (*reinterpret_cast<void(__cdecl**)(void*)>(
                reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x138))(rowPtrs);
        }
        RwStreamClose(stream, 0);
        ((pngDestroyReadStructFn)0x7D4660)(&png, &info, 0);
        return NULL;
    }

    ((pngSetReadFnFn)0x7DA870)(png, stream,
                               (void(__cdecl*)(void*, unsigned char*, size_t))0x7CFDE0);
    ((pngReadInfoFn)0x7D3AB0)(png, info);

    RwUInt32 width, height;
    RwInt32  bitDepth, colorType;
    RwUInt8  interlaceOut[4];
    ((pngGetIhdrFn)0x7D5190)(png, info, &width, &height, &bitDepth, &colorType,
                             interlaceOut, 0, 0);
    ((pngSetStrip16Fn)0x7D5790)(png);

    RwInt32 imageDepth = 0;
    switch (colorType) {
    case 0:     // gray
        imageDepth = 8;
        if (bitDepth < 8) {
            ((pngSetGray124To8Fn)0x7D6000)(png);
        }
        break;
    case 2:     // RGB
        imageDepth = 32;
        ((pngSetFillerFn)0x7D13E0)(png, 0xff, 1);
        break;
    case 3:     // palette
        imageDepth = 8;
        if (bitDepth < 8) {
            ((pngSetPackingFn)0x7D1340)(png);
            imageDepth = 4;
        }
        break;
    case 4:     // gray + alpha
        imageDepth = 32;
        ((pngSetGrayToRgbFn)0x7D6020)(png);
        break;
    case 6:     // RGBA
        imageDepth = 32;
        break;
    }

    RwImage* image = RwImageCreate((RwInt32)width, (RwInt32)height, imageDepth);
    if (image != NULL) {
        if (RwImageAllocatePixels(image) == NULL) {
            RwImageDestroy(image);
            RwStreamClose(stream, 0);       // декомпил: png-структуры здесь НЕ разрушаются
            return NULL;
        }

        RwRGBA* palette = image->palette;       /* +0x18 */
        rowPtrs = (*reinterpret_cast<RwMemoryFunctions*>(
            reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x134)).rwmalloc(
                (size_t)((RwInt32)height << 2), 0x3010D);
        if (rowPtrs == NULL) {
            RwImageFreePixels(image);
            RwImageDestroy(image);
            RwStreamClose(stream, 0);
            return NULL;
        }

        RwUInt8* row = image->cpPixels;         /* +0x14 */
        const RwInt32 stride = image->stride;   /* +0x10 */
        for (RwUInt32 i = 0; i < height; i++) {
            ((void**)rowPtrs)[i] = row;
            row += stride;
        }

        if ((imageDepth == 4 || imageDepth == 8) && palette != NULL) {
            const RwInt32 entries = 1 << (imageDepth & 0x1f);
            // дефолтная серая шкала
            RwUInt8 v = 0;
            for (RwInt32 k = 0; k < entries; k++) {
                RwUInt8* e = reinterpret_cast<RwUInt8*>(palette) + k * 4;
                e[0] = v;
                e[1] = v;
                e[2] = v;
                e[3] = 0xFF;
                v = (RwUInt8)(v + 1);
            }
            if (((pngGetValidFn)0x7D4B80)(png, info, 8) != 0) {         // PNG_INFO_PLTE
                void*    plt;
                RwUInt32 num;
                ((pngGetPlteFn)0x7D5440)(png, info, &plt, &num);
                if ((RwInt32)num > entries) {
                    num = (RwUInt32)entries;
                }
                const RwUInt8* src = (const RwUInt8*)plt;
                for (RwUInt32 k = 0; k < num; k++) {
                    RwUInt8* e = reinterpret_cast<RwUInt8*>(palette) + k * 4;
                    e[0] = src[0];
                    e[1] = src[1];
                    e[2] = src[2];
                    src += 3;
                }
            }
            if (((pngGetValidFn)0x7D4B80)(png, info, 0x10) != 0) {      // PNG_INFO_tRNS
                void*    trans;
                RwUInt32 numTrans;
                void*    transVals;
                ((pngGetTrnsFn)0x7D5530)(png, info, &trans, &numTrans, &transVals);
                const RwUInt8* src = (const RwUInt8*)trans;
                for (RwUInt32 k = 0; k < numTrans; k++) {
                    reinterpret_cast<RwUInt8*>(palette)[k * 4 + 3] = src[k];
                }
            }
        }

        ((pngReadImageFn)0x7D4380)(png, (void**)rowPtrs);
        ((pngReadEndFn)0x7D43E0)(png, info);
        ((pngDestroyReadStructFn)0x7D4660)(&png, &info, 0);
        png = NULL;
        info = NULL;

        (*reinterpret_cast<void(__cdecl**)(void*)>(
            reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x138))(rowPtrs);
        rowPtrs = NULL;
    }

    RwStreamClose(stream, 0);
    return image;
}

