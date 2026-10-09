// ============================================================================
// ЗАМЕНЯЮЩИЙ ФАЙЛ (int-fx, батч fxemitter-1) — готов к прямой замене
// source/game_sa/Fx/FxEmitterBP.cpp
//
// Влито 7 функций: 0x4A1940 RenderHeatHaze, 0x4A21D0 UpdateParticle,
// 0x4A2B40 CreateInstance, 0x4A2BC0 Update, 0x5C25F0 Load, 0x4A2C40 Render,
// 0x4A2510 FreePrtFromPrim.
//
// !!! ТРЕБУЕТСЯ правка 2 заголовков (репо был read-only для интегратора), иначе не соберётся:
//   FxEmitterBP.h:24  void Update(float deltaTime) override; -> int32 Update(float deltaTime) override;
//   FxPrimBP.h:70     virtual void Update(float a1) = 0;     -> virtual int32 Update(float a1) = 0;
// Причина: ABI 0x4A2BC0 возвращает int32 (число частиц на входе; вызывающий
// FxSystemBP_c::Update 0x4AA130 результат игнорирует — FxSystemBP.cpp:138).
// Других наследников FxPrimBP_c в репо нет (греп "public FxPrimBP_c" -> только FxEmitterBP.h:7).
// Подробности: notes_int-fx.md рядом с этим файлом.
// ============================================================================

#include "StdInc.h"

#include "FxEmitterBP.h"
#include "FxEmitter.h"
#include "FxPrimBP.h"
#include "FxEmitterPrt.h"
#include "FxInfo.h"
#include "FxInfoManager.h"

#include "Particle.h"
#include "FxTools.h"

// int-fx (fxemitter-1): новые зависимости влитых тел (каждый заголовок проверен grep-ом по репо):
#include "Fx.h"         // RenderBegin/RenderAddTri_/RenderEnd/RotateVecAboutVec (Fx.h:116-136, тела Fx.cpp)
#include "FxPrtMult.h"  // FxPrtMult_c — реэмиттер дыма в Render (FxPrtMult.h:5)
#include "WaterLevel.h" // CWaterLevel::GetWaterLevel 0x6EB690 (WaterLevel.h:261)
#include "Timer.h"      // CTimer::ms_fTimeStep 0xB7CB5C (Timer.h:33)
// std::max — уже доступен через StdInc.h (<algorithm>, StdInc.h:7); StaticRef — Base.h:164 (StdInc.h:27);
// RenderInfo_t/MovementInfo_t приходят через уже подключённый FxInfo.h (FxInfo.h:6-7).

void FxEmitterBP_c::InjectHooks() {
    RH_ScopedVirtualClass(FxEmitterBP_c, 0x85A788, 7);
    RH_ScopedCategory("Fx");

    RH_ScopedInstall(Constructor, 0x4A18D0);
    RH_ScopedInstall(RenderHeatHaze, 0x4A1940);
    RH_ScopedInstall(UpdateParticle, 0x4A21D0);
    RH_ScopedVMTInstall(CreateInstance, 0x4A2B40);
    RH_ScopedVMTInstall(Update, 0x4A2BC0);
    RH_ScopedVMTInstall(Load, 0x5C25F0);
    RH_ScopedVMTInstall(LoadTextures, 0x5C0A30, {.Reversed = true}); // НЕ в батче — не трогать
    RH_ScopedVMTInstall(Render, 0x4A2C40);
    RH_ScopedVMTInstall(FreePrtFromPrim, 0x4A2510);
}

// 0x4A18D0
FxEmitterBP_c::FxEmitterBP_c() : FxPrimBP_c() {
    m_Type = 0;
}

