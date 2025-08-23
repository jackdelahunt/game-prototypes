#pragma once

/////////////////////////////////////////////////////////////////////////////////
// WARNING: auto generated file from meta.exe, manual edits will be replaced!! //
/////////////////////////////////////////////////////////////////////////////////

#include "meta.h"

template<>
struct MetaEnum<PickupType> {
const static int count = 5;

inline static EnumValue values[count] = {
    {.name = "PT_NONE", .value = int(PT_NONE)},
    {.name = "PT_M4", .value = int(PT_M4)},
    {.name = "PT_TAP", .value = int(PT_TAP)},
    {.name = "PT_HEALTH", .value = int(PT_HEALTH)},
    {.name = "PT_PAL", .value = int(PT_PAL)},
};

static std::string name(PickupType value) {
    switch (value) {
        case PT_NONE: return values[PT_NONE].name;
        case PT_M4: return values[PT_M4].name;
        case PT_TAP: return values[PT_TAP].name;
        case PT_HEALTH: return values[PT_HEALTH].name;
        case PT_PAL: return values[PT_PAL].name;
    }
}

static PickupType value(std::string name) {
    for (int i = 0; i < count; i++) {
        if (values[i].name == name) return (PickupType) values[i].value;
    }
    return (PickupType) 0;
}

};

