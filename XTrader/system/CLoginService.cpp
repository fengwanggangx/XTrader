#include "CLoginService.h"
#include "CTraderCenter.h"

CLoginService::CLoginService()
{
}

CLoginService::~CLoginService()
{
	Cancel();
}

_TyCallbackId CLoginService::Subscribe(_TyCallback&& callback)
{
	return m_dispatcher_auth.Subscribe(std::move(callback));
}

void CLoginService::Unsubscribe(_TyCallbackId id)
{
	m_dispatcher_auth.Unsubscribe(id);
}

bool CLoginInfo::Valid() const noexcept
{
	return !m_strUser.empty() && !m_strToken.empty() && m_host.Valid();
}

bool CLoginService::Authenticate(const CAuthParam& param)
{
	if (nullptr == m_pTraderCenter)
	{
		m_dispatcher_auth.Notify({ param.m_operation, AuthState::Failed, AuthError::ConnectFailed, false, "客户端尚未初始化" });
		return false;
	}
	if (param.m_strUser.empty() || param.m_strPassword.empty())
	{
		m_dispatcher_auth.Notify({ param.m_operation, AuthState::Failed, AuthError::InvalidInput, false, "账号和密码不能为空" });
		return false;
	}

	if (!param.m_host.Valid())
	{
		m_dispatcher_auth.Notify({ param.m_operation, AuthState::Failed, AuthError::InvalidSite, false, "当前站点配置无效" });
		return false;
	}

	if (m_busy.exchange(true))
	{
		m_dispatcher_auth.Notify({ param.m_operation, AuthState::Failed, AuthError::InvalidInput, false, "已有认证请求正在处理，请稍后重试" });
		return false;
	}
	m_pTraderCenter->Authenticate(param, [this](const AuthEvent& event)
	{
		if (AuthState::Success == event.m_state || AuthState::Failed == event.m_state || AuthState::Cancelled == event.m_state)
		{
			m_busy = false;
		}
		m_dispatcher_auth.Notify(event);
	});
	return true;
}

void CLoginService::Cancel()
{
	if (nullptr != m_pTraderCenter)
	{
		m_pTraderCenter->CancelAuthentication();
	}
	m_busy = false;
}

bool CLoginService::IsBusy() const noexcept
{
	return m_busy;
}

void CLoginService::SetTraderCenter(CTraderCenter* pCenter)
{
	m_pTraderCenter = pCenter;
}

bool CLoginService::IsAuthenticated() const noexcept
{
	return (nullptr != m_pTraderCenter) && m_pTraderCenter->IsAuthenticated();
}

SessionState CLoginService::GetState() const noexcept
{
	return nullptr != m_pTraderCenter ? m_pTraderCenter->GetState() : SessionState::Disconnected;
}

void CLoginService::SetStateCallback(std::function<void(SessionState, const std::string&)>&& callback)
{
	if (nullptr != m_pTraderCenter)
	{
		m_pTraderCenter->SetStateCallback(std::move(callback));
	}
}

void CLoginService::RegisterStateHandler(std::function<void(SessionState, const std::string&)>&& callback)
{
	if (nullptr != m_pTraderCenter)
	{
		m_pTraderCenter->RegisterStateHandler(std::move(callback));
	}
}
