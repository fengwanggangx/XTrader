#include "CSession.h"

#include "../network/CTcpClient.h"

#include <algorithm>
#include <utility>
#include <vector>

CSession::CSession() = default;

CSession::~CSession()
{
	Stop();
}

void CSession::Authenticate(const CAuthParam& param, AuthCallback&& cb)
{
	bool bRestart = false;
	{
		std::lock_guard<std::mutex> lock(m_mtx_auth);
		std::optional<CHostInfo> host;
		if (m_authContext.has_value())
		{
			host = m_authContext->m_param.m_host;
		}
		else if (m_loginInfo.has_value())
		{
			host = m_loginInfo->m_host;
		}
		bRestart = m_thread_conn.joinable() && (!host.has_value() || !(host.value() == param.m_host));
	}
	if (bRestart)
	{
		Stop();
	}

	{
		std::lock_guard<std::mutex> lock(m_mtx_auth);
		m_loginInfo.reset();
		m_authRequest.reset();
		m_authContext.emplace(AuthContext{ param, std::move(cb) });
	}

	SessionState state = m_state.load();
	if (!m_thread_conn.joinable())
	{
		StartConnection();
	}
	else if ((SessionState::Connected == state) || (SessionState::Ready == state) || (SessionState::Authenticating == state))
	{
		SendAuthentication();
	}
}

void CSession::CancelAuthentication()
{
	AuthCallback cb;
	AuthEvent ev;
	{
		std::lock_guard<std::mutex> lock(m_mtx_auth);
		if (!m_authContext.has_value())
		{
			return;
		}
		ev = { m_authContext->m_param.m_operation, AuthState::Cancelled, AuthError::Cancelled, false, "认证已取消" };
		cb = std::move(m_authContext->m_callback);
		m_authContext.reset();
		m_authRequest.reset();
	}
	if (nullptr != cb)
	{
		cb(ev);
	}
}

void CSession::StartConnection()
{
	m_stopping.store(false);
	m_thread_conn = std::thread(&CSession::ConnectionLoop, this);
	m_thread_heartbeat = std::thread(&CSession::MaintenanceLoop, this);
}

void CSession::Stop()
{
	if (!m_thread_conn.joinable() && !m_thread_heartbeat.joinable())
	{
		return;
	}

	m_stopping.store(true);
	m_state.store(SessionState::Stopping);
	NotifyState(SessionState::Stopping, "客户端正在关闭");
	m_cv_loops.notify_all();
	{
		std::lock_guard<std::mutex> lock(m_mtx_client);
		if (nullptr != m_client)
		{
			m_client->ShutDown();
		}
	}
	if (m_thread_heartbeat.joinable())
	{
		m_thread_heartbeat.join();
	}
	if (m_thread_conn.joinable())
	{
		m_thread_conn.join();
	}
	decltype(m_client) client;
	{
		std::lock_guard<std::mutex> lock(m_mtx_client);
		client = std::move(m_client);
	}
	if (nullptr != client)
	{
		client->Release();
	}
	FailPending("客户端已关闭");
	{
		std::lock_guard<std::mutex> lock(m_mtx_auth);
		m_authContext.reset();
		m_loginInfo.reset();
		m_authRequest.reset();
	}
	m_state.store(SessionState::Disconnected);
}

bool CSession::IsAuthenticated() const noexcept
{
	return SessionState::Ready == m_state.load();
}

SessionState CSession::GetState() const noexcept
{
	return m_state.load();
}

std::optional<CLoginInfo> CSession::GetLoginInfo() const
{
	std::lock_guard<std::mutex> lock(m_mtx_auth);
	return m_loginInfo;
}

void CSession::SetStateCallback(StateCallback&& cb)
{
	std::lock_guard<std::mutex> lock(m_mtx_callbacks);
	m_stateCallback = std::move(cb);
}

void CSession::RegisterStateHandler(StateCallback&& cb)
{
	std::lock_guard<std::mutex> lock(m_mtx_callbacks);
	m_stateHandlers.emplace_back(std::move(cb));
}

void CSession::SetResponseCallback(_TyRequestNotify&& cb)
{
	std::lock_guard<std::mutex> lock(m_mtx_callbacks);
	m_responseCallbacks.clear();
	m_responseCallbacks.emplace_back(std::move(cb));
}

void CSession::RegisterResponseHandler(_TyRequestNotify&& cb)
{
	std::lock_guard<std::mutex> lock(m_mtx_callbacks);
	m_responseCallbacks.emplace_back(std::move(cb));
}

void CSession::SetErrorCallback(ErrorCallback&& cb)
{
	std::lock_guard<std::mutex> lock(m_mtx_callbacks);
	m_errorCallback = std::move(cb);
}

