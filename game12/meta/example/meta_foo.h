#pragma once

#include "meta.h"
#include "foo.h"

#include <string>

template<>
struct MetaEnum<BuildType> {
    const static int count = 3;

    inline static EnumValue values[count] = {
        {.name = "Client",  .value = int(Client)},
        {.name = "Host",    .value = int(Host)},
        {.name = "Server",  .value = int(Server)},
    };

	static std::string name(BuildType value) {
        switch (value) {
        case Client:    return values[0].name;
        case Host:      return values[1].name;
        case Server:    return values[2].name;
        }
	}

	static BuildType value(std::string name) {
        for (int i = 0; i < count; i++) {
            if (values[i].name == name) {
                return (BuildType) values[i].value;
            }
        }

        return (BuildType) 0;
	}
};
