// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#pragma once
#include <Preferences.h>
#include "Metric.h"

// ConfigVar extends Metric with Preferences load/save support.
// Specializations below handle type-specific Preferences calls.
template<typename T>
class ConfigVar : public Metric<T> {
public:
	ConfigVar(const char* name, T defaultValue, const char* units = "", const char* description = "",
			  T minVal = T(), T maxVal = T(), const char* prefKey = nullptr)
		: Metric<T>(name, defaultValue, units, description, minVal, maxVal),
		  _prefKey(prefKey ? prefKey : name) {}

	void load(Preferences& prefs) {
		_load(prefs);
	}

	void save(Preferences& prefs) const {
		_save(prefs);
	}

private:
	const char* _prefKey;
	void _load(Preferences& prefs);
	void _save(Preferences& prefs) const;
};

// ---- String ----
template<> inline void ConfigVar<String>::_load(Preferences& prefs) {
	_value = prefs.getString(_prefKey, _defaultValue);
}
template<> inline void ConfigVar<String>::_save(Preferences& prefs) const {
	prefs.putString(_prefKey, _value);
}

// ---- bool ----
template<> inline void ConfigVar<bool>::_load(Preferences& prefs) {
	_value = prefs.getBool(_prefKey, _defaultValue);
}
template<> inline void ConfigVar<bool>::_save(Preferences& prefs) const {
	prefs.putBool(_prefKey, _value);
}

// ---- float ----
template<> inline void ConfigVar<float>::_load(Preferences& prefs) {
	_value = prefs.getFloat(_prefKey, _defaultValue);
}
template<> inline void ConfigVar<float>::_save(Preferences& prefs) const {
	prefs.putFloat(_prefKey, _value);
}

// ---- uint8_t (byte) ----
template<> inline void ConfigVar<uint8_t>::_load(Preferences& prefs) {
	_value = prefs.getUChar(_prefKey, _defaultValue);
}
template<> inline void ConfigVar<uint8_t>::_save(Preferences& prefs) const {
	prefs.putUChar(_prefKey, _value);
}

// ---- unsigned int ----
template<> inline void ConfigVar<unsigned int>::_load(Preferences& prefs) {
	_value = prefs.getUInt(_prefKey, _defaultValue);
}
template<> inline void ConfigVar<unsigned int>::_save(Preferences& prefs) const {
	prefs.putUInt(_prefKey, _value);
}

// ---- int ----
template<> inline void ConfigVar<int>::_load(Preferences& prefs) {
	_value = prefs.getInt(_prefKey, _defaultValue);
}
template<> inline void ConfigVar<int>::_save(Preferences& prefs) const {
	prefs.putInt(_prefKey, _value);
}
