#include "CLogger.h"

#include "spdlog/async.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_sinks.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <iostream>

namespace
{
	struct DailyFilename
	{
		static spdlog::filename_t calc_filename(const spdlog::filename_t& base, const std::tm& date)
		{
			auto [stem, extension] = spdlog::details::file_helper::split_by_extension(base);
			return spdlog::fmt_lib::format("{}_{:04}{:02}{:02}{}", stem,
				date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, extension);
		}
	};

	using _TyDailySink = spdlog::sinks::daily_file_sink<std::mutex, DailyFilename>;
} // namespace

CLogger* CLogger::Instance()
{
	static CLogger logger;
	return &logger;
}

CLogger::CLogger()
{
	// Construct the spdlog registry before the singleton so it survives ShutDown().
	(void)spdlog::default_logger();
}

void CLogger::Initialize(const std::string& project)
{
	CLogger* instance = Instance();
	if (nullptr != instance->m_sink)
	{
		return;
	}

	QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("logs/") + QString::fromStdString(project) + QStringLiteral(".log"));
	spdlog::sink_ptr sink;
	try
	{
		sink = std::make_shared<_TyDailySink>(QFile::encodeName(path).toStdString(), 0, 0);
	}
	catch (const spdlog::spdlog_ex& error)
	{
		std::cerr << "File logging initialization failed (" << path.toStdString() << "): " << error.what() << '\n';
		sink = std::make_shared<spdlog::sinks::stderr_sink_mt>();
	}

	spdlog::set_level(spdlog::level::trace);
	spdlog::flush_on(spdlog::level::info);
	spdlog::init_thread_pool(200, 1);
	spdlog::flush_every(std::chrono::seconds(5));
	instance->m_sink = std::move(sink);
}

CLogger::~CLogger()
{
	ShutDown();
}

void CLogger::ShutDown()
{
	if (nullptr != m_sink)
	{
		spdlog::shutdown();
		m_sink->flush();
		m_sink.reset();
	}
}

std::shared_ptr<spdlog::logger> CLogger::GetLogger(md::modules m)
{
	std::string name = GetModuleName(m);
	if (name.empty())
	{
		return nullptr;
	}
	return GetLogger(name);
}

std::shared_ptr<spdlog::logger> CLogger::GetLogger(const std::string& name)
{
	std::lock_guard<std::mutex> lock(m_registry_mutex);
	if (nullptr == m_sink)
	{
		return spdlog::default_logger();
	}
	std::shared_ptr<spdlog::logger> logger = spdlog::get(name);
	if (nullptr == logger)
	{
		logger = std::make_shared<spdlog::async_logger>(name, m_sink,
			spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
		logger->set_level(spdlog::level::trace);
		logger->flush_on(spdlog::level::info);
		spdlog::register_logger(logger);
	}
	return logger;
}
