#ifndef __CNETCLIENT_H__
#define __CNETCLIENT_H__
#include <vector>
#include <string>
#include "CNet.h"
#include "CNetRouter.h"
#include <memory>
#include <functional>

template <bool bAsyn, class _Ty, class _TyHandler>
class CDistributor;

class CRequest;
namespace net
{
	class CNetClient final : public CNet, public CNetRouter<CNetClient>
	{
		using _TyData = std::unique_ptr<CRequest>;
		using _TyHandler = std::function<int(const _TyData&)>;
		using _TyDistributor = CDistributor<true, std::vector<_TyData>, _TyHandler>;

	public:
		explicit CNetClient(const std::string& strAddr, int nPort);
		~CNetClient() = default;

	public:
		int Initialize();

		void RegisterHandler(_TyHandler&& func);

	public:
		std::size_t OnRead(struct bufferevent* pEvent) override;
		void OnEvent(struct bufferevent* pEvent, short events) override;
		void OnConnected(bufferevent* pEvent) override;

	private:
		std::string m_strAddr;
		int m_nPort{ -1 };

		std::vector<char> m_buffer_recv;
		std::vector<char> m_buffer_send;

		std::unique_ptr<_TyDistributor> m_dispatcher;

		evutil_socket_t m_fd{ -1 };
	};
} // namespace net
#endif
