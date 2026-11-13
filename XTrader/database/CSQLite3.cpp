#include "CSQLite3.h"
#include <memory>
#include <sqlite3.h>

namespace db
{
	CSQLite3::CSQLite3()
	{
	}

	CSQLite3::~CSQLite3()
	{
		Close();
	}

	int CSQLite3::Connect(const CConnectParam& param)
	{
		if (nullptr != m_pDB)
		{
			return SQLITE_OK;
		}

		sqlite3* pDB = nullptr;
		int nRet = sqlite3_open_v2(param.m_strDataBase.c_str(), &pDB, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
		if (SQLITE_OK != nRet)
		{
			if (nullptr != pDB)
			{
				sqlite3_close(pDB);
			}
			return nRet;
		}

		m_pDB = pDB;
		return SQLITE_OK;
	}

	int CSQLite3::Close()
	{
		if (nullptr == m_pDB)
		{
			return SQLITE_OK;
		}

		int nRet = sqlite3_close(static_cast<sqlite3*>(m_pDB));
		if (SQLITE_OK == nRet)
		{
			m_pDB = nullptr;
		}
		return nRet;
	}

	int CSQLite3::ExecUpdate(const std::string& strSQL)
	{
		if (nullptr == m_pDB)
		{
			return SQLITE_MISUSE;
		}
		return sqlite3_exec(static_cast<sqlite3*>(m_pDB), strSQL.c_str(), nullptr, nullptr, nullptr);
	}

	const CQueryTable& CSQLite3::ExecQuery(const std::string& strSQL)
	{
		thread_local CQueryTable table;
		table.Clear();
		if (nullptr == m_pDB)
		{
			return table;
		}

		sqlite3_stmt* pStatement = nullptr;
		int nRet = sqlite3_prepare_v2(static_cast<sqlite3*>(m_pDB), strSQL.c_str(), -1, &pStatement, nullptr);
		std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(pStatement, sqlite3_finalize);
		if (SQLITE_OK != nRet)
		{
			return table;
		}

		int nCols = sqlite3_column_count(statement.get());
		table.m_columns.reserve(static_cast<std::size_t>(nCols));
		for (int i = 0; i < nCols; ++i)
		{
			table.m_columns.emplace_back();
			CQueryColumn& column = table.m_columns.back();
			column.m_uId = static_cast<unsigned int>(i);
			column.m_strName = sqlite3_column_name(statement.get(), i);
		}

		while (SQLITE_ROW == (nRet = sqlite3_step(statement.get())))
		{
			table.m_rows.emplace_back();
			_TyQueryRow& row = table.m_rows.back();
			row.reserve(static_cast<std::size_t>(nCols));
			for (int i = 0; i < nCols; ++i)
			{
				int nType = sqlite3_column_type(statement.get(), i);
				if (SQLITE_INTEGER == nType)
				{
					row.emplace_back(static_cast<std::int64_t>(sqlite3_column_int64(statement.get(), i)));
				}
				else if (SQLITE_FLOAT == nType)
				{
					row.emplace_back(sqlite3_column_double(statement.get(), i));
				}
				else if (SQLITE_BLOB == nType)
				{
					const std::uint8_t* pData = static_cast<const std::uint8_t*>(sqlite3_column_blob(statement.get(), i));
					int nSize = sqlite3_column_bytes(statement.get(), i);
					row.emplace_back(nullptr == pData ? std::vector<std::uint8_t>() : std::vector<std::uint8_t>(pData, pData + nSize));
				}
				else if (SQLITE_TEXT == nType)
				{
					const char* pszValue = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), i));
					int nSize = sqlite3_column_bytes(statement.get(), i);
					row.emplace_back(nullptr == pszValue ? std::string() : std::string(pszValue, static_cast<std::size_t>(nSize)));
				}
				else
				{
					row.emplace_back(std::monostate());
				}
			}
		}
		if (SQLITE_DONE != nRet)
		{
			table.Clear();
		}
		return table;
	}

	bool CSQLite3::BeginTransaction()
	{
		return SQLITE_OK == ExecUpdate("BEGIN IMMEDIATE");
	}

	bool CSQLite3::EndTransaction()
	{
		return SQLITE_OK == ExecUpdate("COMMIT");
	}

	bool CSQLite3::RollBackTransaction()
	{
		return SQLITE_OK == ExecUpdate("ROLLBACK");
	}
} // namespace db
