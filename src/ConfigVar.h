#pragma once
#include <Preferences.h>
#include "Metric.h"

// ConfigVar extends Metric with Preferences load/save support.
// Specializations below handle type-specific Preferences calls.
template<typename T>
class ConfigVar : public Metric<T> {
public:
	ConfigVar(const char* name, T defaultValue, const char* units = "", const char* description = "",
			  T minVal = T(), T maxVal = T())
		: Metric<T>(name, defaultValue, units, description, minVal, maxVal) {}

	void load(Preferences& prefs) {
		_load(prefs);
	}

	void save(Preferences& prefs) const {
		_save(prefs);
	}

private:
	void _load(Preferences& prefs);
	void _save(Preferences& prefs) const;
};

// ---- String ----
template<> inline void ConfigVar<String>::_load(Preferences& prefs) {
	_value = prefs.getString(_name, _defaultValue);
}
template<> inline void ConfigVar<String>::_save(Preferences& prefs) const {
	prefs.putString(_name, _value);
}

// ---- bool ----
template<> inline void ConfigVar<bool>::_load(Preferences& prefs) {
	_value = prefs.getBool(_name, _defaultValue);
}
template<> inline void ConfigVar<bool>::_save(Preferences& prefs) const {
	prefs.putBool(_name, _value);
}

// ---- float ----
template<> inline void ConfigVar<float>::_load(Preferences& prefs) {
	_value = prefs.getFloat(_name, _defaultValue);
}
template<> inline void ConfigVar<float>::_save(Preferences& prefs) const {
	prefs.putFloat(_name, _value);
}

// ---- uint8_t (byte) ----
template<> inline void ConfigVar<uint8_t>::_load(Preferences& prefs) {
	_value = prefs.getUChar(_name, _defaultValue);
}
template<> inline void ConfigVar<uint8_t>::_save(Preferences& prefs) const {
	prefs.putUChar(_name, _value);
}

// ---- unsigned int ----
template<> inline void ConfigVar<unsigned int>::_load(Preferences& prefs) {
	_value = prefs.getUInt(_name, _defaultValue);
}
template<> inline void ConfigVar<unsigned int>::_save(Preferences& prefs) const {
	prefs.putUInt(_name, _value);
}

// ---- int ----
template<> inline void ConfigVar<int>::_load(Preferences& prefs) {
	_value = prefs.getInt(_name, _defaultValue);
}
template<> inline void ConfigVar<int>::_save(Preferences& prefs) const {
	prefs.putInt(_name, _value);
}
