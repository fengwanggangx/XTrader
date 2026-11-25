#include "Json.h"
#include "rapidjson/document.h"
#include <rapidjson/filereadstream.h>

namespace json
{
	Parser::Parser()
	{
	}

	Parser::~Parser()
	{
	}

	bool Parser::LoadFromFile(const std::string& strFileName)
	{
		FILE* pFile = nullptr;
		errno_t err = fopen_s(&pFile, strFileName.c_str(), "rb");
		if ((0 != err) || (nullptr == pFile))
		{
			return false;
		}

		rapidjson::Document doc;
		char buffer[65536];
		rapidjson::FileReadStream istream(pFile, buffer, sizeof(buffer));
		doc.ParseStream(istream);
		fclose(pFile);

		if (doc.HasParseError())
		{
			// std::string strErr = "Parse error at offset " + std::to_string(m_pDoc->GetErrorOffset()) + ": " + GetParseError_En(m_pDoc->GetParseError());
			return false;
		}
		return true;
	}
} // namespace json