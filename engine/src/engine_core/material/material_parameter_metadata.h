﻿#pragma once

#include "material_parameter_types.h"
#include <string>
#include <json/json.hpp>

namespace ad_astris::ecore
{
	class MaterialParameterMetadata
	{
		public:
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(MaterialParameterMetadata, _name, _offset, _defaultValue, _precision)
		
			MaterialParameterMetadata() = default;
			MaterialParameterMetadata(
				const std::string& name,
				uint32_t offset,
				const MaterialParameterValue& defaultValue,
				MaterialParameterPrecision precision) : _name(name), _offset(offset), _defaultValue(defaultValue), _precision(precision) { }

			void set_name(const std::string& name) { _name = name; }
			void set_precision(MaterialParameterPrecision precision) { _precision = precision; }
			void set_default_value(const MaterialParameterValue& value)
			{
				if ((MaterialParameterType)value.index() == (MaterialParameterType)_defaultValue.index())
					_defaultValue = value;
			}

			size_t get_size() const { return internal::MATERIAL_PARAMETER_TYPE_SIZES[(int)(MaterialParameterType)_defaultValue.index()]; }
			uint32_t get_offset() const { return _offset; }
			std::string get_name() const { return _name; }
			MaterialParameterType get_type() const { return (MaterialParameterType)_defaultValue.index(); }
			MaterialParameterPrecision get_precision() const { return _precision; }
			const MaterialParameterValue& get_default_value() const { return _defaultValue; }
		
		private:
			std::string _name;
			uint32_t _offset{ 0 };
			MaterialParameterValue _defaultValue;
			MaterialParameterPrecision _precision{ MaterialParameterPrecision::UNDEFINED };	// Only for floats
	};
}
