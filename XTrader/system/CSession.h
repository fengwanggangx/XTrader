#ifndef XTRADER_SERVER_CSESSION_H
#define XTRADER_SERVER_CSESSION_H

#include "../common/ISingleton.h"
#include "../request/request.pb.h"
#include "CLoginService.h"
#include "../request/RequestCenter.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace net
{
	class CTcpClient;
	struct CNetEvent;
} // namespace net

enum class SessionState
{
	Disconnected,
	Connecting,
	Connected,
	Authenticating,
	Ready,
	Reconnecting,
	Stopping
};

class CSession final : public ISingleton<CSession>
{
	friend class CReviewRegressionTests;
	DECLARE_SINGLE_DFAULT(CSession)

	public:
	using AuthCallback = std::function<void(const AuthEvent&)>;
	using StateCallback = std::function<void(SessionState, const std::string&)>;
	using ErrorCallback = std::function<void(const std::string&)>;

	void Authenticate(const CAuthParam& param, AuthCallback&& cb);
	void CancelAuthentication();
	void Stop();
	bool SendRequest(const CRequest& request);
	void SetStateCallback(StateCallback&& cb);
	void RegisterStateHandler(StateCallback&& cb);
	void SetResponseCallback(_TyRequestNotify&& cb);
	void RegisterResponseHandler(_TyRequestNotify&& cb);
	void SetErrorCallback(ErrorCallback&& cb);
	bool IsAuthenticated() const noexcept;
	SessionState GetState() const noexcept;
	std::optional<CLoginInfo> GetLoginInfo() const;

	private:
	struct PendingRequest
	{
		_TyRequest m_type{ _TyRequest::UNKNOWN };
		std::chrono::steady_clock::time_point m_deadline;
	};

	struct AuthContext
	{
		CAuthParam m_param;
		AuthCallback m_callback;
	};

	struct AuthRequest
	{
		_TyRequestId m_id{ 0 };
		_TyRequest m_type{ _TyRequest::UNKNOWN };
		std::chrono::steady_clock::time_point m_deadline;
	};

	void StartConnection();
	void ConnectionLoop();
	void MaintenanceLoop();
	int OnNetEvent(const net::CNetEvent& ev);
	void HandleResponse(const CRequest& response);
	void SendAuthentication();
	void FailAuthentication(_TyRequestId id, const std::string& message);
	void FailPending(const std::string& reason);
	void NotifyAuthentication(AuthEvent ev);
	void NotifyState(SessionState state, const std::string& message);
	void NotifyResponse(const CRequest& response);
	void NotifyError(const std::string& error);

	private:
	mutable std::mutex m_mtx_client;
	std::unique_ptr<net::CTcpClient> m_client;

	private:
	std::atomic_bool m_stopping{ false };
	std::atomic<SessionState> m_state{ SessionState::Disconnected };

	std::thread m_thread_conn;		// 连接、收包、断线重连
	std::thread m_thread_heartbeat; // 发送心跳、检查请求超时

	std::mutex m_mtx_loops;
	std::condition_variable m_cv_loops;

	mutable std::mutex m_mtx_auth;
	std::optional<AuthContext> m_authContext;
	std::optional<AuthRequest> m_authRequest;
	std::optional<CLoginInfo> m_loginInfo;

	std::mutex m_mtx_pending;
	std::unordered_map<_TyRequestId, PendingRequest> m_reqs_sendout;
	std::mutex m_mtx_callbacks;
	StateCallback m_stateCallback;
	std::vector<StateCallback> m_stateHandlers;
	std::vector<_TyRequestNotify> m_responseCallbacks;
	ErrorCallback m_errorCallback;

#if defined(_DEBUG)
	int m_heartbeatSeconds{ 7200 };
#else
	int m_heartbeatSeconds{ 15 };
#endif
	int m_timeoutSeconds{ 10 };
	int m_maxReconnectSeconds{ 30 };
};

#endif
