#include "StdInc.h"

#include "AEStaticChannel.h"

#include "AESmoothFadeThread.h"

void CAEStaticChannel::InjectHooks() {
    RH_ScopedVirtualClass(CAEStaticChannel, 0x85F3CC, 9);
    RH_ScopedCategory("Audio/Hardware");

    RH_ScopedVMTInstall(Service, 0x4F10D0);
    RH_ScopedVMTInstall(IsSoundPlaying, 0x4F0F40);
    RH_ScopedVMTInstall(GetPlayTime, 0x4F0F70);
    RH_ScopedVMTInstall(GetLength, 0x4F0FA0);
    RH_ScopedVMTInstall(Play, 0x4F0BD0);
    RH_ScopedVMTInstall(SynchPlayback, 0x4F1040);
    RH_ScopedVMTInstall(Stop, 0x4F0FB0);

    RH_ScopedInstall(SetAudioBuffer, 0x4F0C40);
}

CAEStaticChannel::CAEStaticChannel(IDirectSound* pDirectSound, uint16 channelId, bool hardwareMixAvailable, uint32 samplesPerSec, uint16 bitsPerSample) :
    CAEAudioChannel(pDirectSound, channelId, samplesPerSec, bitsPerSample),
    m_bNeedData(false),
    m_bNeedsSynch(false),
    m_IsHardwareMixAvailable(hardwareMixAvailable)
{
}

// 0x4F10D0
void CAEStaticChannel::Service() {
    if (!m_pDirectSoundBuffer) {
        m_nBufferStatus = 0;
        return;
    }

    if (m_bNeedData && (int32)(CTimer::GetTimeInMS() - m_nSyncTime) > field_74) {
        uint8* ppvAudioPtr1{};
        DWORD pdwAudioBytes{};

        VERIFY(SUCCEEDED(m_pDirectSoundBuffer->Lock(
            m_dwLockOffset,
            m_nNumLockBytes,
            reinterpret_cast<LPVOID*>(&ppvAudioPtr1),
            &pdwAudioBytes,
            nullptr,
            0,
            0
        )));

        for (auto i = 0u; i < m_nNumLoops; i++) {
            memcpy(
                &ppvAudioPtr1[i * m_nNumLockBytes],
                (uint8*)m_pBuffer + m_nCurrentBufferOffset,
                m_nNumLockBytes
            );
        }
        VERIFY(SUCCEEDED(m_pDirectSoundBuffer->Unlock(ppvAudioPtr1, pdwAudioBytes, nullptr, 0)));
        m_bNeedData = false;
    }

    UpdateStatus();

    if (!m_bPaused && !bufferStatus.Bit0x1) {
        if (const auto buf = std::exchange(m_pDirectSoundBuffer, nullptr)) {
            --g_numSoundChannelsUsed;
            buf->Release();
        }
    }
}

// 0x4F0F40
bool CAEStaticChannel::IsSoundPlaying() {
    if (!m_pDirectSoundBuffer)
        return false;

    if (m_bPaused || m_bNeedsSynch)
        return true;

    return CAEAudioChannel::IsBufferPlaying();
}

// 0x4F0F70
int16 CAEStaticChannel::GetPlayTime() {
    if (!IsSoundPlaying())
        return -1;

    const auto curPos = CAEAudioChannel::GetCurrentPlaybackPosition();
    return CAEAudioChannel::ConvertFromBytesToMS(curPos);
}

// 0x4F0FA0
uint16 CAEStaticChannel::GetLength() {
    return CAEAudioChannel::ConvertFromBytesToMS(m_nLengthInBytes);
}

// 0x4F0BD0
void CAEStaticChannel::Play(int16 timeInMs, int8 unused, float scalingFactor) {
    if (m_bLooped && m_nCurrentBufferOffset != 0 || !timeInMs) {
        m_bUnkn2 = false;
    } else {
        m_pDirectSoundBuffer->SetCurrentPosition(ConvertFromMsToBytes(timeInMs));
        m_bUnkn2 = true;
    }
    m_bNeedsSynch = true;
    m_bPaused = scalingFactor == 0.0f;
}

    

// 0x4F1040
void CAEStaticChannel::SynchPlayback() {
    if (!m_pDirectSoundBuffer || !m_bNeedsSynch || m_bPaused)
        return;

    if (m_bUnkn2) {
        m_pDirectSoundBuffer->SetVolume(-10000);
        if (!AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, m_Volume, -2, false)) {
            const auto dwVolume = static_cast<LONG>(m_Volume * 100.0F);
            m_pDirectSoundBuffer->SetVolume(dwVolume);
        }
    }

    m_pDirectSoundBuffer->Play(0, 0, m_bLooped);
    m_nSyncTime = CTimer::GetTimeInMS();
    m_bNeedsSynch = false;
}

// 0x4F0FB0
void CAEStaticChannel::Stop() {
    if (m_pDirectSoundBuffer &&
        CAEAudioChannel::IsBufferPlaying() &&
        !AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, -100.0F, -1, true)
    ) {
        m_pDirectSoundBuffer->Stop();
    }

    { // todo: Same as CAEAudioChannel::~CAEAudioChannel
    if (m_pDirectSoundBuffer) {
        --g_numSoundChannelsUsed;
        m_pDirectSoundBuffer->Release();
        m_pDirectSoundBuffer = nullptr;
    }

    if (m_pDirectSound3DBuffer) {
        m_pDirectSound3DBuffer->Release();
        m_pDirectSound3DBuffer = nullptr;
    }
    }
}

