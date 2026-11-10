#pragma once
#include "CSession.h"
#include "CTraderServices.h"

class CTraderCenter final
{
public:
	CTraderCenter();
	~CTraderCenter();
	CTraderCenter(const CTraderCenter& arg) = delete;
	CTraderCenter& operator=(const CTraderCenter& arg) = delete;

	void Stop();
	void Finalize();
	void Initialize(CSession& session, CTraderServices& services);

	bool OnRequestReply(const CRequest& req);
	bool SendRequest(const CRequest& req);

	void CancelAuthentication();
	bool IsAuthenticated() const noexcept;
	SessionState GetState() const noexcept;
	void Authenticate(const CAuthParam& param, CSession::_TyAuthCallback&& cb);
	void SetStateCallback(CSession::_TyStateCallback&& cb);
	void RegisterStateHandler(CSession::_TyStateCallback&& cb);

private:
	// Borrowed from CBootLoader; stopped and drained before detaching.
	CSession* m_pSession{ nullptr };
	CTraderServices* m_pServices{ nullptr };
};
