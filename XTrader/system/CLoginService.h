#pragma once

#include "../basic/TMessagePump.h"
#include "CHostMgr.h"

#include <atomic>
#include <functional>
#include <string>

enum class AuthOperation
{
	Login,
	Register
};

struct CAuthParam
{
	AuthOperation m_operation{ AuthOperation::Login };
	std::string m_strUser;
	std::string m_strPassword;
	CHostInfo m_host;
};

struct CLoginInfo
{
	std::string m_strUser;
	std::string m_strToken;
	CHostInfo m_host;

	bool Valid() const noexcept;
};

enum class AuthState
{
	Idle,
	Connecting,
	Authenticating,
	Success,
	Failed,
	Cancelled
};

enum class AuthError
{
	None,
	InvalidInput,
	InvalidSite,
	ConnectFailed,
	AuthenticationFailed,
	NetworkError,
	Cancelled
};

struct AuthEvent
{
	AuthOperation m_operation{ AuthOperation::Login };
	AuthState m_state{ AuthState::Idle };
	AuthError m_error{ AuthError::None };
	bool m_success{ false };
	std::string m_message;
};

enum class SessionState;
class CTraderCenter;

class CLoginService final
{

public:
	CLoginService();
	~CLoginService();
	CLoginService(const CLoginService& arg) = delete;
	CLoginService& operator=(const CLoginService& arg) = delete;

	void SetTraderCenter(CTraderCenter* pCenter);
	bool IsAuthenticated() const noexcept;
	SessionState GetState() const noexcept;
	void SetStateCallback(std::function<void(SessionState, const std::string&)>&& callback);
	void RegisterStateHandler(std::function<void(SessionState, const std::string&)>&& callback);

	using _TyCallback = std::function<void(const AuthEvent&)>;

	_TyCallbackId Subscribe(_TyCallback&& callback);
	void Unsubscribe(_TyCallbackId id);
	bool Authenticate(const CAuthParam& param);
	void Cancel();
	bool IsBusy() const noexcept;

private:
	TMessagePump<AuthEvent> m_dispatcher_auth;
	std::atomic_bool m_busy{ false };
	// Borrowed from CBootLoader; detached after network callbacks drain.
	CTraderCenter* m_pTraderCenter{ nullptr };
};
