#pragma once

/////////////////////////////////////////////////////////////////////////////////
// WARNING: auto generated file from meta.exe, manual edits will be replaced!! //
/////////////////////////////////////////////////////////////////////////////////

#include "meta.h"

template<>
struct MetaEnum<MaterialHandle> {
const static int count = 9;

inline static EnumValue<MaterialHandle> values[count] = {
    EnumValue<MaterialHandle>(string("MAT_DEFAULT"), MAT_DEFAULT),
    EnumValue<MaterialHandle>(string("MAT_DEFAULT_UNLIT"), MAT_DEFAULT_UNLIT),
    EnumValue<MaterialHandle>(string("MAT_MUZZLE_FLASH"), MAT_MUZZLE_FLASH),
    EnumValue<MaterialHandle>(string("MAT_DEV_WHITE"), MAT_DEV_WHITE),
    EnumValue<MaterialHandle>(string("MAT_DEV_RED"), MAT_DEV_RED),
    EnumValue<MaterialHandle>(string("MAT_DEV_GREEN"), MAT_DEV_GREEN),
    EnumValue<MaterialHandle>(string("MAT_DEV_BLUE"), MAT_DEV_BLUE),
    EnumValue<MaterialHandle>(string("MAT_DEV_YELLOW"), MAT_DEV_YELLOW),
    EnumValue<MaterialHandle>(string("_MAT_COUNT"), _MAT_COUNT),
};

static string name(MaterialHandle value) {
    switch (value) {
        case MAT_DEFAULT: return values[0].name;
        case MAT_DEFAULT_UNLIT: return values[1].name;
        case MAT_MUZZLE_FLASH: return values[2].name;
        case MAT_DEV_WHITE: return values[3].name;
        case MAT_DEV_RED: return values[4].name;
        case MAT_DEV_GREEN: return values[5].name;
        case MAT_DEV_BLUE: return values[6].name;
        case MAT_DEV_YELLOW: return values[7].name;
        case _MAT_COUNT: return values[8].name;
    }
}

static EnumValue<MaterialHandle> *value(string name) {
    for (int i = 0; i < count; i++) {
        if (slice_memcmp(values[i].name, name)) return &values[i];
    }
    return nullptr;
}

static int index(MaterialHandle value) {
    for (int i = 0; i < count; i++) {
        if (values[i].value == value) return i;
    }
    return -1;
}

};

template<>
struct MetaEnum<MeshHandle> {
const static int count = 5;

inline static EnumValue<MeshHandle> values[count] = {
    EnumValue<MeshHandle>(string("MH_NONE"), MH_NONE),
    EnumValue<MeshHandle>(string("MH_DEAGLE"), MH_DEAGLE),
    EnumValue<MeshHandle>(string("MH_M4"), MH_M4),
    EnumValue<MeshHandle>(string("MH_CROSS"), MH_CROSS),
    EnumValue<MeshHandle>(string("_MH_COUNT"), _MH_COUNT),
};

static string name(MeshHandle value) {
    switch (value) {
        case MH_NONE: return values[0].name;
        case MH_DEAGLE: return values[1].name;
        case MH_M4: return values[2].name;
        case MH_CROSS: return values[3].name;
        case _MH_COUNT: return values[4].name;
    }
}

static EnumValue<MeshHandle> *value(string name) {
    for (int i = 0; i < count; i++) {
        if (slice_memcmp(values[i].name, name)) return &values[i];
    }
    return nullptr;
}

static int index(MeshHandle value) {
    for (int i = 0; i < count; i++) {
        if (values[i].value == value) return i;
    }
    return -1;
}

};

