#pragma once
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

using _TyAccountIndex = std::pair<std::uint64_t, std::string>;

inline bool IsAccountValid(const _TyAccountIndex& account)
{
	return 0 != account.first && !account.second.empty();
}

inline std::string EncodeAccount(const _TyAccountIndex& account)
{
	std::ostringstream stream;
	stream << account.first << ' ' << std::quoted(account.second);
	return stream.str();
}

inline bool DecodeAccount(const std::string& text, _TyAccountIndex& account)
{
	std::istringstream stream(text);
	_TyAccountIndex parsed;
	if (!(stream >> parsed.first >> std::quoted(parsed.second)))
	{
		return false;
	}
	stream >> std::ws;
	if (!stream.eof() || !IsAccountValid(parsed))
	{
		return false;
	}
	account = std::move(parsed);
	return true;
}

template <class T>
_TyAccountIndex AccountIndex(const T& account)
{
	return { account.broker_id(), account.account() };
}

template <class T>
void SetAccountIdentity(const _TyAccountIndex& account, T& target)
{
	target.set_broker_id(account.first);
	target.set_account(account.second);
}
