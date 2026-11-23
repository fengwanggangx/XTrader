#ifndef __CLOGGER_H__
#define __CLOGGER_H__

#include "spdlog/spdlog.h"
#include "./common/modules.h"

#include <utility>

enum class Level
{
	Trace = spdlog::level::trace,
	Debug = spdlog::level::debug,
	Info = spdlog::level::info,
	Warning = spdlog::level::warn,
	Error = spdlog::level::err,
	Critical = spdlog::level::critical
};

class CLogger final
{
public:
	void ShutDown();
	static CLogger* Instance();
	static void Initialize(const std::string& project);
	std::shared_ptr<spdlog::logger> GetLogger(md::modules m);

	template <typename... _TyArg>
	static void Write(Level level, md::modules module, spdlog::format_string_t<_TyArg...> format, _TyArg&&... args)
	{
		std::shared_ptr<spdlog::logger> logger = Instance()->GetLogger(module);
		if (nullptr != logger)
		{
			logger->log(static_cast<spdlog::level::level_enum>(level), format, std::forward<_TyArg>(args)...);
		}
	}

private:
	CLogger();
	~CLogger();
	CLogger(const CLogger& arg) = delete;
	CLogger& operator=(const CLogger& arg) = delete;
	std::shared_ptr<spdlog::logger> GetLogger(const std::string& name);

	std::mutex m_registry_mutex;
	spdlog::sink_ptr m_sink;
};

#endif
