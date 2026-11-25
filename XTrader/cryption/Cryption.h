#ifndef __CRYPTION_H__
#define __CRYPTION_H__

#include <string>
#include <vector>
#include <unordered_map>
#include <botan/aead.h>
#include "../common/ISingleton.h"

class Botan::AEAD_Mode;
namespace crypto
{
	enum class proc
	{
		encode = 0,
		decode
	};

	enum class algorithm
	{
		base64 = 0,
		aes_128_cbc,
		aes_256_cbc,
		aes_128_gcm,
		aes_256_gcm,
		chacha20_poly1305,
	};

	struct CParam
	{
		using T = std::vector<uint8_t>;
		using TRRef = T&&;
		using TCRef = const T&;
		T m_key;
		T m_iv;
		T m_lk;
		CParam() = default;
		CParam(TCRef key, TCRef iv, TCRef lk) : m_key(key), m_iv(iv), m_lk(lk)
		{
		}
		CParam(TRRef key, TRRef iv, TRRef lk) : m_key(std::move(key)), m_iv(std::move(iv)), m_lk(std::move(lk))
		{
		}

		bool IsEmpty() const
		{
			return m_key.empty() || m_iv.empty() || m_lk.empty();
		}
	};

	template <typename T, typename T1>
	T To(T1&& data)
	{
		return { data.begin(), data.end() };
	}

	class CCryptor final : public ISingleton<CCryptor>
	{
		DECLARE_SINGLE_DFAULT(CCryptor)

	public:
		std::string Encrypt(const std::string& strText, crypto::algorithm agm, const CParam& param);
		std::string Decrypt(const std::string& strText, crypto::algorithm agm, const CParam& param);

		std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& data, crypto::algorithm agm, const CParam& param);
		std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& data, crypto::algorithm agm, const CParam& param);
		bool ProtectCurrentUser(const std::vector<uint8_t>& data, std::vector<uint8_t>& protectedData, std::string& strError) const;
		bool UnprotectCurrentUser(const std::vector<uint8_t>& protectedData, std::vector<uint8_t>& data, std::string& strError) const;

	private:
		Botan::AEAD_Mode* GetCryptor(crypto::algorithm agm, crypto::proc ty);
		std::vector<uint8_t> Process(crypto::proc ty, const std::vector<uint8_t>& data, crypto::algorithm agm, const CParam& param);

	private:
		std::unordered_map<crypto::algorithm, std::pair<std::unique_ptr<Botan::AEAD_Mode>, std::unique_ptr<Botan::AEAD_Mode>>> m_crytors;
	};
} // namespace crypto

#endif
