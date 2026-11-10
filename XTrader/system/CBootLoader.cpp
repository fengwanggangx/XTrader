#include "CBootLoader.h"
#include "CTraderCenter.h"
#include "../service/strategy/CStrategyService.h"
#include "../service/hqmarket/CHQService.h"
#include "../service/account/CAccountService.h"
#include "../database/CDBEngine.h"
#include "../database/IDataBase.h"
#include "../network/common_net.h"
#include <ini/CINIHandler.h>
#include <exception>
#include <stdexcept>
#include <utility>

CBootLoader::CBootLoader() = default;

CBootLoader::~CBootLoader()
{
	Finalize();
}

bool CBootLoader::Initialize()
{
	if (m_bInitialized)
	{
		return true;
	}

	m_nErrorCode = 0;
	m_strLastError.clear();
	try
	{
		m_exec = std::filesystem::current_path();
		if (!net::EnvInitialize())
		{
			m_nErrorCode = 1;
			m_strLastError = "Failed to enable libevent thread support";
			return false;
		}
		m_bNetInitialized = true;

		ini::CINIHandler& hIni = ini::CINIHandler::InstanceRef();
		int nSqlitePoolSize = hIni.GetInt(ini::Config::System, "Sqlite", "pool_size");
		std::string strSqliteDB = hIni.GetString(ini::Config::System, "Sqlite", "db_name");
		if ((nSqlitePoolSize <= 0) || strSqliteDB.empty())
		{
			m_nErrorCode = 2;
			m_strLastError = "Sqlite parameters error";
			Finalize();
			return false;
		}

		db::CConnectParam dbParam("", 0, "", "", (m_exec / strSqliteDB).string(), "");
		m_bDatabaseInitialized = true;
		if (0 != CDBEngine::InstanceRef().Initialize(db::em_database::sqlite, dbParam, 4))
		{
			m_nErrorCode = 3;
			m_strLastError = "Sqlite initialization failed";
			Finalize();
			return false;
		}
		db::_TyDBPtr db = CDBEngine::InstanceRef().GetDBPtr(db::em_database::sqlite);
		if ((nullptr == db) || (0 != db->ExecSqlFile(m_exec / "sql" / "table_create.sql")))
		{
			m_nErrorCode = 4;
			m_strLastError = "Failed to initialize Sqlite tables";
			Finalize();
			return false;
		}

		m_pSession = new CSession();
		m_pLoginService = new CLoginService();
		m_pStrategyService = new CStrategyService();
		m_pHQService = new CHQService();
		m_pAccountService = new CAccountService();
		m_pServices = new CTraderServices{ *m_pLoginService, *m_pStrategyService, *m_pHQService, *m_pAccountService };
		m_pTraderCenter = new CTraderCenter();
		m_pTraderCenter->Initialize(*m_pSession, *m_pServices);
		m_pLoginService->SetTraderCenter(m_pTraderCenter);
		m_pStrategyService->SetTraderCenter(m_pTraderCenter);
		m_pHQService->SetTraderCenter(m_pTraderCenter);
		m_pAccountService->SetTraderCenter(m_pTraderCenter);
		m_bInitialized = true;
		return true;
	}
	catch (const std::exception& error)
	{
		Finalize();
		m_nErrorCode = 5;
		m_strLastError = std::string("Service initialization failed: ") + error.what();
		return false;
	}
	catch (...)
	{
		Finalize();
		throw;
	}
}

bool CBootLoader::Run()
{
	if (!m_bInitialized)
	{
		m_nErrorCode = 4;
		m_strLastError = "Boot loader is not initialized";
		return false;
	}
	return true;
}

void CBootLoader::Stop()
{
	if (nullptr != m_pSession)
	{
		m_pSession->Stop();
	}
}

void CBootLoader::Finalize()
{
	Stop();
	if (nullptr != m_pTraderCenter)
	{
		m_pTraderCenter->Finalize();
	}
	if (nullptr != m_pLoginService)
	{
		m_pLoginService->SetTraderCenter(nullptr);
	}
	if (nullptr != m_pStrategyService)
	{
		m_pStrategyService->SetTraderCenter(nullptr);
	}
	if (nullptr != m_pHQService)
	{
		m_pHQService->SetTraderCenter(nullptr);
	}
	if (nullptr != m_pAccountService)
	{
		m_pAccountService->SetTraderCenter(nullptr);
	}

	delete std::exchange(m_pTraderCenter, nullptr);
	delete std::exchange(m_pServices, nullptr);
	delete std::exchange(m_pAccountService, nullptr);
	delete std::exchange(m_pHQService, nullptr);
	delete std::exchange(m_pStrategyService, nullptr);
	delete std::exchange(m_pLoginService, nullptr);
	delete std::exchange(m_pSession, nullptr);
	if (m_bDatabaseInitialized)
	{
		CDBEngine::InstanceRef().Close(db::em_database::sqlite);
		m_bDatabaseInitialized = false;
	}
	if (m_bNetInitialized)
	{
		net::EnvCleanup();
		m_bNetInitialized = false;
	}
	m_bInitialized = false;
}

const std::string& CBootLoader::GetLastError() const
{
	return m_strLastError;
}

int CBootLoader::GetErrorCode() const
{
	return m_nErrorCode;
}

CTraderServices& CBootLoader::GetServices()
{
	if (nullptr == m_pServices)
	{
		throw std::logic_error("Boot loader services are not initialized");
	}
	return *m_pServices;
}
