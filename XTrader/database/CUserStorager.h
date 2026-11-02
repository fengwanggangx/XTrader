#ifndef DATABASE_CUSERREPOSITORY_H
#define DATABASE_CUSERREPOSITORY_H

#include <cstdint>
#include <string>
#include <vector>

struct CUserInfo
{
	std::string m_strUser;
	std::vector<std::uint8_t> m_password;
	std::int64_t m_nCreateTime{ 0 };
	std::int64_t m_nUpdateTime{ 0 };
	std::int64_t m_nLastSuccessTime{ 0 };
};

class CUserStorager final
{
  public:
	  std::vector<CUserInfo> LoadUsers() const;
	  bool SaveUser(const std::string& strUser, const std::vector<std::uint8_t>& password, std::int64_t nTimestamp) const;
	  bool DeleteUser(const std::string& strUser) const;
};

#endif
