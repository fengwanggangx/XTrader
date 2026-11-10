#ifndef HQMARKET_SYSTEM_CBOOTLOADER_H
#define HQMARKET_SYSTEM_CBOOTLOADER_H

#include "CSession.h"
#include "CTraderCenter.h"
#include "CLoginService.h"
#include "../service/strategy/CStrategyService.h"
#include "../service/hqmarket/CHQService.h"
#include "../service/account/CAccountService.h"

#include <string>
#include <filesystem>

class CBootLoader final
{
public:
	CBootLoader();
	~CBootLoader();
	CBootLoader(const CBootLoader& arg) = delete;
	CBootLoader& operator=(const CBootLoader& arg) = delete;

	CTraderServices& GetServices();
	bool Initialize();
	bool Run();
	void Stop();
	void Finalize();
	const std::filesystem::path& GetRoot() const;
	const std::string& GetLastError() const;
	int GetErrorCode() const;

private:
	// Owned here; Finalize drains callbacks before releasing dependencies.
	CSession* m_pSession{ nullptr };
	CLoginService* m_pLoginService{ nullptr };
	CStrategyService* m_pStrategyService{ nullptr };
	CHQService* m_pHQService{ nullptr };
	CAccountService* m_pAccountService{ nullptr };
	CTraderServices* m_pServices{ nullptr };
	CTraderCenter* m_pTraderCenter{ nullptr };
	std::filesystem::path m_exec;
	std::filesystem::path m_path_py_runtime;
	std::filesystem::path m_path_py_scripts;
	std::string m_strLastError;
	int m_nErrorCode{ 0 };
	bool m_bInitialized{ false };
	bool m_bNetInitialized{ false };
	bool m_bDatabaseInitialized{ false };
};

#endif
