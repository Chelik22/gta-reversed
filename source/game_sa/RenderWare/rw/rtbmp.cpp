#include "StdInc.h"

#include "rtbmp.h"

RwImage* RtBMPImageWrite(RwImage* image, const RwChar* imageName) {
    RwStream* stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMWRITE, imageName);
    if (stream == NULL) {
        return NULL;
    }

    RwInt32 depth = (image->palette != NULL) ? image->depth : 24 /*0x18*/;

    RwStreamWrite(stream, "B", 1);      // 0x42
    RwStreamWrite(stream, "M", 1);      // 0x4D

    RwInt32 palBytes = (depth < 9) ? ((1 << (depth & 0x1f)) << 2) : 0;
    RwInt32 fileSize = (((image->width * depth + 0x1f) >> 3) & 0x1ffffffc) * image->height +
                       0x36 + palBytes;
    RwStreamWriteInt32(stream, &fileSize, 4);

    RwInt16 zero16 = 0;                 // reserved1, reserved2
    RwStreamWriteInt16(stream, &zero16, 2);
    RwStreamWriteInt16(stream, &zero16, 2);

    RwInt32 offBits = ((depth < 9) ? ((1 << (depth & 0x1f)) << 2) : 0) + 0x36;
    RwStreamWriteInt32(stream, &offBits, 4);

    RwInt32 hdrSize = 0x28;             // biSize
    RwStreamWriteInt32(stream, &hdrSize, 4);
    RwStreamWriteInt32(stream, &image->width, 4);
    RwStreamWriteInt32(stream, &image->height, 4);

    RwInt16 planes = 1;
    RwInt16 depth16 = (RwInt16)depth;
    RwStreamWriteInt16(stream, &planes, 2);
    RwStreamWriteInt16(stream, &depth16, 2);

    RwInt32 zero32 = 0;                 // compression, sizeImage, xppm, yppm
    RwStreamWriteInt32(stream, &zero32, 4);
    RwStreamWriteInt32(stream, &zero32, 4);
    RwStreamWriteInt32(stream, &zero32, 4);
    RwStreamWriteInt32(stream, &zero32, 4);

    if (depth < 9) {
        RwInt32 entries = 1 << (depth & 0x1f);
        RwStreamWriteInt32(stream, &entries, 4);    // biClrUsed
        RwStreamWriteInt32(stream, &entries, 4);    // biClrImportant
        const RwUInt8* pal = reinterpret_cast<const RwUInt8*>(image->palette) + 1;
        for (RwInt32 i = 0; i < entries; i++) {
            // 0x7CE990: байты записи = {palette[1], palette[2], palette[0], 0xFF} — зелёный в
            // слоте blue, синий в слоте green (квирк оригинала, сохранён)
            RwUInt32 entry = (RwUInt32)pal[0] |
                             ((RwUInt32)pal[1] << 8) |
                             ((RwUInt32)pal[-1] << 16) |
                             0xFF000000u;
            RwStreamWrite(stream, &entry, 4);
            pal += 4;
        }
    } else {
        RwStreamWriteInt32(stream, &zero32, 4);     // biClrUsed
        RwStreamWriteInt32(stream, &zero32, 4);     // biClrImportant
    }

    void* rowBuf = (*reinterpret_cast<RwMemoryFunctions*>(
        reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x134)).rwmalloc(
            (size_t)(image->width * 3), 0x10000);

    for (RwInt32 row = image->height - 1; row >= 0; row--) {
        ((void(__cdecl*)(RwStream*, RwImage*, RwInt32, RwInt32, void*))0x7CEC40)(
            stream, image, row, depth, rowBuf);
    }

    (*reinterpret_cast<void(__cdecl**)(void*)>(
        reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x138))(rowBuf);
    RwStreamClose(stream, 0);
    return image;
}


