// AYG Cool Bed™
// https://aygarage.com/cool-bed/
//
// Copyright (c) 2026 AY Garage Ltd. All rights reserved.
// SPDX-License-Identifier: MIT

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Base templated class for all metrics and configuration variables.
// Holds a value with metadata: min/max, units, description.
// Supports serialization to/from String and JSON.
template<typename T>
class Metric {
public:
	Metric(const char* name, T defaultValue, const char* units = "", const char* description = "",
		   T minVal = T(), T maxVal = T())
		: _name(name), _value(defaultValue), _defaultValue(defaultValue),
		  _units(units), _description(description), _minVal(minVal), _maxVal(maxVal) {}

	const char* getName() const { return _name; }
	const char* getUnits() const { return _units; }
	const char* getDescription() const { return _description; }
	T getMin() const { return _minVal; }
	T getMax() const { return _maxVal; }
	T getDefault() const { return _defaultValue; }

	T get() const { return _value; }
	bool isDefault() const { return _value == _defaultValue; }

	void set(T val) { _value = val; }

	// Serialize value to String
	String toString() const {
		return _toString(_value);
	}

	// Parse value from String
	bool fromString(const String& s) {
		T parsed;
		if (_fromString(s, parsed)) {
			_value = parsed;
			return true;
		}
		return false;
	}

	// Serialize value to JSON (writes key=name, value=value)
	void toJson(JsonObject& obj) const {
		_toJson(obj, _name, _value);
	}

	// Serialize value to JSON with a custom key
	void toJson(JsonObject& obj, const char* key) const {
		_toJson(obj, key, _value);
	}

	// Parse value from JSON object using our name as key
	bool fromJson(const JsonObjectConst& obj) {
		if (!obj[_name].isNull()) {
			_value = _fromJson(obj[_name]);
			return true;
		}
		return false;
	}

protected:
	const char* _name;
	T _value;
	T _defaultValue;
	const char* _units;
	const char* _description;
	T _minVal;
	T _maxVal;

	// --- type-specific helpers (specialized below) ---
	String _toString(T val) const;
	bool _fromString(const String& s, T& out) const;
	void _toJson(JsonObject& obj, const char* key, T val) const;
	T _fromJson(JsonVariantConst v) const;
};

// ---- String specializations ----
template<> inline String Metric<String>::_toString(String val) const { return val; }
template<> inline bool Metric<String>::_fromString(const String& s, String& out) const { out = s; return true; }
template<> inline void Metric<String>::_toJson(JsonObject& obj, const char* key, String val) const { obj[key] = val; }
template<> inline String Metric<String>::_fromJson(JsonVariantConst v) const { return v.as<String>(); }

// ---- bool specializations ----
template<> inline String Metric<bool>::_toString(bool val) const { return val ? "true" : "false"; }
template<> inline bool Metric<bool>::_fromString(const String& s, bool& out) const {
	if (s.equalsIgnoreCase("true") || s == "1") { out = true; return true; }
	if (s.equalsIgnoreCase("false") || s == "0") { out = false; return true; }
	return false;
}
template<> inline void Metric<bool>::_toJson(JsonObject& obj, const char* key, bool val) const { obj[key] = val; }
template<> inline bool Metric<bool>::_fromJson(JsonVariantConst v) const { return v.as<bool>(); }

// ---- float specializations ----
template<> inline String Metric<float>::_toString(float val) const { return String(val, 2); }
template<> inline bool Metric<float>::_fromString(const String& s, float& out) const { out = s.toFloat(); return true; }
template<> inline void Metric<float>::_toJson(JsonObject& obj, const char* key, float val) const { obj[key] = val; }
template<> inline float Metric<float>::_fromJson(JsonVariantConst v) const { return v.as<float>(); }

// ---- uint8_t (byte) specializations ----
template<> inline String Metric<uint8_t>::_toString(uint8_t val) const { return String((unsigned int)val); }
template<> inline bool Metric<uint8_t>::_fromString(const String& s, uint8_t& out) const { out = (uint8_t)s.toInt(); return true; }
template<> inline void Metric<uint8_t>::_toJson(JsonObject& obj, const char* key, uint8_t val) const { obj[key] = (unsigned int)val; }
template<> inline uint8_t Metric<uint8_t>::_fromJson(JsonVariantConst v) const { return (uint8_t)v.as<unsigned int>(); }

// ---- unsigned int specializations ----
template<> inline String Metric<unsigned int>::_toString(unsigned int val) const { return String(val); }
template<> inline bool Metric<unsigned int>::_fromString(const String& s, unsigned int& out) const { out = (unsigned int)s.toInt(); return true; }
template<> inline void Metric<unsigned int>::_toJson(JsonObject& obj, const char* key, unsigned int val) const { obj[key] = val; }
template<> inline unsigned int Metric<unsigned int>::_fromJson(JsonVariantConst v) const { return v.as<unsigned int>(); }

// ---- int specializations ----
template<> inline String Metric<int>::_toString(int val) const { return String(val); }
template<> inline bool Metric<int>::_fromString(const String& s, int& out) const { out = s.toInt(); return true; }
template<> inline void Metric<int>::_toJson(JsonObject& obj, const char* key, int val) const { obj[key] = val; }
template<> inline int Metric<int>::_fromJson(JsonVariantConst v) const { return v.as<int>(); }

// ---- uint64_t specializations ----
template<> inline String Metric<uint64_t>::_toString(uint64_t val) const { return String((unsigned long)val); }
template<> inline bool Metric<uint64_t>::_fromString(const String& s, uint64_t& out) const { out = (uint64_t)s.toInt(); return true; }
template<> inline void Metric<uint64_t>::_toJson(JsonObject& obj, const char* key, uint64_t val) const { obj[key] = (unsigned long)val; }
template<> inline uint64_t Metric<uint64_t>::_fromJson(JsonVariantConst v) const { return (uint64_t)v.as<unsigned long>(); }
