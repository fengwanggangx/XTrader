#ifndef __CLOGGER_H__
#define __CLOGGER_H__
#include "spdlog/spdlog.h"
#include "./common/ISingleton.h"
#include "./common/modules.h"

class CLogger final : public ISingleton<CLogger>
{
	DECLARE_SINGLE_DFAULT(CLogger)

public:
	void ShutDown();
	std::shared_ptr<spdlog::logger> GetLogger(md::modules m);

private:
	void Initialize();
	std::string GetRootDir() const;
	std::shared_ptr<spdlog::logger> GetLogger(const std::string& strName);

private:
	std::string m_strDirRoot{ "./log/" };
	spdlog::level::level_enum m_level{ spdlog::level::level_enum::trace };
	spdlog::level::level_enum m_flush_level{ spdlog::level::level_enum::info };
};

#endif