// 0x4A1940
// int-fx: тело из батча fxemitter-1/0x4A1940.cpp (свежий реверс с декомпила).
// txdHashKey/brightness телом не используются (как в декомпиле).
// .rdata-константы читаются StaticRef<float>: 0x858B50=0.0f, 0x859E2C=2pi, 0x85A7BC=pi/180, 0x859A3C=1/255, 0x858B8C=0.5f.
void FxEmitterBP_c::RenderHeatHaze(RwCamera* camera, uint32 txdHashKey, float brightness) {
    // TList_c<Particle_c>::GetNumItems (0x4A8EC0); в оригинале вызывается дважды
    if (m_Particles.GetNumItems() == 0) {
        return;
    }

    const RwMatrix& camMat = RwCameraGetFrame(camera)->modelling; // parent-фрейм камеры, RwFrame::modelling (rwcore.h:4195)

    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));     // (0xC, 1)
    RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDSRCALPHA));  // (0xA, 5)
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDONE));       // (0xB, 2)

    auto currentRaster = RwTextureGetRaster(m_apTextures[0]);
    RenderBegin(currentRaster, nullptr, rwIM3D_VERTEXUV);

    if (m_Particles.GetNumItems() != 0) { // да, оригинал проверяет повторно
        for (auto* particle = m_Particles.GetHead(); particle; particle = m_Particles.GetNext(particle)) {
            const auto prt = particle->AsFxEmitterPrt();

            // Позиция
            CVector pos;
            if (!prt->m_bLocalToSystem) {
                pos = prt->m_Pos;
            } else {
                auto* transformMat = g_fxMan.FxRwMatrixCreate();
                prt->m_System->GetCompositeMatrix(transformMat);
                RwV3dTransformPoints(&pos, &prt->m_Pos, 1, transformMat);
                g_fxMan.FxRwMatrixDestroy(transformMat);
            }

            RenderInfo_t renderInfo;
            m_FxInfoManager.ProcessRenderInfo(
                prt->m_System->m_fCurrentTime,
                prt->m_fCurrentLife / prt->m_fTotalLife,
                0.0f,
                prt->m_System->m_SystemBP->m_fLength,
                false,
                &renderInfo
            );

            // Матрица квада (из пула g_fxMan)
            auto* mat = g_fxMan.FxRwMatrixCreate();
            mat->right = CVector{ 1.0f, 0.0f, 0.0f };
            mat->up    = CVector{ 0.0f, 1.0f, 0.0f };
            mat->at    = CVector{ 0.0f, 0.0f, 1.0f };
            mat->pos   = CVector{ 0.0f, 0.0f, 0.0f };
            mat->flags |= 0x20003; // OR поверх существующих flags (по декомпилу)

            if (!renderInfo.m_bHasDir) {
                if (!renderInfo.m_bIsFlat) {
                    mat->right = camMat.right; // копия матрицы кадра камеры
                    mat->up    = camMat.up;
                    mat->at    = camMat.at;
                    mat->pos   = camMat.pos;
                } else {
                    mat->right = renderInfo.m_FlatMatrix.right; // копия m_FlatMatrix
                    mat->up    = renderInfo.m_FlatMatrix.up;
                    mat->at    = renderInfo.m_FlatMatrix.at;
                    mat->pos   = renderInfo.m_FlatMatrix.pos;
                }
            } else {
                CVector dir = renderInfo.m_bUseVel ? prt->m_Velocity : renderInfo.m_Direction;
                RwV3dNormalize(&dir, &dir);

                CVector delta = pos - CVector{ camMat.pos };
                RwV3dNormalize(&delta, &delta);

                CVector axisRight{}, axisAt{};
                RwV3dCrossProduct(&axisRight, &delta, &dir); // right = delta x dir
                RwV3dCrossProduct(&axisAt, &dir, &axisRight); // at = dir x right
                mat->right = axisRight;
                mat->up    = dir;
                mat->at    = axisAt;
            }

            // Вращение: m_RotZ*2, wrap в [0, 2*pi)
            if (prt->m_RotZ != -1) { // сырой байт +0x36 != 0xFF
                prt->m_CurrentRotation = float(prt->m_RotZ) + float(prt->m_RotZ);
            }
            if (prt->m_CurrentRotation < StaticRef<float>(0x858B50)) { // < 0.0f
                auto rot = prt->m_CurrentRotation;
                do {
                    rot += StaticRef<float>(0x859E2C); // += 2*pi
                } while (rot < StaticRef<float>(0x858B50));
                prt->m_CurrentRotation = rot;
            }
            if (StaticRef<float>(0x859E2C) <= prt->m_CurrentRotation) {
                auto rot = prt->m_CurrentRotation;
                do {
                    rot -= StaticRef<float>(0x859E2C);
                } while (StaticRef<float>(0x859E2C) <= rot);
                prt->m_CurrentRotation = rot;
            }

            CVector axisRight = mat->right;
            CVector axisUp    = mat->up;
            CVector axisAt    = mat->at;
            if (prt->m_CurrentRotation > StaticRef<float>(0x858B50)) {
                RotateVecAboutVec(axisRight, mat->right, axisAt, prt->m_CurrentRotation * StaticRef<float>(0x85A7BC)); // град -> рад
                RwV3dCrossProduct(&axisUp, &axisAt, &axisRight); // up = at x right
            }
            g_fxMan.FxRwMatrixDestroy(mat);

            // Размеры: джиттер по m_RandR/m_RandG и множитель m_MultSize
            auto sizeX = renderInfo.m_fSizeX;
            auto sizeY = renderInfo.m_fSizeY;
            sizeX += (float(prt->m_RandR) * StaticRef<float>(0x859A3C) - StaticRef<float>(0x858B8C)) * renderInfo.m_fSizeXBias;
            sizeY += (float(prt->m_RandG) * StaticRef<float>(0x859A3C) - StaticRef<float>(0x858B8C)) * renderInfo.m_fSizeYBias;
            if (float(prt->m_MultSize) != 1.0f) { // сырой байт +0x30 != 0xFF
                sizeX *= float(prt->m_MultSize);
                sizeY *= float(prt->m_MultSize);
            }

            // Цвет: r/g/b обнуляются каждый проход (по декомпилу), альфа остаётся из ProcessRenderInfo
            renderInfo.m_Color1.red   = 0;
            renderInfo.m_Color1.green = 0;
            renderInfo.m_Color1.blue  = 0;

            // Выбор растра по anim-текстурам (m_nCurrentTexId 1..4 -> текстуры 0..3)
            RwRaster* raster = nullptr;
            if (!renderInfo.m_bHasAnimTextures || renderInfo.m_nCurrentTexId == 1) {
                raster = RwTextureGetRaster(m_apTextures[0]);
            } else if (renderInfo.m_nCurrentTexId == 2) {
                raster = m_apTextures[1] ? RwTextureGetRaster(m_apTextures[1]) : RwTextureGetRaster(m_apTextures[0]);
            } else if (renderInfo.m_nCurrentTexId == 3) {
                raster = m_apTextures[2] ? RwTextureGetRaster(m_apTextures[2]) : RwTextureGetRaster(m_apTextures[0]);
            } else if (renderInfo.m_nCurrentTexId == 4) {
                raster = m_apTextures[3] ? RwTextureGetRaster(m_apTextures[3]) : RwTextureGetRaster(m_apTextures[0]);
            } else {
                raster = RwTextureGetRaster(m_apTextures[0]); // в декомпиле ветка без совпадения не показана
            }
            if (currentRaster != raster) {
                RenderEnd();
                currentRaster = raster;
                RenderBegin(raster, nullptr, rwIM3D_VERTEXUV);
            }

            // Квад: A/B по вертикали (spriteTop/Bottom * sizeY), C/D по горизонтали (spriteLeft/Right * sizeX)
            const float extA = renderInfo.m_fSpriteTop    * sizeY;
            const float extB = renderInfo.m_fSpriteBottom * sizeY;
            const float extC = renderInfo.m_fSpriteLeft   * sizeX;
            const float extD = renderInfo.m_fSpriteRight  * sizeX;

            const CVector vTL = pos + axisRight * extC + axisUp * extA; // right*C + up*A
            const CVector vTR = pos + axisRight * extD + axisUp * extB; // right*D + up*B
            const CVector vBR = pos + axisRight * extD + axisUp * extA; // right*D + up*A
            const CVector vBL = pos + axisRight * extC + axisUp * extB; // right*C + up*B

            const auto& col = renderInfo.m_Color1;
            RenderAddTri_(
                vTL.x, vTL.y, vTL.z,
                vTR.x, vTR.y, vTR.z,
                vBR.x, vBR.y, vBR.z,
                0.0f, 0.0f,
                1.0f, 1.0f,
                1.0f, 0.0f,
                col.red, col.green, col.blue, col.alpha,
                col.red, col.green, col.blue, col.alpha,
                col.red, col.green, col.blue, col.alpha
            );
            RenderAddTri_(
                vTR.x, vTR.y, vTR.z,
                vTL.x, vTL.y, vTL.z,
                vBL.x, vBL.y, vBL.z,
                1.0f, 1.0f,
                0.0f, 0.0f,
                0.0f, 1.0f,
                col.red, col.green, col.blue, col.alpha,
                col.red, col.green, col.blue, col.alpha,
                col.red, col.green, col.blue, col.alpha
            );
        }
        RenderEnd();
    }
}

