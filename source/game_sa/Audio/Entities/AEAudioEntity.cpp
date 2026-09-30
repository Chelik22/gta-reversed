/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"
#include "AEAudioEntity.h"

constexpr auto EVENT_VOLUMES_BUFFER_SIZE = 45401;

auto& m_pAudioEventVolumes = StaticRef<int8*>(0xBD00F8); // Use `GetDefaultVolume` to access!

// NOTSA | INLINED | REFACTORED
bool CAEAudioEntity::StaticInitialise() {
    // NOTSA: reuse the pre-allocated buffer if present (audio re-init path)
    if (!m_pAudioEventVolumes) {
        m_pAudioEventVolumes = new int8[EVENT_VOLUMES_BUFFER_SIZE]{};
    }
    auto file = CFileMgr::OpenFile("AUDIO\\CONFIG\\EVENTVOL.DAT", "r");
    if (!file) {
        NOTSA_LOG_WARN("[AudioEngine] Failed to open EVENTVOL.DAT");
        CFileMgr::CloseFile(file);
        return false;
    }
    if (CFileMgr::Read(file, m_pAudioEventVolumes, EVENT_VOLUMES_BUFFER_SIZE) != EVENT_VOLUMES_BUFFER_SIZE) {
        NOTSA_LOG_WARN("[AudioEngine] Failed to read EVENTVOL.DAT");
        CFileMgr::CloseFile(file);
        return false;
    }
    CFileMgr::CloseFile(file);
    return true;
}

// NOTSA | INLINED | REFACTORED
void CAEAudioEntity::Shutdown() {
    delete[] std::exchange(m_pAudioEventVolumes, nullptr);
}

float CAEAudioEntity::GetDefaultVolume(eAudioEvents event) {
    // NOTSA(stability): audio can be absent (headless/no dsound); original would
    // crash the menu on first volume lookup. Fail soft with silence (0 dB -> 0).
    if (!m_pAudioEventVolumes) {
        return 0.0f;
    }
    return static_cast<float>(m_pAudioEventVolumes[event]);
}
