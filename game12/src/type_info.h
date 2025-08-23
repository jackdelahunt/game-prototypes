#pragma once

/////////////////////////////////////////////////////////////////////////////////
// WARNING: auto generated file from meta.exe, manual edits will be replaced!! //
/////////////////////////////////////////////////////////////////////////////////

#include "meta.h"

template<>
struct MetaEnum<PickupType> {
const static int count = 5;

inline static EnumValue<PickupType> values[count] = {
    EnumValue<PickupType>(string("PT_NONE"), PT_NONE),
    EnumValue<PickupType>(string("PT_M4"), PT_M4),
    EnumValue<PickupType>(string("PT_TAP"), PT_TAP),
    EnumValue<PickupType>(string("PT_HEALTH"), PT_HEALTH),
    EnumValue<PickupType>(string("PT_PAL"), PT_PAL),
};

static string name(PickupType value) {
    switch (value) {
        case PT_NONE: return values[PT_NONE].name;
        case PT_M4: return values[PT_M4].name;
        case PT_TAP: return values[PT_TAP].name;
        case PT_HEALTH: return values[PT_HEALTH].name;
        case PT_PAL: return values[PT_PAL].name;
    }
}

static PickupType value(string name) {
    for (int i = 0; i < count; i++) {
        if (slice_memcmp(values[i].name, name)) return values[i].value;
    }
    return (PickupType) 0;
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
const static int count = 9;

inline static EnumValue<EntityFlag> values[count] = {
    EnumValue<EntityFlag>(string("EF_PLAYER"), EF_PLAYER),
    EnumValue<EntityFlag>(string("EF_SPAWN_POINT"), EF_SPAWN_POINT),
    EnumValue<EntityFlag>(string("EF_SOLID_HITBOX"), EF_SOLID_HITBOX),
    EnumValue<EntityFlag>(string("EF_STATIC_HITBOX"), EF_STATIC_HITBOX),
    EnumValue<EntityFlag>(string("EF_DEAD"), EF_DEAD),
    EnumValue<EntityFlag>(string("EF_PICKUP"), EF_PICKUP),
    EnumValue<EntityFlag>(string("EF_TRIGGER_HITBOX"), EF_TRIGGER_HITBOX),
    EnumValue<EntityFlag>(string("EF_MISSLE"), EF_MISSLE),
    EnumValue<EntityFlag>(string("EF_DELETE"), EF_DELETE),
};

static string name(EntityFlag value) {
    switch (value) {
        case EF_PLAYER: return values[EF_PLAYER].name;
        case EF_SPAWN_POINT: return values[EF_SPAWN_POINT].name;
        case EF_SOLID_HITBOX: return values[EF_SOLID_HITBOX].name;
        case EF_STATIC_HITBOX: return values[EF_STATIC_HITBOX].name;
        case EF_DEAD: return values[EF_DEAD].name;
        case EF_PICKUP: return values[EF_PICKUP].name;
        case EF_TRIGGER_HITBOX: return values[EF_TRIGGER_HITBOX].name;
        case EF_MISSLE: return values[EF_MISSLE].name;
        case EF_DELETE: return values[EF_DELETE].name;
    }
}

static EntityFlag value(string name) {
    for (int i = 0; i < count; i++) {
        if (slice_memcmp(values[i].name, name)) return values[i].value;
    }
    return (EntityFlag) 0;
}

static int index(EntityFlag value) {
    for (int i = 0; i < count; i++) {
        if (values[i].value == value) return i;
    }
    return -1;
}

};

