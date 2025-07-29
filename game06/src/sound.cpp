#ifndef SOUND_CPP
#define SOUND_CPP

#include "libs/libs.h"
#include "game.h"

enum SoundHandle {
    SH_DASH,
    SH_COUNT__
};

struct SoundEngine {
    ma_engine engine;

    Array<ma_sound, SH_COUNT__> sounds;
};

bool init_sound_engine(SoundEngine *sound_engine);
bool load_sounds(SoundEngine *sound_engine);
void play_sound(SoundEngine *sound_engine, SoundHandle handle);

string sound_path(SoundHandle handle);

bool init_sound_engine(SoundEngine *sound_engine) {
    ma_result result = ma_engine_init(NULL, &sound_engine->engine);
    if (result != MA_SUCCESS) {
        printf("failed to init sound engine\n");
        return false;
    }

    return true;
}

bool load_sounds(SoundEngine *sound_engine) {
    for (i64 i = 0; i < sound_engine->sounds.size; i++) {
        SoundHandle handle = (SoundHandle) i;

        string path = sound_path(handle);
        ma_sound *sound = &sound_engine->sounds[i];

        ma_result result = ma_sound_init_from_file(&sound_engine->engine, path.c(), 0, NULL, NULL, sound);
        if (result != MA_SUCCESS) {
            printf("failed to load sound: %s\n", path.c());
            return false;
        }
    }

    return true; 
}

void play_sound(SoundEngine *sound_engine, SoundHandle handle) {
    ma_sound *sound = &sound_engine->sounds[handle];
    ma_sound_start(sound);
}

string sound_path(SoundHandle handle) {
    switch (handle) {
        case SH_DASH: 
            return "resources/sounds/dash.wav";
        default: 
            assert(0);
    }

    return {};
}

#endif