RwImage* RtBMPImageRead(const RwChar* imageName) {
    RwStream* stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, imageName);
    if (stream == NULL) {
        return NULL;
    }

    RwUInt32 magic;                     // bfType + младшее слово bfSize
    if (RwStreamRead(stream, &magic, 4) != 4) {
        RwStreamClose(stream, 0);
        return NULL;
    }
    if ((magic & 0xffff) != 0x4d42) {   // 'BM'
        RwStreamClose(stream, 0);
        return NULL;
    }

    RwUInt8 fileRest[14];               // bfSize.hi(2) res1(2) res2(2) bfOffBits(4) biSize(4)
    if (RwStreamRead(stream, fileRest, 14) != 14) {
        RwStreamClose(stream, 0);
        return NULL;
    }
    RwUInt32 dataOffset;                // bfOffBits
    RwInt32  dibSize;                   // biSize
    dataOffset = *(RwUInt32*)(fileRest + 6);
    dibSize    = *(RwInt32*)(fileRest + 10);

    RwInt32  width, height, compression;
    RwInt16  bitCount;
    RwInt32  clrUsed = 0;
    RwUInt8  dibBody[1024];             // стековое окно декомпила (&local_400, 0x400 байт)

    if (RwStreamRead(stream, dibBody, dibSize - 4) != (RwUInt32)(dibSize - 4)) {
        RwStreamClose(stream, 0);
        return NULL;
    }
    RwInt32 headerEnd = dibSize + 14;   // смещение данных от начала файла до чтения палитры

    if (dibSize == 12) {
        // BITMAPCOREHEADER: bcWidth(2) bcHeight(2) bcPlanes(2) bcBitCount(2)
        width       = (RwInt16)*(RwUInt16*)(dibBody + 0);
        height      = (RwInt16)*(RwUInt16*)(dibBody + 2);
        bitCount    = (RwInt16)*(RwUInt16*)(dibBody + 6);
        compression = 0;
    } else {
        width       = *(RwInt32*)(dibBody + 0);
        height      = *(RwInt32*)(dibBody + 4);
        bitCount    = (RwInt16)*(RwUInt16*)(dibBody + 10);
        compression = *(RwInt32*)(dibBody + 12);
        clrUsed     = *(RwInt32*)(dibBody + 28);
        if (compression == 2) {         // BI_RLE4 не поддерживается
            RwError rwError;
            rwError.pluginID = 0x19D;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000009);
            RwErrorSet(&rwError);
            RwStreamClose(stream, 0);
            return NULL;
        }
    }

    RwInt32 paletteEntries;
    if (dibSize == 12) {
        paletteEntries = 1 << (bitCount & 0x1f);
    } else if (clrUsed < 1 || (1 << (bitCount & 0x1f)) < clrUsed) {
        paletteEntries = 1 << (bitCount & 0x1f);
    } else {
        paletteEntries = clrUsed;
    }

    RwInt32 imageDepth;
    if (bitCount == 1 || bitCount == 4) {
        imageDepth = 4;
    } else if (bitCount == 8) {
        imageDepth = 8;
    } else {
        imageDepth = 32;
    }

    RwImage* image = RwImageCreate(width, height, imageDepth);
    if (image == NULL) {
        RwStreamClose(stream, 0);
        return NULL;
    }
    if (RwImageAllocatePixels(image) == NULL) {
        RwImageDestroy(image);
        RwStreamClose(stream, 0);
        return NULL;
    }

    RwUInt8 palBuf[1024];               // стековый буфер палитры декомпила (&local_400)
    if (bitCount < 9) {
        RwRGBA* palette = image->palette;
        RwUInt32 palBytes;
        if (dibSize == 12) {
            // OS/2: 3-байтовые записи BGR
            palBytes = paletteEntries * 3;
            if (RwStreamRead(stream, palBuf, palBytes) != palBytes) {
                RwStreamClose(stream, 0);           // декомпил: image здесь НЕ уничтожается
                return NULL;
            }
            headerEnd = (RwInt32)palBytes + 0x1a;
            for (RwInt32 i = 0; i < paletteEntries; i++) {
                palette[i].red   = palBuf[i * 3 + 2];
                palette[i].green = palBuf[i * 3 + 1];
                palette[i].blue  = palBuf[i * 3];
                palette[i].alpha = 0xFF;
            }
        } else {
            if (dibSize != 0x28) {
                RwError rwError;
                rwError.pluginID = 0x19D;
                rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000009);
                RwErrorSet(&rwError);
                RwStreamClose(stream, 0);           // декомпил: image здесь НЕ уничтожается
                return NULL;
            }
            // Windows: 4-байтовые записи BGRx
            palBytes = paletteEntries * 4;
            if (RwStreamRead(stream, palBuf, palBytes) != palBytes) {
                RwStreamClose(stream, 0);           // декомпил: image здесь НЕ уничтожается
                return NULL;
            }
            headerEnd = (RwInt32)palBytes + 0x36;
            for (RwInt32 i = 0; i < paletteEntries; i++) {
                palette[i].red   = palBuf[i * 4 + 2];
                palette[i].green = palBuf[i * 4 + 1];
                palette[i].blue  = palBuf[i * 4];
                palette[i].alpha = 0xFF;
            }
        }
    }

    if (RwStreamSkip(stream, dataOffset - headerEnd) == NULL) {
        RwImageDestroy(image);
        RwStreamClose(stream, 0);
        return NULL;
    }

    const RwInt32  bytesPerPixel = (bitCount + 7) >> 3;
    const RwUInt32 rowStride     = ((((RwUInt32)(bitCount * width + 7)) >> 3) + 3u) & 0xfffffffcu;

    // аллокация декомпила: (width+7)&~7 пикселей * байт-на-пиксель (кратно 8 пикселям, НЕ rowStride)
    RwUInt8* rowBuf = (RwUInt8*)(*reinterpret_cast<RwMemoryFunctions*>(
        reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x134)).rwmalloc(
            (size_t)(((width + 7) & ~7) * bytesPerPixel), 0x10000);
    if (rowBuf == NULL) {
        RwError rwError;
        rwError.pluginID = 0x19D;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(
            0x80000013, ((width + 7) & ~7) * bytesPerPixel);
        RwErrorSet(&rwError);
        RwImageDestroy(image);
        RwStreamClose(stream, 0);
        return NULL;
    }

    if (compression == 0 || bitCount == 24 || bitCount == 32) {
        // несжатые строки, снизу вверх
        for (RwInt32 row = height - 1; row >= 0; row--) {
            if (RwStreamRead(stream, rowBuf, rowStride) != rowStride) {
                (*reinterpret_cast<void(__cdecl**)(void*)>(
                    reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x138))(rowBuf);
                RwImageDestroy(image);
                RwStreamClose(stream, 0);
                return NULL;
            }
            ((void(__cdecl*)(RwImage*, RwInt32, void*, RwInt32, RwInt32))0x7CE5D0)(
                image, row, rowBuf, bitCount, 0);
        }
    } else if (compression == 1) {
        // BI_RLE8
        RwInt32 flushRow = 0;
        while (flushRow < height) {
            RwInt32  destPos = 0;
            RwUInt16 code;
            RwUInt8  esc = 0;
            RwBool   fail = FALSE;
            for (;;) {
                if (RwStreamRead(stream, &code, 2) != 2) {
                    fail = TRUE;                // LAB_007ce4e5
                    break;
                }
                esc = (RwUInt8)(code >> 8);
                if ((RwUInt8)code != 0) {
                    // закодированный прогон: count байт цвета esc
                    memset(rowBuf + destPos, esc, (RwUInt8)code);
                    destPos += (RwUInt8)code;
                    continue;
                }
                if (esc >= 3) {
                    // абсолютный прогон esc байт (+ паддинг при нечётном)
                    if (RwStreamRead(stream, rowBuf + destPos, esc) != esc) {
                        fail = TRUE;
                        break;
                    }
                    destPos += esc;
                    if ((esc & 1) != 0) {
                        if (RwStreamRead(stream, &code, 1) != 1) {
                            fail = TRUE;
                            break;
                        }
                    }
                    continue;
                }
                break;                          // esc 0/1/2
            }
            if (fail) {
                (*reinterpret_cast<void(__cdecl**)(void*)>(
                    reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x138))(rowBuf);
                RwImageDestroy(image);
                RwStreamClose(stream, 0);
                return NULL;
            }
            if (esc != 0) {
                if (esc != 1) {
                    // esc == 2 (delta) - не поддерживается, путь LAB_007ce4e5 (без error-рекорда)
                    (*reinterpret_cast<void(__cdecl**)(void*)>(
                        reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x138))(rowBuf);
                    RwImageDestroy(image);
                    RwStreamClose(stream, 0);
                    return NULL;
                }
                flushRow = height - 1;          // EOB
            }
            ((void(__cdecl*)(RwImage*, RwInt32, void*, RwInt32, RwInt32))0x7CE5D0)(
                image, (height - flushRow) - 1, rowBuf, bitCount, 1);
            flushRow++;
        }
    } else {
        RwError rwError;
        rwError.pluginID = 0x19D;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000009);
        RwErrorSet(&rwError);
        (*reinterpret_cast<void(__cdecl**)(void*)>(
            reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x138))(rowBuf);
        RwImageDestroy(image);
        RwStreamClose(stream, 0);
        return NULL;
    }

    (*reinterpret_cast<void(__cdecl**)(void*)>(
        reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x138))(rowBuf);
    RwStreamClose(stream, 0);
    return image;
}