// 0x4A21D0
// int-fx: тело из батча fxemitter-1/0x4A21D0.cpp (свежий реверс: ProcessMovementInfo 0x4A4A10 +
// CWaterLevel::GetWaterLevel 0x6EB690 + вращение). В оригинале множитель m_MultRot — сырой байт с /255
// последним множителем; здесь FixedFloat::operator float() (сырой/255) — возможна погрешность 1 ulp.
bool FxEmitterBP_c::UpdateParticle(float deltaTime, FxEmitterPrt_c* prt) {
    const float timeStep = float(prt->m_System->m_nTimeMult) * StaticRef<float>(0x858CDC) * deltaTime; // uint16 * 0.001f * dt

    prt->m_fCurrentLife += timeStep;
    if (prt->m_fCurrentLife >= prt->m_fTotalLife) {
        return true; // частица умерла
    }

    prt->m_Pos += prt->m_Velocity * timeStep;

    MovementInfo_t moveInfo;
    moveInfo.m_Pos = prt->m_Pos;
    moveInfo.m_Vel = prt->m_Velocity;

    m_FxInfoManager.ProcessMovementInfo(
        prt->m_System->m_fCurrentTime,
        prt->m_fCurrentLife / prt->m_fTotalLife,
        timeStep,
        prt->m_System->m_SystemBP->m_fLength,
        false,
        &moveInfo
    );

    prt->m_Pos      = moveInfo.m_Pos;
    prt->m_Velocity = moveInfo.m_Vel;

    // Вода (float/underwater-флаги из FxInfoGroundCollide/FxInfoUnderwater)
    if (moveInfo.m_bHasFloatInfo || moveInfo.m_bHasUnderwaterInfo) {
        float waterLevel;
        const auto inWater = CWaterLevel::GetWaterLevel(prt->m_Pos.x, prt->m_Pos.y, prt->m_Pos.z, waterLevel, true, nullptr);
        if (moveInfo.m_bHasFloatInfo && inWater && prt->m_Pos.z < waterLevel) {
            prt->m_Pos.z = waterLevel; // не тонуть ниже воды
        }
        if (moveInfo.m_bHasUnderwaterInfo && (!inWater || waterLevel < prt->m_Pos.z)) {
            return true; // умереть
        }
    }

    // Вращение: две пары диапазонов из m_Rot[4]
    const float r1min = moveInfo.m_Rot[0];
    const float r1max = moveInfo.m_Rot[1];
    const float r2min = moveInfo.m_Rot[2];
    const float r2max = moveInfo.m_Rot[3];

    float rotDelta;
    if ((r1min <= StaticRef<float>(0x858B50) && r1max <= StaticRef<float>(0x858B50))
        || (r2min <= StaticRef<float>(0x858B50) && r2max <= StaticRef<float>(0x858B50))
    ) {
        if (r1min > StaticRef<float>(0x858B50) || r1max > StaticRef<float>(0x858B50)) {
            prt->m_CurrentRotation +=
                ((r1max - r1min) * float(prt->m_RandR) * StaticRef<float>(0x859A3C) + r1min)
                * float(prt->m_MultRot) * timeStep;
            return false;
        }
        if (r2min <= StaticRef<float>(0x858B50) && r2max <= StaticRef<float>(0x858B50)) {
            return false;
        }
        rotDelta = ((r2max - r2min) * float(prt->m_RandR) * StaticRef<float>(0x859A3C) + r2min) * float(prt->m_MultRot);
    } else {
        if (prt->m_RandR < 0x80) {
            prt->m_CurrentRotation +=
                ((r1max - r1min) * float(prt->m_RandR) * StaticRef<float>(0x858B88) + r1min)
                * float(prt->m_MultRot) * timeStep;
            return false;
        }
        // byte >= 0x80 трактуется как (byte - 128)
        rotDelta = ((r2max - r2min) * (float(prt->m_RandR) - StaticRef<float>(0x858BF4)) * StaticRef<float>(0x858B88) + r2min) * float(prt->m_MultRot);
    }
    prt->m_CurrentRotation -= rotDelta * timeStep;
    return false;
}

