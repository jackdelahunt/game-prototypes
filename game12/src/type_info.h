#pragma once

/////////////////////////////////////////////////////////////////////////////////
// WARNING: auto generated file from meta.exe, manual edits will be replaced!! //
/////////////////////////////////////////////////////////////////////////////////

#include "meta.h"

template<>
struct MetaEnum<WeaponHandle> {
const static int count = 5;

inline static EnumValue<WeaponHandle> values[count] = {
    EnumValue<WeaponHandle>(string("WH_DEAGLE"), WH_DEAGLE),
    EnumValue<WeaponHandle>(string("WH_M4"), WH_M4),
    EnumValue<WeaponHandle>(string("WH_TAP"), WH_TAP),
    EnumValue<WeaponHandle>(string("WH_PAL"), WH_PAL),
    EnumValue<WeaponHandle>(string("_WH_COUNT"), _WH_COUNT),
};

static string name(WeaponHandle value) {
    switch (value) {
        case WH_DEAGLE: return values[0].name;
        case WH_M4: return values[1].name;
        case WH_TAP: return values[2].name;
        case WH_PAL: return values[3].name;
        case _WH_COUNT: return values[4].name;
    }
}

static EnumValue<WeaponHandle> *value(string name) {
    for (int i = 0; i < count; i++) {
        if (slice_memcmp(values[i].name, name)) return &values[i];
    }
    return nullptr;
}

static int index(WeaponHandle value) {
    for (int i = 0; i < count; i++) {
        if (values[i].value == value) return i;
    }
    return -1;
}

};

template<>
struct MetaEnum<PickupType> {
const static int count = 5;

inline static EnumValue<PickupType> values[count] = {
    EnumValue<PickupType>(string("PT_NONE"), PT_NONE),
    EnumValue<PickupType>(string("PT_M4"), PT_M4),
    EnumValue<PickupType>(string("PT_TAP"), PT_TAP),
    EnumValue<PickupType>(string("PT_PAL"), PT_PAL),
    EnumValue<PickupType>(string("PT_HEALTH"), PT_HEALTH),
};

static string name(PickupType value) {
    switch (value) {
        case PT_NONE: return values[0].name;
        case PT_M4: return values[1].name;
        case PT_TAP: return values[2].name;
        case PT_PAL: return values[3].name;
        case PT_HEALTH: return values[4].name;
    }
}

static EnumValue<PickupType> *value(string name) {
    for (int i = 0; i < count; i++) {
        if (slice_memcmp(values[i].name, name)) return &values[i];
    }
    return nullptr;
}

static int index(PickupType value) {
    for (int i = 0; i < count; i++) {
        if (values[i].value == value) return i;
    }
    return -1;
}

};

template<>
struct MetaEnum<EntityFlag> {
const static int count = 10;

inline static EnumValue<EntityFlag> values[count] = {
    EnumValue<EntityFlag>(string("EF_PLAYER"), EF_PLAYER),
    EnumValue<EntityFlag>(string("EF_SPAWN_POINT"), EF_SPAWN_POINT),
    EnumValue<EntityFlag>(string("EF_SOLID_HITBOX"), EF_SOLID_HITBOX),
    EnumValue<EntityFlag>(string("EF_STATIC_HITBOX"), EF_STATIC_HITBOX),
    EnumValue<EntityFlag>(string("EF_TRIGGER_HITBOX"), EF_TRIGGER_HITBOX),
    EnumValue<EntityFlag>(string("EF_DEAD"), EF_DEAD),
    EnumValue<EntityFlag>(string("EF_PICKUP"), EF_PICKUP),
    EnumValue<EntityFlag>(string("EF_MISSLE"), EF_MISSLE),
    EnumValue<EntityFlag>(string("EF_JUMP_PAD"), EF_JUMP_PAD),
    EnumValue<EntityFlag>(string("EF_DELETE"), EF_DELETE),
};

static string name(EntityFlag value) {
    switch (value) {
        case EF_PLAYER: return values[0].name;
        case EF_SPAWN_POINT: return values[1].name;
        case EF_SOLID_HITBOX: return values[2].name;
        case EF_STATIC_HITBOX: return values[3].name;
        case EF_TRIGGER_HITBOX: return values[4].name;
        case EF_DEAD: return values[5].name;
        case EF_PICKUP: return values[6].name;
        case EF_MISSLE: return values[7].name;
        case EF_JUMP_PAD: return values[8].name;
        case EF_DELETE: return values[9].name;
    }
}

static EnumValue<EntityFlag> *value(string name) {
    for (int i = 0; i < count; i++) {
        if (slice_memcmp(values[i].name, name)) return &values[i];
    }
    return nullptr;
}

static int index(EntityFlag value) {
    for (int i = 0; i < count; i++) {
        if (values[i].value == value) return i;
    }
    return -1;
}

};

