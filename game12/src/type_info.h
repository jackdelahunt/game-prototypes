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
    EnumValue<MaterialHandle>(string("MAT_MUZZLE_FLASH"), MAT_MUZZLE_FLASH),
    EnumValue<MaterialHandle>(string("MAT_PARTICLE"), MAT_PARTICLE),
    EnumValue<MaterialHandle>(string("MAT_METAL_PLATE"), MAT_METAL_PLATE),
    EnumValue<MaterialHandle>(string("MAT_BROKEN_BRICK_WALL"), MAT_BROKEN_BRICK_WALL),
    EnumValue<MaterialHandle>(string("MAT_METAL_05C"), MAT_METAL_05C),
    EnumValue<MaterialHandle>(string("MAT_TILES_037"), MAT_TILES_037),
    EnumValue<MaterialHandle>(string("MAT_GRID"), MAT_GRID),
    EnumValue<MaterialHandle>(string("_MAT_COUNT"), _MAT_COUNT),
};

static string name(MaterialHandle value) {
    switch (value) {
        case MAT_DEFAULT: return values[0].name;
        case MAT_MUZZLE_FLASH: return values[1].name;
        case MAT_PARTICLE: return values[2].name;
        case MAT_METAL_PLATE: return values[3].name;
        case MAT_BROKEN_BRICK_WALL: return values[4].name;
        case MAT_METAL_05C: return values[5].name;
        case MAT_TILES_037: return values[6].name;
        case MAT_GRID: return values[7].name;
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