void CSession::ConnectionLoop()
{
	int reconnectSeconds = 1;
	while (!m_stopping.load())
	{
		SessionState state = 1 == reconnectSeconds ? SessionState::Connecting : SessionState::Reconnecting;
		m_state.store(state);
		NotifyState(state, 1 == reconnectSeconds ? "正在连接" : std::to_string(reconnectSeconds) + " 秒后重连");

		std::optional<CHostInfo> host;
		{
			std::lock_guard<std::mutex> lock(m_mtx_auth);
			if (m_authContext.has_value())
			{
				host = m_authContext->m_param.m_host;
			}
			else if (m_loginInfo.has_value())
			{
				host = m_loginInfo->m_host;
			}
		}
		if (!host.has_value())
		{
			break;
		}
		std::unique_ptr<net::CTcpClient> client = std::make_unique<net::CTcpClient>(host->m_strHost, static_cast<int>(host->m_nPort));
		client->RegisterHandler(std::bind_front(&CSession::OnNetEvent, this));

		{
			std::lock_guard<std::mutex> lock(m_mtx_client);
			m_client = std::move(client);
		}
		int result = 0;
		{
			std::lock_guard<std::mutex> lock(m_mtx_client);
			result = m_client->Initialize();
		}
		if (0 == result)
		{
			m_client->Start(true);
		}
		else
		{
			NotifyError("连接初始化失败：" + std::to_string(result));
		}

		bool bAuthed = IsAuthenticated();
		m_state.store(SessionState::Disconnected);
		FailPending("连接已断开");
		{
			std::lock_guard<std::mutex> lock(m_mtx_client);
			client = std::move(m_client);
		}
		if (nullptr != client)
		{
			client->Release();
		}
		if (m_stopping.load())
		{
			break;
		}
		std::unique_lock<std::mutex> lock(m_mtx_loops);
		m_cv_loops.wait_for(lock, std::chrono::seconds(reconnectSeconds), [this]()
		{
			return m_stopping.load();
		});
		reconnectSeconds = bAuthed ? 1 : (std::min)(m_maxReconnectSeconds, reconnectSeconds * 2);
	}
	NotifyState(SessionState::Disconnected, "已关闭");
}

void CSession::MaintenanceLoop()
{
	std::chrono::steady_clock::time_point nextHeartbeat = std::chrono::steady_clock::now();
	while (!m_stopping.load())
	{
		std::unique_lock<std::mutex> lck(m_mtx_loops);
		m_cv_loops.wait_for(lck, std::chrono::milliseconds(250), [this]()
		{
			return m_stopping.load();
		});
		lck.unlock();
		if (m_stopping.load())
		{
			break;
		}

		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		_TyRequestId expiredAuthId = 0;
		{
			std::lock_guard<std::mutex> lock(m_mtx_auth);
			if (m_authRequest.has_value() && (m_authRequest->m_deadline <= now))
			{
				expiredAuthId = m_authRequest->m_id;
			}
		}
		if (0 != expiredAuthId)
		{
			FailAuthentication(expiredAuthId, "认证请求超时，请重试");
		}
		if (IsAuthenticated() && (nextHeartbeat <= now))
		{
			SendRequest(request::HeartBeat());
			nextHeartbeat = now + std::chrono::seconds(m_heartbeatSeconds);
		}
		std::vector<CRequest> expired;
		{
			std::lock_guard<std::mutex> lock(m_mtx_pending);
			for (auto mIter = m_reqs_sendout.begin(); m_reqs_sendout.end() != mIter;)
			{
				if (mIter->second.m_deadline <= now)
				{
					CRequest response;
					response.SetId(mIter->first);
					response.SetType(mIter->second.m_type);
					response.SetErrorInfo(-1, "请求超时");
					expired.emplace_back(std::move(response));
					mIter = m_reqs_sendout.erase(mIter);
				}
				else
				{
					++mIter;
				}
			}
		}
		for (const CRequest& response : expired)
		{
			NotifyResponse(response);
		}
	}
}

int CSession::OnNetEvent(const net::CNetEvent& ev)
{
	if (net::em_event::connected == ev.m_event)
	{
		m_state.store(SessionState::Connected);
		NotifyState(SessionState::Connected, "已连接");
		SendAuthentication();
		return 1;
	}
	if ((net::em_event::request == ev.m_event) && (nullptr != ev.m_request))
	{
		HandleResponse(*ev.m_request);
		return 1;
	}

	bool bAuthed = IsAuthenticated();
	m_state.store(SessionState::Disconnected);
	NotifyState(SessionState::Disconnected, "连接已断开");
	NotifyError(0 == ev.m_error ? "服务器主动关闭连接" : "网络错误：" + std::to_string(ev.m_error));
	if (bAuthed)
	{
		NotifyState(SessionState::Disconnected, "认证连接已断开");
	}
	return 1;
}

