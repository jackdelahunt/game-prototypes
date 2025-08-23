#pragma once

#define meta

#include <type_traits>

template<typename T>
struct EnumValue {
	string name;
	T value;

	EnumValue(string n, T v) : name(n), value(v) {}
};

template<typename T>
struct MetaEnum {
	string name(T value);
	T value(string name);
};

template<typename T>
string meta_name(T value) {
	static_assert(std::is_enum_v<T>, "Using meta_name requires type 'T' to be an enum and tagged with the meta keyword");

	return MetaEnum<T>::name(value);
}

template<typename T>
T meta_value(string name) {
	static_assert(std::is_enum_v<T>, "Using meta_value requires type 'T' to be an enum and tagged with the meta keyword");

	return MetaEnum<T>::value(name);
}

template<typename T>
EnumValue<T> *meta_values() {
	static_assert(std::is_enum_v<T>, "Using meta_values requires type 'T' to be an enum and tagged with the meta keyword");

	return MetaEnum<T>::values;
}

template<typename T>
int meta_index(T value) {
	static_assert(std::is_enum_v<T>, "Using meta_index requires type 'T' to be an enum and tagged with the meta keyword");

	return MetaEnum<T>::index(value);
}

template<typename T>
int meta_count() {
	static_assert(std::is_enum_v<T>, "Using meta_count requires type 'T' to be an enum and tagged with the meta keyword");

	return MetaEnum<T>::count;
}
