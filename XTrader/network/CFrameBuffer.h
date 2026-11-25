#ifndef XTRADER_NETWORK_CFRAMEBUFFER_H
#define XTRADER_NETWORK_CFRAMEBUFFER_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace net
{
	class CFrameBuffer final
	{
	public:
		CFrameBuffer();

		void Reset();
		bool HasError() const;
		static std::optional<std::string> Encode(const std::string& strPayload);
		std::optional<std::vector<std::string>> Decode(const void* pData, std::size_t nLength);

	private:
		static constexpr std::size_t MaxFrameSize{ 8U * 1024U * 1024U };
		std::vector<std::uint8_t> m_receiveBuffer;
		bool m_hasError{ false };
	};
} // namespace net

#endif
