#pragma once

class CLoginService;
class CStrategyService;
class CHQMarketService;
class CTradeAccountService;

struct CTraderServices
{
	CLoginService& m_login;
	CStrategyService& m_strategy;
	CHQMarketService& m_market;
	CTradeAccountService& m_account;
};
