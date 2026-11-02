#include "CLoginService.h"
#include "CSession.h"

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
	CSession::InstanceRef().Authenticate(param, [this](const AuthEvent& event)
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
	CSession::InstanceRef().CancelAuthentication();
	m_busy = false;
}

bool CLoginService::IsBusy() const noexcept
{
	return m_busy;
}
