#include "CTraderCenter.h"
#include "../service/account/CAccountService.h"
#include "../service/hqmarket/CHQService.h"
#include "../service/strategy/CStrategyService.h"
#include <utility>

CTraderCenter::CTraderCenter() = default;

CTraderCenter::~CTraderCenter()
{
	Finalize();
}

void CTraderCenter::Initialize(CSession& session, CTraderServices& services)
{
	if ((&session == m_pSession) && (&services == m_pServices))
	{
		return;
	}
	Finalize();
	m_pSession = &session;
	m_pServices = &services;
	session.SetResponseCallback(std::bind_front(&CTraderCenter::OnRequestReply, this));
}

void CTraderCenter::Finalize()
{
	if (nullptr != m_pSession)
	{
		m_pSession->Stop();
		m_pSession->SetResponseCallback({});
		m_pSession = nullptr;
		m_pServices = nullptr;
	}
}

bool CTraderCenter::SendRequest(const CRequest& request)
{
	return (nullptr != m_pSession) && m_pSession->SendRequest(request);
}

bool CTraderCenter::IsAuthenticated() const noexcept
{
	return (nullptr != m_pSession) && m_pSession->IsAuthenticated();
}

SessionState CTraderCenter::GetState() const noexcept
{
	return nullptr != m_pSession ? m_pSession->GetState() : SessionState::Disconnected;
}

void CTraderCenter::Authenticate(const CAuthParam& param, CSession::_TyAuthCallback&& callback)
{
	if (nullptr != m_pSession)
	{
		m_pSession->Authenticate(param, std::move(callback));
	}
	else if (nullptr != callback)
	{
		callback({ param.m_operation, AuthState::Failed, AuthError::ConnectFailed, false, "Session is not initialized" });
	}
}

void CTraderCenter::Stop()
{
	if (nullptr != m_pSession)
	{
		m_pSession->Stop();
	}
}

void CTraderCenter::CancelAuthentication()
{
	if (nullptr != m_pSession)
	{
		m_pSession->CancelAuthentication();
	}
}

void CTraderCenter::SetStateCallback(CSession::_TyStateCallback&& cb)
{
	if (nullptr != m_pSession)
	{
		m_pSession->SetStateCallback(std::move(cb));
	}
}

void CTraderCenter::RegisterStateHandler(CSession::_TyStateCallback&& cb)
{
	if (nullptr != m_pSession)
	{
		m_pSession->RegisterStateHandler(std::move(cb));
	}
}

bool CTraderCenter::OnRequestReply(const CRequest& req)
{
	if (nullptr == m_pServices)
	{
		return false;
	}

	_TyRequest t = req.GetType();
	switch (t)
	{
	case _TyRequest::MARKET_SUBSCRIBE:
	case _TyRequest::MARKET_UNSUBSCRIBE:
	case _TyRequest::MARKET_QUOTE_QUERY:
	case _TyRequest::MARKET_BARS_QUERY:
	case _TyRequest::MARKET_SECURITIES_QUERY:
	case _TyRequest::MARKET_SECTORS_QUERY:
	case _TyRequest::MARKET_SECTOR_CONSTITUENTS_QUERY:
	case _TyRequest::MARKET_QUOTE_PUSH:
	case _TyRequest::MARKET_DEPTH_PUSH:
		return m_pServices->m_hq.OnRequestReply(req);
	case _TyRequest::STRATEGY_ADD:
	case _TyRequest::STRATEGY_MODIFY:
	case _TyRequest::STRATEGY_QUERY:
	case _TyRequest::STRATEGY_DELETE:
	case _TyRequest::STRATEGY_RUNTIME_QUERY:
	case _TyRequest::STRATEGY_START:
	case _TyRequest::STRATEGY_PAUSE:
	case _TyRequest::STRATEGY_STOP:
		return m_pServices->m_strategy.OnRequestReply(req);
	case _TyRequest::TRADE_ACCOUNT_LOGIN:
	case _TyRequest::TRADE_ACCOUNT_LIST_QUERY:
	case _TyRequest::TRADE_ACCOUNT_DISCONNECT:
	case _TyRequest::TRADE_FUNDS_QUERY:
	case _TyRequest::TRADE_ORDER_SUBMIT:
	case _TyRequest::TRADE_ORDER_CANCEL:
	case _TyRequest::TRADE_POSITION_QUERY:
	case _TyRequest::TRADE_ORDER_QUERY:
	case _TyRequest::TRADE_DEAL_QUERY:
	case _TyRequest::TRADE_ACCOUNT_STATE_PUSH:
	case _TyRequest::TRADE_ORDER_PUSH:
	case _TyRequest::TRADE_TRADE_PUSH:
	case _TyRequest::TRADE_ACCOUNT_INFO_PUSH:
	case _TyRequest::TRADE_POSITION_PUSH:
	case _TyRequest::SUPPORTED_BROKER_QUERY:
		return m_pServices->m_account.OnRequestReply(req);
	default:
		return false;
	}
}
