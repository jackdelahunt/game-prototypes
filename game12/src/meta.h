#pragma once

#include <string>

#define meta

struct EnumValue {
	std::string name;
	int value;
};

template<typename T>
struct MetaEnum {
	std::string name(T value);
	T value(std::string name);
};

template<typename T>
std::string meta_name(T value) {
	static_assert(std::is_enum_v<T>, "Using meta_name requires type 'T' to be an enum and tagged with the meta keyword");

	return MetaEnum<T>::name(value);
}

template<typename T>
T meta_value(std::string name) {
	static_assert(std::is_enum_v<T>, "Using meta_value requires type 'T' to be an enum and tagged with the meta keyword");

	return MetaEnum<T>::value(name);
}

template<typename T>
EnumValue *meta_values() {
	static_assert(std::is_enum_v<T>, "Using meta_values requires type 'T' to be an enum and tagged with the meta keyword");

	return MetaEnum<T>::values;
}

template<typename T>
int meta_count() {
	static_assert(std::is_enum_v<T>, "Using meta_count requires type 'T' to be an enum and tagged with the meta keyword");

	return MetaEnum<T>::count;
}
