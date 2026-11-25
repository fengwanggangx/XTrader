#ifndef __JSONDOCUMENT_H__
#define __JSONDOCUMENT_H__

#include <string>
#include "JsonValue.h"
#include <rapidjson/document.h>

namespace json
{
	class JsonDocument
	{
	public:
		JsonDocument();
		JsonDocument(const std::string& strJson);

		bool Parse(const std::string& strJson);

		std::string ToString(bool bPretty = false) const;

		JsonValue Root();

		bool IsWithError() const
		{
			return m_bError;
		}
		std::string GetErrorMsg() const
		{
			return m_strErrorMsg;
		}
		_TyDocument* GetDocument()
		{
			return &m_document;
		}

	private:
		_TyDocument m_document;
		bool m_bError{ false };
		std::string m_strErrorMsg;
	};

} // namespace json

#endif
