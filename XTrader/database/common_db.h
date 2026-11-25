#ifndef DATABASE_COMMON_DB_H
#define DATABASE_COMMON_DB_H

#include <string>

namespace db
{
	struct CConnectParam
	{
		CConnectParam(const std::string& host, unsigned int port, const std::string& account, const std::string& password, const std::string& database, const std::string& charset);
		CConnectParam(const std::string& parameter, char delimiter);
		std::string m_strHost;
		unsigned int m_nPort{ 0 };
		std::string m_strAccount;
		std::string m_strPasswd;
		std::string m_strDataBase;
		std::string m_strCharset;
	};

	enum class em_data_types
	{
		em_string = 0,
		em_int32,
		em_int64,
		em_double,
		em_bool,
		binary
	};

	enum class em_database
	{
		unknown = 0,
		mysql,
		oracle,
		sqlite
	};

	em_data_types GetDataType(em_database databaseType, int nativeType);
	std::string GetDBName(em_database databaseType);
	em_database GetDBType(const std::string& name);
} // namespace db

#endif