void CSession::HandleResponse(const CRequest& response)
{
	_TyRequestId id = response.GetId();
	{
		std::lock_guard<std::mutex> lock(m_mtx_pending);
		if (0 != id)
		{
			auto mIter = m_reqs_sendout.find(id);
			if ((m_reqs_sendout.end() != mIter) && (response.GetType() != mIter->second.m_type))
			{
				return;
			}
			m_reqs_sendout.erase(id);
		}
	}

	std::optional<std::pair<int, std::string>> errorInfo = response.GetErrorInfo();
	std::string strError;
	if (errorInfo.has_value())
	{
		strError = errorInfo->second;
	}

	_TyRequest t = response.GetType();
	if (_TyRequest::HEARTBEAT == t)
	{
		return;
	}

	if ((_TyRequest::AUTH_LOGIN == t) || (_TyRequest::AUTH_REGISTER == t))
	{
		AuthOperation op = _TyRequest::AUTH_LOGIN == t ? AuthOperation::Login : AuthOperation::Register;
		std::optional<CAuthParam> authParam;
		AuthCallback authCallback;
		{
			std::lock_guard<std::mutex> lock(m_mtx_auth);
			if (!m_authRequest.has_value() || (id != m_authRequest->m_id) || (response.GetType() != m_authRequest->m_type))
			{
				return;
			}
			m_authRequest.reset();
			if (m_authContext.has_value())
			{
				authParam = m_authContext->m_param;
				op = authParam->m_operation;
				authCallback = std::move(m_authContext->m_callback);
				m_authContext.reset();
			}
		}
		if (errorInfo.has_value() && (0 != errorInfo->first))
		{
			SessionState expected = SessionState::Authenticating;
			m_state.compare_exchange_strong(expected, SessionState::Connected);
			if (authParam.has_value())
			{
				if (nullptr != authCallback)
				{
					authCallback({ op, AuthState::Failed, AuthError::AuthenticationFailed, false, strError });
				}
			}
			else
			{
				{
					std::lock_guard<std::mutex> lock(m_mtx_auth);
					m_loginInfo.reset();
				}
				NotifyError("重新认证失败：" + strError);
			}
			return;
		}

		if (AuthOperation::Login == op)
		{
			if (authParam.has_value())
			{
				CLoginInfo info;
				info.m_strUser = authParam->m_strUser;
				info.m_strToken = response.GetExtraData("token");
				info.m_host = authParam->m_host;
				std::lock_guard<std::mutex> lock(m_mtx_auth);
				m_loginInfo = std::move(info);
			}
			m_state.store(SessionState::Ready);
			{
				std::lock_guard<std::mutex> lock(m_mtx_client);
				if (nullptr != m_client)
				{
#if defined(_DEBUG)
					m_client->SetReadTimeout(7200);
#else
					m_client->SetReadTimeout(45);
#endif
				}
			}
			NotifyState(SessionState::Ready, "已认证");
		}
		else
		{
			m_state.store(SessionState::Connected);
		}
		if (nullptr != authCallback)
		{
			authCallback({ op, AuthState::Success, AuthError::None, true, AuthOperation::Login == op ? "登录成功" : "注册成功" });
		}

		return;
	}

	NotifyResponse(response);
}

void CSession::SendAuthentication()
{
	CRequest request;
	AuthOperation operation = AuthOperation::Login;
	bool hasAuthContext = false;
	{
		std::lock_guard<std::mutex> lock(m_mtx_auth);
		if (m_authRequest.has_value())
		{
			return;
		}
		if (m_authContext.has_value())
		{
			hasAuthContext = true;
			operation = m_authContext->m_param.m_operation;
			request::AuthAction action = AuthOperation::Login == operation ? request::AuthAction::Login : request::AuthAction::Register;
			request = request::Auth(action, m_authContext->m_param.m_strUser, m_authContext->m_param.m_strPassword);
		}
		else if (m_loginInfo.has_value() && m_loginInfo->Valid())
		{
			request = request::Auth(m_loginInfo->m_strToken);
		}
		else
		{
			return;
		}
		m_authRequest.emplace(AuthRequest{ request.GetId(), request.GetType(), std::chrono::steady_clock::now() + std::chrono::seconds(m_timeoutSeconds) });
	}
	m_state.store(SessionState::Authenticating);
	if (hasAuthContext)
	{
		NotifyAuthentication({ operation, AuthState::Authenticating, AuthError::None, false, AuthOperation::Login == operation ? "正在认证" : "正在注册" });
	}
	else
	{
		NotifyState(SessionState::Authenticating, "正在重新认证");
	}
	if (!SendRequest(request))
	{
		FailAuthentication(request.GetId(), "认证请求发送失败");
	}
}

