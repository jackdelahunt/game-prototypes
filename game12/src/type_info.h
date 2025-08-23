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

