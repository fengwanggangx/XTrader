#include "CFrameBuffer.h"

#include <limits>

namespace net
{
	CFrameBuffer::CFrameBuffer()
	{
		m_receiveBuffer.reserve(4096);
	}

	std::optional<std::vector<std::string>> CFrameBuffer::Decode(const void* pData, std::size_t nLength)
	{
		if (m_hasError || ((nullptr == pData) && (0 != nLength)))
		{
			return std::nullopt;
		}
		if (nLength > (std::numeric_limits<std::size_t>::max)() - m_receiveBuffer.size())
		{
			m_hasError = true;
			return std::nullopt;
		}
		if (0 != nLength)
		{
			const std::uint8_t* pBytes = static_cast<const std::uint8_t*>(pData);
			m_receiveBuffer.insert(m_receiveBuffer.end(), pBytes, pBytes + nLength);
		}

		std::vector<std::string> frames;
		std::size_t offset = 0;
		while (sizeof(std::uint32_t) <= m_receiveBuffer.size() - offset)
		{
			std::uint32_t frameLength = (static_cast<std::uint32_t>(m_receiveBuffer[offset]) << 24) | (static_cast<std::uint32_t>(m_receiveBuffer[offset + 1]) << 16) | (static_cast<std::uint32_t>(m_receiveBuffer[offset + 2]) << 8) | static_cast<std::uint32_t>(m_receiveBuffer[offset + 3]);
			if ((0 == frameLength) || (MaxFrameSize < frameLength))
			{
				m_hasError = true;
				return std::nullopt;
			}
			if (frameLength > m_receiveBuffer.size() - offset - sizeof(std::uint32_t))
			{
				break;
			}
			const char* pPayload = reinterpret_cast<const char*>(m_receiveBuffer.data() + offset + sizeof(std::uint32_t));
			frames.emplace_back(pPayload, frameLength);
			offset += sizeof(std::uint32_t) + frameLength;
		}
		if (0 != offset)
		{
			m_receiveBuffer.erase(m_receiveBuffer.begin(), m_receiveBuffer.begin() + static_cast<std::ptrdiff_t>(offset));
		}
		return frames;
	}

	void CFrameBuffer::Reset()
	{
		m_receiveBuffer.clear();
		m_hasError = false;
	}

	bool CFrameBuffer::HasError() const
	{
		return m_hasError;
	}

	std::optional<std::string> CFrameBuffer::Encode(const std::string& strPayload)
	{
		if (strPayload.empty() || (MaxFrameSize < strPayload.size()))
		{
			return std::nullopt;
		}
		std::uint32_t length = static_cast<std::uint32_t>(strPayload.size());
		std::string frame(sizeof(std::uint32_t), '\0');
		frame[0] = static_cast<char>((length >> 24) & 0xffU);
		frame[1] = static_cast<char>((length >> 16) & 0xffU);
		frame[2] = static_cast<char>((length >> 8) & 0xffU);
		frame[3] = static_cast<char>(length & 0xffU);
		frame.append(strPayload);
		return frame;
	}
} // namespace net
