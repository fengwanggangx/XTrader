#ifndef __JSON_H__
#define __JSON_H__

#include "./common/ISingleton.h"

#include <string>

namespace json
{
	class Parser final : public ISingleton<Parser>
	{
		DECLARE_SINGLE_DFAULT(Parser)

	public:
		bool LoadFromFile(const std::string& strFileName);
	};
} // namespace json
#endif
