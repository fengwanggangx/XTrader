#ifndef __JSONVALUE_H__
#define __JSONVALUE_H__
#include <rapidjson/document.h>
#include <string>

namespace json
{
	using _TyDocument = rapidjson::Document;
	using _TyAllocator = rapidjson::Document::AllocatorType;
	using _TyValue = rapidjson::Value;

	class JsonValue
	{
	public:
		JsonValue() = default;
		JsonValue(_TyValue* pValue, _TyAllocator* pAllocator);

		bool IsNull() const;
		bool IsBool() const;
		bool IsInt() const;
		bool IsUint() const;
		bool IsInt64() const;
		bool IsUint64() const;
		bool IsDouble() const;
		bool IsString() const;
		bool IsArray() const;
		bool IsObject() const;

		bool GetBool() const;
		int GetInt() const;
		unsigned GetUint() const;
		int64_t GetInt64() const;
		uint64_t GetUint64() const;
		double GetDouble() const;
		std::string GetString() const;

		std::size_t Size() const;

		JsonValue operator[](std::size_t nIdx);

		bool HasMember(const std::string& strKey) const;
		JsonValue operator[](const std::string& strKey);

		void AddMember(const std::string& strKey, bool value);
		void AddMember(const std::string& strKey, int value);
		void AddMember(const std::string& strKey, unsigned value);
		void AddMember(const std::string& strKey, int64_t value);
		void AddMember(const std::string& strKey, uint64_t value);
		void AddMember(const std::string& strKey, double value);
		void AddMember(const std::string& strKey, const std::string& value);
		JsonValue AddObjectMember(const std::string& strKey);
		JsonValue AddArrayMember(const std::string& strKey);

		void PushBack(bool value);
		void PushBack(int value);
		void PushBack(unsigned value);
		void PushBack(int64_t value);
		void PushBack(uint64_t value);
		void PushBack(double value);
		void PushBack(const std::string& value);
		JsonValue PushBackObject();
		JsonValue PushBackArray();

	private:
		_TyValue* m_pValue{ nullptr };
		_TyAllocator* m_pAllocator{ nullptr };
	};
} // namespace json

#endif