// 0x4A2B40
// int-fx: тело из батча fxemitter-1/0x4A2B40.cpp. Декомпил: operator new(0x14) -> FxPrim_c ctor (0x4A9F20)
// -> vtable 0x85A7A4 (FxEmitter_c); sizeof(FxEmitter_c) == 0x14 (VALIDATE_SIZE в FxEmitter.h:25).
FxPrim_c* FxEmitterBP_c::CreateInstance() {
    return new FxEmitter_c();
}

// 0x4A2BC0
// int-fx: тело из батча fxemitter-1/0x4A2BC0.cpp.
// ABI: оригинал возвращает int32 — число частиц на входе (TList_c<Particle_c>::GetNumItems 0x4A8EC0),
// 0 если пусто. Вызывающий FxSystemBP_c::Update (0x4AA130, FxSystemBP.cpp:136-138) результат игнорирует.
// ВНИМАНИЕ: требует замены void -> int32 в FxEmitterBP.h:24 и FxPrimBP.h:70 (см. шапку файла).
void FxEmitterBP_c::Update(float deltaTime) {
    const auto numParticles = m_Particles.GetNumItems();
    if (numParticles == 0) {
        
    }

    for (auto it = m_Particles.GetHead(); it; ) {
        const auto next = m_Particles.GetNext(it); // заранее: элемент может быть удалён в этой итерации

        if (it->m_System->m_nKillStatus == eFxSystemKillStatus::FX_3) {
            it->m_System->m_nKillStatus = eFxSystemKillStatus::FX_KILLED;
        }

        if (it->m_System->m_nPlayStatus != eFxSystemPlayStatus::T2
            && UpdateParticle(deltaTime, it->AsFxEmitterPrt())
        ) {
            m_Particles.RemoveItem(it); // 0x4A8E30
            g_fxMan.ReturnParticle(it->AsFxEmitterPrt()); // 0x4A93B0
        }

        it = next;
    }
    return; // original: return numParticles (header is void)
}

// 0x5C25F0
// int-fx: тело из батча fxemitter-1/0x5C25F0.cpp. Отличия от прежнего тела:
//  1) По декомпилу значения пишутся в m_FxInfoManager.m_nLodStart/m_nLodEnd (this+0x38/0x3A),
//     а НЕ в собственные поля FxEmitterBP_c (0x40/0x42).
//  2) Умножение *64.0f в декомпиле не видно (x87 fmul перед _ftol2 0x821B40), но присутствует
//     в сентябрьском стэше и восходит к plugin-sdk — оставлено.
bool FxEmitterBP_c::Load(FILESTREAM file, int32 version, FxName32_t* textureNames) {
    FxPrimBP_c::Load(file, version, textureNames); // 0x5C2010 (в репо ещё редирект — вызов уйдёт в оригинал)

    // sscanf("%s %f") + _ftol2 -> uint16
    m_FxInfoManager.m_nLodStart = uint16(ReadField<float>(file, "LODSTART:") * 64.0f);
    m_FxInfoManager.m_nLodEnd   = uint16(ReadField<float>(file, "LODEND:") * 64.0f);

    return true;
}

