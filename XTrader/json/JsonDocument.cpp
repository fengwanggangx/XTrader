#include "JsonDocument.h"
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include "../defines.h"
namespace json
{
	JsonDocument::JsonDocument() : m_bError(false)
	{
		m_document.SetObject();
	}

	JsonDocument::JsonDocument(const std::string& strJson) : m_bError(false)
	{
		Parse(strJson);
	}

	bool JsonDocument::Parse(const std::string& strJson)
	{
		m_bError = false;
		m_strErrorMsg.clear();

		rapidjson::ParseResult ret = m_document.Parse(strJson.c_str());
		if (!ret.IsError())
		{
			m_bError = true;
			m_strErrorMsg = std::string("JSON parse error: ") + rapidjson::GetParseError_En(ret.Code()) + " at offset " + std::to_string(ret.Offset());
			return false;
		}
		return true;
	}

	std::string JsonDocument::ToString(bool bPretty) const
	{
		rapidjson::StringBuffer buffer;
		if (bPretty)
		{
			rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
			m_document.Accept(writer);
		}
		else
		{
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			m_document.Accept(writer);
		}
		return buffer.GetString();
	}

	JsonValue JsonDocument::Root()
	{
		return JsonValue(&m_document, &m_document.GetAllocator());
	}
} // namespace json