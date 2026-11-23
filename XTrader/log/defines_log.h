#ifndef __LOGGER_DEFINES_H__
#define __LOGGER_DEFINES_H__
#include "CLogger.h"

#define LOGGER CLogger::Instance()
#define MLOGGER(md) LOGGER->GetLogger(md)

#define MLOG_TRACE(md, ...) CLogger::Write(Level::Trace, md, __VA_ARGS__)
#define MLOG_DEBUG(md, ...) CLogger::Write(Level::Debug, md, __VA_ARGS__)
#define MLOG_INFO(md, ...) CLogger::Write(Level::Info, md, __VA_ARGS__)
#define MLOG_WARN(md, ...) CLogger::Write(Level::Warning, md, __VA_ARGS__)
#define MLOG_ERR(md, ...) CLogger::Write(Level::Error, md, __VA_ARGS__)
#define MLOG_TRIVAL(md, ...) CLogger::Write(Level::Critical, md, __VA_ARGS__)

#define GLOBAL_LOG_TRACE(...) MLOG_TRACE(md::modules::global, __VA_ARGS__)
#define GLOBAL_LOG_DEBUG(...) MLOG_DEBUG(md::modules::global, __VA_ARGS__)
#define GLOBAL_LOG_INFO(...) MLOG_INFO(md::modules::global, __VA_ARGS__)
#define GLOBAL_LOG_WARN(...) MLOG_WARN(md::modules::global, __VA_ARGS__)
#define GLOBAL_LOG_ERR(...) MLOG_ERR(md::modules::global, __VA_ARGS__)
#define GLOBAL_LOG_TRIVAL(...) MLOG_TRIVAL(md::modules::global, __VA_ARGS__)

#endif
