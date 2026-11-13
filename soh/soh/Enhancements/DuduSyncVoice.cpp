#include "global.h"

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

#include <SDL2/SDL.h>
#include <ship/Context.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

extern "C" {
extern PlayState* gPlayState;
}

namespace {

SDL_AudioDeviceID sVoiceDevice = 0;
SDL_AudioSpec sVoiceDeviceSpec = {};

bool sTracking = false;
bool sPendingPage = false;
bool sHasPlayed = false;
uint16_t sTextId = 0;
uint16_t sPage = 0;

bool EnsureVoiceDevice() {
    if (sVoiceDevice != 0) {
        return true;
    }
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            SPDLOG_ERROR("[DuduVoice] SDL audio init failed: {}", SDL_GetError());
            return false;
        }
    }
    SDL_AudioSpec wanted = {};
    wanted.freq = 44100;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 2;
    wanted.samples = 1024;
    wanted.callback = nullptr;
    sVoiceDevice = SDL_OpenAudioDevice(nullptr, 0, &wanted, &sVoiceDeviceSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (sVoiceDevice == 0) {
        SPDLOG_ERROR("[DuduVoice] open device failed: {}", SDL_GetError());
        return false;
    }
    SDL_PauseAudioDevice(sVoiceDevice, 0);
    return true;
}

void StopVoice() {
    if (sVoiceDevice != 0) {
        SDL_ClearQueuedAudio(sVoiceDevice);
    }
}

bool PlayVoice(uint16_t textId, uint16_t page) {
    if (!EnsureVoiceDevice()) {
        return false;
    }
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
    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt, src.format, src.channels, src.freq, sVoiceDeviceSpec.format,
                          sVoiceDeviceSpec.channels, sVoiceDeviceSpec.freq) >= 0) {
        cvt.len = srcLen;
        cvt.buf = (Uint8*)SDL_malloc((size_t)cvt.len * cvt.len_mult);
        if (cvt.buf != nullptr) {
            SDL_memcpy(cvt.buf, srcBuf, srcLen);
            SDL_ConvertAudio(&cvt);
            StopVoice();
            SDL_QueueAudio(sVoiceDevice, cvt.buf, cvt.len_cvt);
            SDL_free(cvt.buf);
        }
    }
    SDL_FreeWAV(srcBuf);
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