template<>
struct MetaEnum<WeaponHandle> {
const static int count = 5;

inline static EnumValue<WeaponHandle> values[count] = {
    EnumValue<WeaponHandle>(string("WH_DEAGLE"), WH_DEAGLE),
    EnumValue<WeaponHandle>(string("WH_M4"), WH_M4),
    EnumValue<WeaponHandle>(string("WH_ROCKET_LAUNCHER"), WH_ROCKET_LAUNCHER),
    EnumValue<WeaponHandle>(string("WH_PEACE_AND_LOVE"), WH_PEACE_AND_LOVE),
    EnumValue<WeaponHandle>(string("_WH_COUNT"), _WH_COUNT),
};

static string name(WeaponHandle value) {
    switch (value) {
        case WH_DEAGLE: return values[0].name;
        case WH_M4: return values[1].name;
        case WH_ROCKET_LAUNCHER: return values[2].name;
        case WH_PEACE_AND_LOVE: return values[3].name;
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
const static int count = 19;

inline static EnumValue<EntityFlag> values[count] = {
    EnumValue<EntityFlag>(string("EF_PLAYER"), EF_PLAYER),
    EnumValue<EntityFlag>(string("EF_DUMMY"), EF_DUMMY),
    EnumValue<EntityFlag>(string("EF_SPAWN_POINT"), EF_SPAWN_POINT),
    EnumValue<EntityFlag>(string("EF_SOLID_HITBOX"), EF_SOLID_HITBOX),
    EnumValue<EntityFlag>(string("EF_STATIC_HITBOX"), EF_STATIC_HITBOX),
    EnumValue<EntityFlag>(string("EF_TRIGGER_HITBOX"), EF_TRIGGER_HITBOX),
    EnumValue<EntityFlag>(string("EF_DAMAGEABLE"), EF_DAMAGEABLE),
    EnumValue<EntityFlag>(string("EF_DEAD"), EF_DEAD),
    EnumValue<EntityFlag>(string("EF_PICKUP"), EF_PICKUP),
    EnumValue<EntityFlag>(string("EF_MISSLE"), EF_MISSLE),
    EnumValue<EntityFlag>(string("EF_JUMP_PAD"), EF_JUMP_PAD),
    EnumValue<EntityFlag>(string("EF_COMPLEX_PHYSICS"), EF_COMPLEX_PHYSICS),
    EnumValue<EntityFlag>(string("EF_POINT_LIGHT"), EF_POINT_LIGHT),
    EnumValue<EntityFlag>(string("EF_PARTICLE"), EF_PARTICLE),
    EnumValue<EntityFlag>(string("EF_BLOOD_PARTICLE"), EF_BLOOD_PARTICLE),
    EnumValue<EntityFlag>(string("EF_SURFACE_PARTICLE"), EF_SURFACE_PARTICLE),
    EnumValue<EntityFlag>(string("EF_DRAW_MESH"), EF_DRAW_MESH),
    EnumValue<EntityFlag>(string("EF_IGNORE_RAYCAST"), EF_IGNORE_RAYCAST),
    EnumValue<EntityFlag>(string("EF_DELETE"), EF_DELETE),
};

static string name(EntityFlag value) {
    switch (value) {
        case EF_PLAYER: return values[0].name;
        case EF_DUMMY: return values[1].name;
        case EF_SPAWN_POINT: return values[2].name;
        case EF_SOLID_HITBOX: return values[3].name;
        case EF_STATIC_HITBOX: return values[4].name;
        case EF_TRIGGER_HITBOX: return values[5].name;
        case EF_DAMAGEABLE: return values[6].name;
        case EF_DEAD: return values[7].name;
        case EF_PICKUP: return values[8].name;
        case EF_MISSLE: return values[9].name;
        case EF_JUMP_PAD: return values[10].name;
        case EF_COMPLEX_PHYSICS: return values[11].name;
        case EF_POINT_LIGHT: return values[12].name;
        case EF_PARTICLE: return values[13].name;
        case EF_BLOOD_PARTICLE: return values[14].name;
        case EF_SURFACE_PARTICLE: return values[15].name;
        case EF_DRAW_MESH: return values[16].name;
        case EF_IGNORE_RAYCAST: return values[17].name;
        case EF_DELETE: return values[18].name;
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