// 0x4F0C40
bool CAEStaticChannel::SetAudioBuffer(IDirectSound3DBuffer* buffer, uint16 size, int16 f88, int16 f8c, int16 loopOffset, uint16 frequency) {
    if (size == 0 || frequency == 0) {
        return false;
    }
    // (ab)use of the `buffer` parameter: it actually carries the PCM sample data
    const auto samples = reinterpret_cast<const void*>(buffer);

    if (m_pDirectSoundBuffer) {
        --g_numSoundChannelsUsed;
        m_pDirectSoundBuffer->Release();
        m_pDirectSoundBuffer = nullptr;
    }
    if (m_pDirectSound3DBuffer) {
        m_pDirectSound3DBuffer->Release();
        m_pDirectSound3DBuffer = nullptr;
    }
    m_pBuffer                = buffer;
    m_nCurrentBufferOffset   = 0; // reused as loop length in bytes below
    field_68                 = 0;
    m_bNeedData              = false;
    m_bUnkn4                 = false;
    m_bPaused                = false;
    field_6C                 = 0; // reused as loop iteration count below
    m_nLengthInBytes         = size;
    field_88                 = f88;
    field_8C                 = f8c;

    if (loopOffset != -1) {
        m_bLooped              = true;
        m_nCurrentBufferOffset = (loopOffset << 4) >> 3; // loop length: loopOffset * 8 bytes
        field_68               = size;
    }

    auto& bufferBytes = *(uint32*)((char*)this + 0x24); // unnamed base-class field: total DSound buffer size
    if (!m_bLooped || m_nCurrentBufferOffset == 0) {
        bufferBytes = size;
    } else {
        auto base = (uint32)field_68;
        m_nNumLockBytes = base - m_nCurrentBufferOffset; // one loop unit
        if (base < 24000) {
            base = 24000;
        }
        field_6C   = base / m_nNumLockBytes + 1; // iterations
        bufferBytes = field_6C * m_nNumLockBytes;
    }

    m_WaveFormat.wFormatTag      = 1;          // PCM
    m_WaveFormat.nChannels       = 1;
    m_WaveFormat.nSamplesPerSec  = frequency;
    m_WaveFormat.nAvgBytesPerSec = frequency * 2;
    m_WaveFormat.nBlockAlign     = 2;
    m_WaveFormat.wBitsPerSample  = 16;
    m_WaveFormat.cbSize          = 0;
    m_nFrequency = m_nOriginalFrequency = frequency;

    DSBUFFERDESC desc{};
    desc.dwSize          = 0x24;
    desc.dwFlags         = 0x80B4 + (m_IsHardwareMixAvailable ? 0 : 4);
    desc.dwBufferBytes   = bufferBytes;
    desc.lpwfxFormat     = &m_WaveFormat;
    desc.guid3DAlgorithm = StaticRef<GUID>(0x859150); // algorithm GUID from the original .rdata

    if (SUCCEEDED(m_pDirectSound->CreateSoundBuffer(&desc, &m_pDirectSoundBuffer, nullptr))) {
        ++g_numSoundChannelsUsed;

        void*  audioPtr{};
        DWORD  audioBytes{};
        if (SUCCEEDED(m_pDirectSoundBuffer->Lock(0, m_nLengthInBytes, &audioPtr, &audioBytes, nullptr, nullptr, DSBLOCK_ENTIREBUFFER))) {
            int32 startPosition = 0;
            if (m_nCurrentBufferOffset == 0) { // plain one-shot: copy everything, zero the tail
                memcpy(audioPtr, samples, size);
                if (audioBytes < m_nLengthInBytes) {
                    memset((char*)audioPtr + audioBytes, 0, m_nLengthInBytes - audioBytes);
                }
                m_bNeedData = false;
            } else { // looping: [intro][loop] samples -> intro repeated, loop placed at the end
                memcpy((char*)audioPtr + bufferBytes - m_nCurrentBufferOffset, samples, m_nCurrentBufferOffset);

                m_nNumLoops   = (uint16)(m_nCurrentBufferOffset / m_nNumLockBytes) + 1;
                m_dwLockOffset = bufferBytes - m_nNumLoops * m_nNumLockBytes;
                startPosition  = m_dwLockOffset;
                for (auto i = 0; i < field_6C - m_nNumLoops; i++) {
                    memcpy((char*)audioPtr + i * m_nNumLockBytes, (char*)samples + m_nCurrentBufferOffset, m_nNumLockBytes);
                }
                field_74    = (int32)(int16)ConvertFromBytesToMS(m_nCurrentBufferOffset); // loop length in ms
                m_bNeedData = true;
            }
            m_pDirectSoundBuffer->Unlock(audioPtr, audioBytes, nullptr, 0);
            m_pDirectSoundBuffer->SetCurrentPosition(startPosition);
            m_pDirectSoundBuffer->QueryInterface(StaticRef<IID>(0x859390), (void**)&m_pDirectSound3DBuffer); // IID_IDirectSound3DBuffer
            m_Volume = -100.0f;
            m_pDirectSoundBuffer->SetVolume(-10000); // muted until SynchPlayback fades in
            return true;
        }
        if (m_pDirectSoundBuffer) {
            m_pDirectSoundBuffer->Release();
            m_pDirectSoundBuffer = nullptr;
        }
    }
    return false;
}
