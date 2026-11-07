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

	bool OnRequest(const CRequest& request);
	bool SendRequest(const CRequest& request);

	void CancelAuthentication();
	bool IsAuthenticated() const noexcept;
	SessionState GetState() const noexcept;
	void Authenticate(const CAuthParam& param, CSession::_TyAuthCallback&& callback);
	void SetStateCallback(CSession::_TyStateCallback&& callback);
	void RegisterStateHandler(CSession::_TyStateCallback&& callback);

private:
	// Borrowed from CBootLoader; stopped and drained before detaching.
	CSession* m_pSession{ nullptr };
	CTraderServices* m_pServices{ nullptr };
};