void CSession::FailAuthentication(_TyRequestId id, const std::string& message)
{
	CRequest response;
	response.SetId(id);
	{
		std::lock_guard<std::mutex> lock(m_mtx_auth);
		if (!m_authRequest.has_value() || (id != m_authRequest->m_id))
		{
			return;
		}
		response.SetType(m_authRequest->m_type);
	}
	response.SetErrorInfo(-1, message);
	HandleResponse(response);
}

bool CSession::SendRequest(const CRequest& req)
{
	_TyRequest t = req.GetType();
	bool bAuthRequest = (_TyRequest::AUTH_LOGIN == t) || (_TyRequest::AUTH_REGISTER == t);
	if (!bAuthRequest && !IsAuthenticated())
	{
		return false;
	}

	_TyRequestId id = req.GetId();
	_TyRequest type = req.GetType();
	if (!bAuthRequest)
	{
		std::lock_guard<std::mutex> lock(m_mtx_pending);
		m_reqs_sendout.emplace(id, PendingRequest{ type, std::chrono::steady_clock::now() + std::chrono::seconds(m_timeoutSeconds) });
	}
	bool bRet = false;
	{
		std::lock_guard<std::mutex> lock(m_mtx_client);
		bRet = (nullptr != m_client) && m_client->SendRequest(req);
	}
	if (!bRet)
	{
		if (!bAuthRequest)
		{
			std::lock_guard<std::mutex> lock(m_mtx_pending);
			if (0 != id)
			{
				m_reqs_sendout.erase(id);
			}
		}
	}
	return bRet;
}

void CSession::FailPending(const std::string& strReson)
{
	_TyRequestId authId = 0;
	{
		std::lock_guard<std::mutex> lock(m_mtx_auth);
		if (m_authRequest.has_value())
		{
			authId = m_authRequest->m_id;
		}
	}
	if (0 != authId)
	{
		FailAuthentication(authId, strReson);
	}
	std::vector<CRequest> failed;
	{
		std::lock_guard<std::mutex> lock(m_mtx_pending);
		failed.reserve(m_reqs_sendout.size());
		for (const auto& [id, pending] : m_reqs_sendout)
		{
			CRequest response;
			response.SetId(id);
			response.SetType(pending.m_type);
			response.SetErrorInfo(-1, strReson);
			failed.emplace_back(std::move(response));
		}
		m_reqs_sendout.clear();
	}
	for (const CRequest& response : failed)
	{
		NotifyResponse(response);
	}
}

void CSession::NotifyAuthentication(AuthEvent ev)
{
	AuthCallback cb;
	{
		std::lock_guard<std::mutex> lock(m_mtx_auth);
		if (!m_authContext.has_value())
		{
			return;
		}
		if ((AuthState::Success == ev.m_state) || (AuthState::Failed == ev.m_state) || (AuthState::Cancelled == ev.m_state))
		{
			cb = std::move(m_authContext->m_callback);
			m_authContext.reset();
		}
		else
		{
			cb = m_authContext->m_callback;
		}
	}
	if (nullptr != cb)
	{
		cb(ev);
	}
}

void CSession::NotifyState(SessionState state, const std::string& message)
{
	StateCallback cb;
	std::vector<StateCallback> handlers;
	{
		std::lock_guard<std::mutex> lock(m_mtx_callbacks);
		cb = m_stateCallback;
		handlers = m_stateHandlers;
	}
	if (nullptr != cb)
	{
		cb(state, message);
	}
	for (const StateCallback& handler : handlers)
	{
		if (nullptr != handler)
		{
			handler(state, message);
		}
	}
}

void CSession::NotifyResponse(const CRequest& response)
{
	std::vector<_TyRequestNotify> callbacks;
	{
		std::lock_guard<std::mutex> lock(m_mtx_callbacks);
		callbacks = m_responseCallbacks;
	}
	for (const _TyRequestNotify& cb : callbacks)
	{
		if (nullptr != cb)
		{
			cb(response);
		}
	}
}

void CSession::NotifyError(const std::string& error)
{
	ErrorCallback cb;
	{
		std::lock_guard<std::mutex> lock(m_mtx_callbacks);
		cb = m_errorCallback;
	}
	if (nullptr != cb)
	{
		cb(error);
	}
}
