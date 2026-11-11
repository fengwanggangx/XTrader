#pragma once

class CLoginService;
class CStrategyService;
class CHQService;
class CAccountService;
class CWatchlistService;

struct CTraderServices
{
	CLoginService& m_login;
	CStrategyService& m_strategy;
	CHQService& m_hq;
	CAccountService& m_account;
	CWatchlistService& m_watchlist;
};
