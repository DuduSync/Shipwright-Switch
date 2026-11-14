#include "global.h"

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

#include <SDL2/SDL.h>
#include <ship/Context.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

extern "C" {
extern PlayState* gPlayState;
}

namespace {

std::mutex gVoiceMutex;
std::vector<int16_t> gVoicePcm;
int gVoiceChannels = 1;
int gVoiceRate = 48000;
double gVoicePos = 0.0;
bool gVoicePlaying = false;

bool sTracking = false;
bool sPendingPage = false;
bool sHasPlayed = false;
uint16_t sTextId = 0;
uint16_t sPage = 0;

void StopVoice() {
    std::lock_guard<std::mutex> lock(gVoiceMutex);
    gVoicePlaying = false;
    gVoicePcm.clear();
    gVoicePos = 0.0;
}

bool PlayVoice(uint16_t textId, uint16_t page) {
    char rel[64];
    std::snprintf(rel, sizeof(rel), "voices/ptbr/%04X_%02u.wav", textId, (unsigned)page);
    std::string full = Ship::Context::GetPathRelativeToAppDirectory(rel);
    if (!std::filesystem::exists(full)) {
        return false;
    }

    SDL_AudioSpec src = {};
    Uint8* srcBuf = nullptr;
    Uint32 srcLen = 0;
    if (!SDL_LoadWAV(full.c_str(), &src, &srcBuf, &srcLen)) {
        return false;
    }

    std::vector<int16_t> pcm;
    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt, src.format, src.channels, src.freq, AUDIO_S16SYS, src.channels, src.freq) >= 0) {
        cvt.len = srcLen;
        cvt.buf = (Uint8*)SDL_malloc((size_t)cvt.len * cvt.len_mult);
        if (cvt.buf != nullptr) {
            SDL_memcpy(cvt.buf, srcBuf, srcLen);
            SDL_ConvertAudio(&cvt);
            pcm.assign((int16_t*)cvt.buf, (int16_t*)(cvt.buf + cvt.len_cvt));
            SDL_free(cvt.buf);
        }
    }
    SDL_FreeWAV(srcBuf);

    if (pcm.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(gVoiceMutex);
    gVoicePcm = std::move(pcm);
    gVoiceChannels = src.channels;
    gVoiceRate = src.freq;
    gVoicePos = 0.0;
    gVoicePlaying = true;
    return true;
}

void OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    (void)loadFromMessageTable;
    StopVoice();
    sTracking = textId != nullptr;
    sPendingPage = sTracking;
    sHasPlayed = false;
    sPage = 0;
    sTextId = textId != nullptr ? *textId : 0;
}

void OnDialogMessage() {
    if (!sTracking || gPlayState == nullptr) {
        return;
    }
    MessageContext* msgCtx = &gPlayState->msgCtx;

    if (msgCtx->msgMode == MSGMODE_TEXT_NEXT_MSG ||
        (msgCtx->msgMode == MSGMODE_TEXT_CONTINUING && msgCtx->stateTimer == 1)) {
        if (!sPendingPage) {
            StopVoice();
        }
        sPendingPage = true;
    }

    if (sPendingPage) {
        bool ready = false;
        switch (msgCtx->msgMode) {
            case MSGMODE_TEXT_DISPLAYING:
            case MSGMODE_TEXT_AWAIT_NEXT:
            case MSGMODE_TEXT_AWAIT_INPUT:
            case MSGMODE_TEXT_DONE:
            case MSGMODE_TEXT_DELAYED_BREAK:
                ready = true;
                break;
            default:
                break;
        }
        if (ready) {
            const uint16_t nextTextId = msgCtx->textId;
            if (sHasPlayed) {
                if (nextTextId == sTextId) {
                    ++sPage;
                } else {
                    sPage = 0;
                }
            }
            sTextId = nextTextId;
            PlayVoice(sTextId, sPage);
            sHasPlayed = true;
            sPendingPage = false;
        }
    }

    if (msgCtx->msgMode == MSGMODE_TEXT_CLOSING || msgCtx->msgMode == MSGMODE_NONE) {
        StopVoice();
        sTracking = false;
        sPendingPage = false;
        sHasPlayed = false;
    }
}

void RegisterDuduVoice() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenText);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnDialogMessage>(OnDialogMessage);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) {
        StopVoice();
        sTracking = false;
        sPendingPage = false;
        sHasPlayed = false;
    });
}

static RegisterShipInitFunc duduVoiceInit(RegisterDuduVoice);

} // namespace

// Called from the game's own SDL audio queue (patched libultraship) to mix the voice
// into the already-open audio device. The Switch only allows a single SDL audio device,
// so opening a second one (like the PC build does) is not an option there.
extern "C" void DuduSyncVoice_Mix(int16_t* out, size_t frames, int channels, int sample_rate) {
    if (out == nullptr || frames == 0 || channels <= 0 || sample_rate <= 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(gVoiceMutex);
    if (!gVoicePlaying || gVoicePcm.empty() || gVoiceChannels <= 0) {
        return;
    }

    const int srcCh = gVoiceChannels;
    const size_t srcFrames = gVoicePcm.size() / (size_t)srcCh;
    if (srcFrames < 2) {
        gVoicePlaying = false;
        return;
    }
    const double step = (double)gVoiceRate / (double)sample_rate;

    for (size_t i = 0; i < frames && gVoicePlaying; i++) {
        const size_t idx = (size_t)gVoicePos;
        if (idx + 1 >= srcFrames) {
            gVoicePlaying = false;
            break;
        }
        const double frac = gVoicePos - (double)idx;
        for (int c = 0; c < channels; c++) {
            const int sc = (c < srcCh) ? c : (srcCh - 1);
            const int16_t a = gVoicePcm[idx * (size_t)srcCh + (size_t)sc];
            const int16_t b = gVoicePcm[(idx + 1) * (size_t)srcCh + (size_t)sc];
            int32_t s = (int32_t)std::lround((1.0 - frac) * (double)a + frac * (double)b);
            int32_t v = (int32_t)out[i * (size_t)channels + (size_t)c] + s;
            v = std::clamp(v, (int32_t)-32768, (int32_t)32767);
            out[i * (size_t)channels + (size_t)c] = (int16_t)v;
        }
        gVoicePos += step;
    }
    if (gVoicePos + 1.0 >= (double)srcFrames) {
        gVoicePlaying = false;
    }
}
