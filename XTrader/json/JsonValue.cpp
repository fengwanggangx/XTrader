#include "JsonValue.h"
#include <stdexcept>
#include "../defines.h"

namespace json
{
	JsonValue::JsonValue(_TyValue* pValue, _TyAllocator* pAllocator) : m_pValue(pValue), m_pAllocator(pAllocator)
	{
	}

	bool JsonValue::IsNull() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, false);
		return m_pValue->IsNull();
	}

	bool JsonValue::IsBool() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, false);
		return m_pValue->IsBool();
	}

	bool JsonValue::IsInt() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, false);
		return m_pValue->IsInt();
	}

	bool JsonValue::IsUint() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, false);
		return m_pValue->IsUint();
	}

	bool JsonValue::IsInt64() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, false);
		return m_pValue->IsInt64();
	}

	bool JsonValue::IsUint64() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, false);
		return m_pValue->IsUint64();
	}

	bool JsonValue::IsDouble() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, false);
		return m_pValue->IsDouble();
	}

	bool JsonValue::IsString() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, false);
		return m_pValue->IsString();
	}

	bool JsonValue::IsArray() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, false);
		return m_pValue->IsArray();
	}

	bool JsonValue::IsObject() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, false);
		return m_pValue->IsObject();
	}

	bool JsonValue::GetBool() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, false);
		if (!IsBool())
		{
			throw std::runtime_error("Value is not a boolean");
		}
		return m_pValue->GetBool();
	}

	int JsonValue::GetInt() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, -1);
		if (!IsInt())
		{
			throw std::runtime_error("Value is not an integer");
		}
		return m_pValue->GetInt();
	}

	unsigned JsonValue::GetUint() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, 0);
		if (!IsUint())
		{
			throw std::runtime_error("Value is not an unsigned integer");
		}
		return m_pValue->GetUint();
	}

	int64_t JsonValue::GetInt64() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, -1);
		if (!IsInt64())
		{
			throw std::runtime_error("Value is not a 64-bit integer");
		}
		return m_pValue->GetInt64();
	}

	uint64_t JsonValue::GetUint64() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, 0);
		if (!IsUint64())
		{
			throw std::runtime_error("Value is not an unsigned 64-bit integer");
		}
		return m_pValue->GetUint64();
	}

	double JsonValue::GetDouble() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, 0.00);
		if (!IsDouble())
		{
			throw std::runtime_error("Value is not a double");
		}
		return m_pValue->GetDouble();
	}

	std::string JsonValue::GetString() const
	{
		RETURN_EMTPTY_IFNULLPTR(m_pValue);
		if (!IsString())
		{
			throw std::runtime_error("Value is not a string");
		}
		return m_pValue->GetString();
	}

	std::size_t JsonValue::Size() const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, 0);
		if (!IsArray() && !IsObject())
		{
			throw std::runtime_error("Value is not an array or object");
		}
		return m_pValue->Size();
	}

	JsonValue JsonValue::operator[](std::size_t nIdx)
	{
		RETURN_EMTPTY_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsArray())
		{
			throw std::runtime_error("Value is not an array");
		}
		if (nIdx >= m_pValue->Size())
		{
			throw std::out_of_range("Array index out of range");
		}
		return JsonValue(&((*m_pValue)[nIdx]), m_pAllocator);
	}

	bool JsonValue::HasMember(const std::string& strKey) const
	{
		RETURN_VALUE_IFNULLPTR(m_pValue, false);
		if (!IsObject())
		{
			throw std::runtime_error("Value is not an object");
		}
		return m_pValue->HasMember(strKey.c_str());
	}

	JsonValue JsonValue::operator[](const std::string& strKey)
	{
		RETURN_EMTPTY_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsObject())
		{
			throw std::runtime_error("Value is not an object");
		}
		return JsonValue(&(*m_pValue)[strKey.c_str()], m_pAllocator);
	}

	void JsonValue::AddMember(const std::string& strKey, bool value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsObject())
		{
			throw std::runtime_error("Value is not an object");
		}
		_TyValue k(strKey.c_str(), *m_pAllocator);
		m_pValue->AddMember(k, _TyValue(value), *m_pAllocator);
	}

	void JsonValue::AddMember(const std::string& strKey, int value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsObject())
		{
			throw std::runtime_error("Value is not an object");
		}
		_TyValue k(strKey.c_str(), *m_pAllocator);
		m_pValue->AddMember(k, _TyValue(value), *m_pAllocator);
	}

	void JsonValue::AddMember(const std::string& strKey, unsigned value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsObject())
		{
			throw std::runtime_error("Value is not an object");
		}
		_TyValue k(strKey.c_str(), *m_pAllocator);
		m_pValue->AddMember(k, _TyValue(value), *m_pAllocator);
	}

	void JsonValue::AddMember(const std::string& strKey, int64_t value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsObject())
		{
			throw std::runtime_error("Value is not an object");
		}
		_TyValue k(strKey.c_str(), *m_pAllocator);
		m_pValue->AddMember(k, _TyValue(value), *m_pAllocator);
	}

	void JsonValue::AddMember(const std::string& strKey, uint64_t value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);

		if (!IsObject())
		{
			throw std::runtime_error("Value is not an object");
		}
		_TyValue k(strKey.c_str(), *m_pAllocator);
		m_pValue->AddMember(k, _TyValue(value), *m_pAllocator);
	}

	void JsonValue::AddMember(const std::string& strKey, double value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsObject())
		{
			throw std::runtime_error("Value is not an object");
		}
		_TyValue k(strKey.c_str(), *m_pAllocator);
		m_pValue->AddMember(k, _TyValue(value), *m_pAllocator);
	}

	void JsonValue::AddMember(const std::string& strKey, const std::string& value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsObject())
		{
			throw std::runtime_error("Value is not an object");
		}
		_TyValue k(strKey.c_str(), *m_pAllocator);
		_TyValue v(value.c_str(), *m_pAllocator);
		m_pValue->AddMember(k, v, *m_pAllocator);
	}

	JsonValue JsonValue::AddObjectMember(const std::string& strKey)
	{
		RETURN_EMTPTY_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsObject())
		{
			throw std::runtime_error("Value is not an object");
		}
		_TyValue k(strKey.c_str(), *m_pAllocator);
		_TyValue v(rapidjson::kObjectType);
		m_pValue->AddMember(k, v, *m_pAllocator);
		return JsonValue(&((*m_pValue)[strKey.c_str()]), m_pAllocator);
	}

	JsonValue JsonValue::AddArrayMember(const std::string& strKey)
	{
		RETURN_EMTPTY_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsObject())
		{
			throw std::runtime_error("Value is not an object");
		}
		_TyValue k(strKey.c_str(), *m_pAllocator);
		_TyValue v(rapidjson::kArrayType);
		m_pValue->AddMember(k, v, *m_pAllocator);
		return JsonValue(&((*m_pValue)[strKey.c_str()]), m_pAllocator);
	}

	void JsonValue::PushBack(bool value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsArray())
		{
			throw std::runtime_error("Value is not an array");
		}
		m_pValue->PushBack(_TyValue(value), *m_pAllocator);
	}

	void JsonValue::PushBack(int value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsArray())
		{
			throw std::runtime_error("Value is not an array");
		}
		m_pValue->PushBack(_TyValue(value), *m_pAllocator);
	}

	void JsonValue::PushBack(unsigned value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsArray())
		{
			throw std::runtime_error("Value is not an array");
		}
		m_pValue->PushBack(_TyValue(value), *m_pAllocator);
	}

	void JsonValue::PushBack(int64_t value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsArray())
		{
			throw std::runtime_error("Value is not an array");
		}
		m_pValue->PushBack(_TyValue(value), *m_pAllocator);
	}

	void JsonValue::PushBack(uint64_t value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsArray())
		{
			throw std::runtime_error("Value is not an array");
		}
		m_pValue->PushBack(_TyValue(value), *m_pAllocator);
	}

	void JsonValue::PushBack(double value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsArray())
		{
			throw std::runtime_error("Value is not an array");
		}
		m_pValue->PushBack(_TyValue(value), *m_pAllocator);
	}

	void JsonValue::PushBack(const std::string& value)
	{
		RETURN_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsArray())
		{
			throw std::runtime_error("Value is not an array");
		}
		_TyValue v(value.c_str(), *m_pAllocator);
		m_pValue->PushBack(v, *m_pAllocator);
	}

	JsonValue JsonValue::PushBackObject()
	{
		RETURN_EMTPTY_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsArray())
		{
			throw std::runtime_error("Value is not an array");
		}
		_TyValue v(rapidjson::kObjectType);
		m_pValue->PushBack(v, *m_pAllocator);
		return JsonValue(&((*m_pValue)[m_pValue->Size() - 1]), m_pAllocator);
	}

	JsonValue JsonValue::PushBackArray()
	{
		RETURN_EMTPTY_IFNULLPTR(m_pValue, m_pAllocator);
		if (!IsArray())
		{
			throw std::runtime_error("Value is not an array");
		}
		_TyValue v(rapidjson::kArrayType);
		m_pValue->PushBack(v, *m_pAllocator);
		return JsonValue(&((*m_pValue)[m_pValue->Size() - 1]), m_pAllocator);
	}
} // namespace json