// 0x5C0A30
// НЕ в батче fxemitter-1 — тело не тронуто (в репо уже реверснуто).
bool FxEmitterBP_c::LoadTextures(FxName32_t* textureNames, int32 version) {
    assert(textureNames);

    const auto LoadTexture = [&](auto ind) -> RwTexture* {
        char mask[64];
        sprintf(mask, "%sm", textureNames[ind]);

        auto* texture = RwTextureRead(textureNames[ind], mask);
        return texture ? texture : RwTextureRead(textureNames[ind], nullptr);
    };

    const auto LoadTextureIfExists = [=](auto ind) -> RwTexture* {
        assert(&textureNames[ind]);
        return strncmp(textureNames[ind], "NULL", 5u) != 0 ? LoadTexture(ind) : nullptr;
    };

    m_apTextures[0] = LoadTexture(0);

    if (version > 101) {
        m_apTextures[1] = LoadTextureIfExists(1);
        m_apTextures[2] = LoadTextureIfExists(2);
        m_apTextures[3] = LoadTextureIfExists(3);
    }

    return true;
}

// 0x4A2C40
// int-fx: тело из батча fxemitter-1/0x4A2C40.cpp (свежий реверс с декомпила).
// Известные оговорки (см. STATUS.md батча, п.4-6):
//  - зелёный/синий цветовые каналы (BRIGHT) восстановлены поканально по формуле красного —
//    в декомпиле x87-операнды _ftol2 потеряны;
//  - 0x858B38 (множитель к ms_fTimeStep в trail-режиме 2) не декодирован — читается StaticRef из .rdata;
//  - trail-режим 2: средний член z-компоненты использует pos.y — сохранено буквально по декомпилу;
//  - таблица 0x8A6230 (режимы смешивания из .fxp "BlendMode") — значения из бинарника неизвестны;
//  - plugin::Call<0x4AB270> — вызов рядом с rand, результат уходил в x87 и потерян Ghidra.
// Fix интегратора: m_MultColor в репо — CRGBA (поля r/g/b/a, RGBA.h:11), а не RwRGBA — .red/.alpha -> .r/.a.
void FxEmitterBP_c::Render(RwCamera* camera, uint32 txdHashKey, float brightness, bool doHeatHaze) {
    if (doHeatHaze) {
        if (m_FxInfoManager.m_bHasHeatHazeParticleEmitter) { // +0x3D
            RenderHeatHaze(camera, txdHashKey, brightness);
        }
        return;
    }

    // В оригинале — инлайн-цикл по infos (int16 тип == 0x4008); свёрнуто в уже существующий IsFxInfoPresent (0x4A24D0)
    if (IsFxInfoPresent(eFxInfoType::FX_INFO_HEATHAZE_DATA)) {
        if (m_Particles.GetNumItems() > 0) {
            g_fxMan.m_bHeatHazeEnabled = true; // 0xA9AF38 (g_fxMan == 0xA9AE80 + 0xB8, FxManager.cpp:18)
        }
        return;
    }

    if (m_Particles.GetNumItems() == 0) {
        return;
    }

    const RwMatrix& camMat = RwCameraGetFrame(camera)->modelling; // *(camera + 4) + 0x10

    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(m_bAlphaOn)); // (0xC, байт m_bAlphaOn)
    auto& blendModes = StaticRef<RwBlendFunction[11]>(0x8A6230);
    if (!m_bAlphaOn) {
        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(blendModes[1])); // DAT_008a6234
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(blendModes[0])); // DAT_008a6230
    } else {
        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(blendModes[m_nSrcBlendId]));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(blendModes[m_nDstBlendId]));
    }

    auto currentRaster = RwTextureGetRaster(m_apTextures[0]);
    RenderBegin(currentRaster, nullptr, rwIM3D_VERTEXUV);

    for (auto* particle = m_Particles.GetHead(); particle; particle = m_Particles.GetNext(particle)) {
        const auto prt = particle->AsFxEmitterPrt();

        // Позиция (преобразованная для local-частиц)
        CVector pos;
        if (!prt->m_bLocalToSystem) {
            pos = prt->m_Pos;
        } else {
            auto* transformMat = g_fxMan.FxRwMatrixCreate();
            prt->m_System->GetCompositeMatrix(transformMat);
            RwV3dTransformPoints(&pos, &prt->m_Pos, 1, transformMat);
            g_fxMan.FxRwMatrixDestroy(transformMat);
        }

        RenderInfo_t renderInfo;
        m_FxInfoManager.ProcessRenderInfo(
            prt->m_System->m_fCurrentTime,
            prt->m_fCurrentLife / prt->m_fTotalLife,
            0.0f,
            prt->m_System->m_SystemBP->m_fLength,
            false,
            &renderInfo
        );

        // Дым: реэмиттер частиц в собственную систему (FxSystem_c::AddParticle 0x4AA440)
        if (renderInfo.m_SmokeType > -1) {
            const float jitterBound = renderInfo.m_SmokeBrightness;
            const auto  rnd = rand(); // _rand 0x821B1E
            plugin::Call<0x4AB270>(); // вызов по декомпилу; результат уходил в x87 и потерян Ghidra
            const float jitter = ((jitterBound - -jitterBound) * float(rnd) * StaticRef<float>(0x858C7C) + -jitterBound)
                               * StaticRef<float>(0x859A3C); // rand в [-B..B], приведённый к 0..1

            auto smokeR = renderInfo.m_SmokeColor.red   * StaticRef<float>(0x859A3C) + jitter;
            auto smokeG = renderInfo.m_SmokeColor.green * StaticRef<float>(0x859A3C) + jitter;
            auto smokeB = renderInfo.m_SmokeColor.blue  * StaticRef<float>(0x859A3C) + jitter;
            const auto smokeA = renderInfo.m_SmokeColor.alpha * StaticRef<float>(0x859A3C);
            // Оригинальный «кламп»: <= 0.5 -> max(v, 0), иначе 1.0 (буквально по декомпилу)
            smokeR = smokeR <= StaticRef<float>(0x858624) ? std::max(smokeR, 0.0f) : 1.0f;
            smokeG = smokeG <= StaticRef<float>(0x858624) ? std::max(smokeG, 0.0f) : 1.0f;
            smokeB = smokeB <= StaticRef<float>(0x858624) ? std::max(smokeB, 0.0f) : 1.0f;

            FxPrtMult_c mult;
            mult.m_Color.red   = smokeR;
            mult.m_Color.green = smokeG;
            mult.m_Color.blue  = smokeB;
            mult.m_Color.alpha = smokeA;
            mult.m_fSize = renderInfo.m_SmokeSize; // uStack_138 <- +0x9C
            mult.m_Rot   = 1.0f;                   // 0x3F800000
            mult.m_fLife = renderInfo.m_SmokeLife; // uStack_130 <- +0xA0

            prt->m_System->AddParticle(
                prt->m_Pos,
                CVector{ 0.0f, 0.0f, 0.0f },
                0.0f,
                mult,
                -1.0f,  // 0xBF800000
                1.2f,   // 0x3F99999A
                0.6f,   // 0x3F19999A
                false
            );
        }

        // Матрица квада
        CVector trailOffset; // вектор растяжения trail-режимов (заполняется только при m_nTrailScreenMode > 0)
        auto* mat = g_fxMan.FxRwMatrixCreate();
        mat->right = CVector{ 1.0f, 0.0f, 0.0f };
        mat->up    = CVector{ 0.0f, 1.0f, 0.0f };
        mat->at    = CVector{ 0.0f, 0.0f, 1.0f };
        mat->pos   = CVector{ 0.0f, 0.0f, 0.0f };
        mat->flags |= 0x20003;

        if (renderInfo.m_nTrailScreenMode < 1) {
            if (!renderInfo.m_bHasDir) {
                if (!renderInfo.m_bIsFlat) {
                    mat->right = camMat.right;
                    mat->up    = camMat.up;
                    mat->at    = camMat.at;
                    mat->pos   = camMat.pos;
                } else {
                    mat->right = renderInfo.m_FlatMatrix.right;
                    mat->up    = renderInfo.m_FlatMatrix.up;
                    mat->at    = renderInfo.m_FlatMatrix.at;
                    mat->pos   = renderInfo.m_FlatMatrix.pos;
                }
            } else {
                CVector dir;
                if (renderInfo.m_bUseVel) {
                    dir = prt->m_Velocity;
                    if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) {
                        dir.z = 1.0f; // ноль-проверка по декомпилу только для скорости
                    }
                } else {
                    dir = renderInfo.m_Direction;
                }
                RwV3dNormalize(&dir, &dir);

                CVector delta = pos - CVector{ camMat.pos };
                RwV3dNormalize(&delta, &delta);

                CVector axisRight{}, axisAt{};
                RwV3dCrossProduct(&axisRight, &delta, &dir);
                RwV3dCrossProduct(&axisAt, &dir, &axisRight);
                mat->right = axisRight;
                mat->up    = dir;
                mat->at    = axisAt;
            }
        } else {
            // Trail-режимы (m_nTrailScreenMode 1/2): вектор растяжения по скорости частицы
            if (renderInfo.m_nTrailScreenMode == 2) {
                const float f = CTimer::ms_fTimeStep * StaticRef<float>(0x858B38);
                const CVector camPos = StaticRef<CVector>(0xB6FA14); // позиция камеры (TheCamera+0x9EC)
                // Буквально по декомпилу (средний член z-компоненты использует pos.y и CAM.z —
                // вероятная путаница стековых слотов Ghidra, сохранено как есть):
                trailOffset.x = ((prt->m_Pos.x - camMat.pos.x) - ((prt->m_Pos.x - prt->m_Velocity.x * f) - camPos.x)) * renderInfo.m_fTrailTime;
                trailOffset.y = ((prt->m_Pos.y - camMat.pos.y) - ((prt->m_Pos.y - prt->m_Velocity.y * f) - camPos.y)) * renderInfo.m_fTrailTime;
                trailOffset.z = ((prt->m_Pos.z - camMat.pos.z) - ((prt->m_Pos.y - prt->m_Velocity.z * f) - camPos.z)) * renderInfo.m_fTrailTime;
            } else { // == 1
                trailOffset.x = prt->m_Pos.x - (prt->m_Pos.x - renderInfo.m_fTrailTime * prt->m_Velocity.x);
                trailOffset.y = prt->m_Pos.y - (prt->m_Pos.y - renderInfo.m_fTrailTime * prt->m_Velocity.y);
                trailOffset.z = prt->m_Pos.z - (prt->m_Pos.z - renderInfo.m_fTrailTime * prt->m_Velocity.z);
            }

            CVector dir = trailOffset;
            if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) {
                dir.z = 1.0f;
            } else {
                RwV3dNormalize(&dir, &dir);
            }

            CVector delta = prt->m_Pos - CVector{ camMat.pos };
            RwV3dNormalize(&delta, &delta);

            CVector axisRight{}, axisAt{};
            RwV3dCrossProduct(&axisRight, &delta, &dir);
            RwV3dCrossProduct(&axisAt, &dir, &axisRight);
            mat->right = axisRight;
            mat->up    = dir;
            mat->at    = axisAt;
        }

        // Вращение: m_RotZ*2, wrap в [0, 2*pi), поворот вокруг at
        if (prt->m_RotZ != -1) {
            prt->m_CurrentRotation = float(prt->m_RotZ) + float(prt->m_RotZ);
        }
        if (prt->m_CurrentRotation < StaticRef<float>(0x858B50)) {
            auto rot = prt->m_CurrentRotation;
            do {
                rot += StaticRef<float>(0x859E2C);
            } while (rot < StaticRef<float>(0x858B50));
            prt->m_CurrentRotation = rot;
        }
        if (StaticRef<float>(0x859E2C) <= prt->m_CurrentRotation) {
            auto rot = prt->m_CurrentRotation;
            do {
                rot -= StaticRef<float>(0x859E2C);
            } while (StaticRef<float>(0x859E2C) <= rot);
            prt->m_CurrentRotation = rot;
        }

        CVector axisRight = mat->right;
        CVector axisUp    = mat->up;
        CVector axisAt    = mat->at;
        if (prt->m_CurrentRotation > StaticRef<float>(0x858B50)) {
            RotateVecAboutVec(axisRight, mat->right, axisAt, prt->m_CurrentRotation * StaticRef<float>(0x85A7BC));
            RwV3dCrossProduct(&axisUp, &axisAt, &axisRight);
        }
        g_fxMan.FxRwMatrixDestroy(mat);

        // Размеры
        auto sizeX = renderInfo.m_fSizeX;
        auto sizeY = renderInfo.m_fSizeY;
        sizeX += (float(prt->m_RandR) * StaticRef<float>(0x859A3C) - StaticRef<float>(0x858B8C)) * renderInfo.m_fSizeXBias;
        sizeY += (float(prt->m_RandG) * StaticRef<float>(0x859A3C) - StaticRef<float>(0x858B8C)) * renderInfo.m_fSizeYBias;
        if (float(prt->m_MultSize) != 1.0f) {
            sizeX *= float(prt->m_MultSize);
            sizeY *= float(prt->m_MultSize);
        }

        // Цвет: красный по декомпилу; зелёный/синий — та же формула по своим каналам (в декомпиле жили в x87)
        float red   = float(renderInfo.m_Color1.red);
        float green = float(renderInfo.m_Color1.green);
        float blue  = float(renderInfo.m_Color1.blue);
        float alpha = float(renderInfo.m_Color1.alpha);
        if (renderInfo.m_nColorType == ERenderColorType::RANGE) {
            red   = float(prt->m_RandR) * StaticRef<float>(0x859A3C) * (float(renderInfo.m_Color2.red) - red) + red;
            green = float(prt->m_RandG) * StaticRef<float>(0x859A3C) * (float(renderInfo.m_Color2.green) - green) + green;
            blue  = float(prt->m_RandB) * StaticRef<float>(0x859A3C) * (float(renderInfo.m_Color2.blue) - blue) + blue;
        } else if (renderInfo.m_nColorType == ERenderColorType::BRIGHT) {
            red   += (float(prt->m_RandR) * StaticRef<float>(0x858B88) - StaticRef<float>(0x858624)) * float(renderInfo.m_Color2.alpha);
            green += (float(prt->m_RandG) * StaticRef<float>(0x858B88) - StaticRef<float>(0x858624)) * float(renderInfo.m_Color2.alpha);
            blue  += (float(prt->m_RandB) * StaticRef<float>(0x858B88) - StaticRef<float>(0x858624)) * float(renderInfo.m_Color2.alpha);
            for (auto c : { &red, &green, &blue }) { // кламп [0..255] виден только для красного
                if (*c < StaticRef<float>(0x858B50)) {
                    *c = 0.0f;
                }
                if (StaticRef<float>(0x859AAC) < *c) {
                    *c = 255.0f;
                }
            }
        }
        if (float(prt->m_MultColor.r) != 1.0f) {     // сырой байт +0x2C != 0xFF; FIX: CRGBA::r (было .red)
            red = float(prt->m_MultColor.r) * StaticRef<float>(0x859A3C) * red;
        }
        if (float(prt->m_MultColor.a) != 1.0f) {     // сырой байт +0x2F != 0xFF; FIX: CRGBA::a (было .alpha)
            alpha = float(prt->m_MultColor.a) * StaticRef<float>(0x859A3C) * alpha;
        }
        // Цветовые байты: в оригинале конверсии _ftol2 (0x821B40) с потерянными x87-операндами;
        // обе ветки m_bSelfLit декомпилировались одинаково — записано единой формулой.
        renderInfo.m_Color1.red   = (uint8)red;
        renderInfo.m_Color1.green = (uint8)green;
        renderInfo.m_Color1.blue  = (uint8)blue;
        renderInfo.m_Color1.alpha = (uint8)alpha;

        // Выбор растра по anim-текстурам
        RwRaster* raster = nullptr;
        if (!renderInfo.m_bHasAnimTextures || renderInfo.m_nCurrentTexId == 1) {
            raster = RwTextureGetRaster(m_apTextures[0]);
        } else if (renderInfo.m_nCurrentTexId == 2) {
            raster = m_apTextures[1] ? RwTextureGetRaster(m_apTextures[1]) : RwTextureGetRaster(m_apTextures[0]);
        } else if (renderInfo.m_nCurrentTexId == 3) {
            raster = m_apTextures[2] ? RwTextureGetRaster(m_apTextures[2]) : RwTextureGetRaster(m_apTextures[0]);
        } else if (renderInfo.m_nCurrentTexId == 4) {
            raster = m_apTextures[3] ? RwTextureGetRaster(m_apTextures[3]) : RwTextureGetRaster(m_apTextures[0]);
        } else {
            raster = RwTextureGetRaster(m_apTextures[0]);
        }
        if (currentRaster != raster) {
            RenderEnd();
            currentRaster = raster;
            RenderBegin(raster, nullptr, rwIM3D_VERTEXUV);
        }

        // В trail-режимах верх/низ квада заменяются длиной вектора растяжения
        float extTop    = renderInfo.m_fSpriteTop;
        float extBottom = renderInfo.m_fSpriteBottom;
        if (renderInfo.m_nTrailScreenMode > 0) {
            extTop    = RwV3dLength(&trailOffset);
            extBottom = 0.0f;
        }

        const float extA = extTop    * sizeY; // fVar1 = fStack_c4 * fStack_d0
        const float extB = extBottom * sizeY; // fStack_15c = fStack_c0 * fStack_d0
        const float extC = renderInfo.m_fSpriteLeft  * sizeX;
        const float extD = renderInfo.m_fSpriteRight * sizeX;

        const CVector vTL = pos + axisRight * extC + axisUp * extA; // right*C + up*A
        const CVector vTR = pos + axisRight * extD + axisUp * extB; // right*D + up*B
        const CVector vBR = pos + axisRight * extD + axisUp * extA; // right*D + up*A
        const CVector vBL = pos + axisRight * extC + axisUp * extB; // right*C + up*B

        const auto& col = renderInfo.m_Color1;
        RenderAddTri_(
            vTL.x, vTL.y, vTL.z,
            vTR.x, vTR.y, vTR.z,
            vBR.x, vBR.y, vBR.z,
            0.0f, 0.0f,
            1.0f, 1.0f,
            1.0f, 0.0f,
            col.red, col.green, col.blue, col.alpha,
            col.red, col.green, col.blue, col.alpha,
            col.red, col.green, col.blue, col.alpha
        );
        RenderAddTri_(
            vTR.x, vTR.y, vTR.z,
            vTL.x, vTL.y, vTL.z,
            vBL.x, vBL.y, vBL.z,
            1.0f, 1.0f,
            0.0f, 0.0f,
            0.0f, 1.0f,
            col.red, col.green, col.blue, col.alpha,
            col.red, col.green, col.blue, col.alpha,
            col.red, col.green, col.blue, col.alpha
        );
    }
    RenderEnd();
}

// 0x4A2510
// int-fx: тело из батча fxemitter-1/0x4A2510.cpp (декомпил).
// Возвращаемое значение: 1 если частица найдена и возвращена, 0 если нет (bool верен).
bool FxEmitterBP_c::FreePrtFromPrim(FxSystem_c* system) {
    for (auto it = m_Particles.GetHead(); it; it = m_Particles.GetNext(it)) {
        if (it->m_System == system) {
            m_Particles.RemoveItem(it);                   // 0x4A8E30 (TList_c<Particle_c>::RemoveItem)
            g_fxMan.ReturnParticle(it->AsFxEmitterPrt()); // 0x4A93B0 (FxManager_c::ReturnParticle)
            return true;
        }
    }
    return false;
}

// todo: eFxInfo
// 0x4A24D0
bool FxEmitterBP_c::IsFxInfoPresent(eFxInfoType type) const {
    if (m_FxInfoManager.m_nNumInfos <= 0)
        return false;

    for (auto& info : m_FxInfoManager.GetInfos()) {
        if (info->m_nType == type) {
            return true;
        }
    }
    return false;
}
