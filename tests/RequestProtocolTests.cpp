#include "../XTrader/request/RequestCenter.h"
#include "../XTrader/request/request.pb.h"
#include <iostream>
#include <limits>

namespace
{
	bool Check(bool bCondition, const char* pMessage)
	{
		if (!bCondition)
		{
			std::cerr << pMessage << '\n';
		}
		return bCondition;
	}
} // namespace

int main()
{
	CRequest req;
	req.SetType(_TyRequest::TRADE_FUNDS_QUERY);
	std::vector<std::uint64_t> ids{ 1, (std::numeric_limits<std::uint64_t>::max)() };
	req.SetExtraData("account_ids", ids);
	std::vector<std::uint64_t> parsed;
	if (!Check(req.GetExtraData("account_ids", parsed) && (ids == parsed), "uint64 JSON round-trip failed"))
	{
		return 1;
	}
	for (const auto& strInvalid : { "{}", "[0]", "[-1]", "[1.5]", "[\"1\"]", "[18446744073709551616]", "[1,]", "[1] trailing" })
	{
		req.SetExtraData("account_ids", std::string(strInvalid));
		if (!Check(!req.GetExtraData("account_ids", parsed) && parsed.empty(), "invalid account_ids accepted"))
		{
			return 1;
		}
	}
	req.SetExtraData("account_ids", std::string("[]"));
	if (!Check(req.GetExtraData("account_ids", parsed) && parsed.empty(), "empty all-accounts filter rejected"))
	{
		return 1;
	}
	req.SetExtraData("account_ids", std::vector<std::uint64_t>(4097, 1));
	if (!Check(!req.GetExtraData("account_ids", parsed), "oversized account filter accepted"))
	{
		return 1;
	}
	CRequest::_TyExtraRows rows{ { { "security", "600000.SSE" }, { "channel", "quote" } }, { { "security", "name\"\\.SZSE" }, { "channel", "depth" } } };
	req.SetExtraData("subscriptions", rows);
	CRequest::_TyExtraRows parsedRows;
	if (!Check(req.GetExtraData("subscriptions", parsedRows) && (rows == parsedRows), "JSON object-array escaping failed"))
	{
		return 1;
	}
	for (const auto& strInvalid : { "[1]", "[{}", "[{\"security\":1}]", "[{\"security\":\"a\",\"security\":\"b\"}]" })
	{
		req.SetExtraData("subscriptions", std::string(strInvalid));
		if (!Check(!req.GetExtraData("subscriptions", parsedRows) && parsedRows.empty(), "invalid subscription JSON accepted"))
		{
			return 1;
		}
	}
	CRequest auth = request::Auth(request::AuthAction::Login, "user", "secret");
	if (!Check((_TyRequest::AUTH_LOGIN == auth.GetType()) && (request::RequestData::PAYLOAD_NOT_SET == auth.GetData().payload_case()), "auth request must use metadata only"))
	{
		return 1;
	}
	if (!Check("user" == auth.GetExtraData("user") && auth.GetExtraData("account").empty(), "login metadata must name the system user"))
	{
		return 1;
	}
	CRequest registration = request::Auth(request::AuthAction::Register, "new-user", "secret");
	if (!Check(_TyRequest::AUTH_REGISTER == registration.GetType() && "new-user" == registration.GetExtraData("user") && registration.GetExtraData("account").empty(), "registration metadata must name the system user"))
	{
		return 1;
	}
	for (bool bSubscribe : { true, false })
	{
		std::vector<CSecurity> securities{ CSecurity("600000", Exchange::sse), CSecurity("000001", Exchange::szse) };
		std::vector<Channel> channels{ Channel::quote, Channel::depth };
		CRequest subscription = bSubscribe ? request::Subscription(securities, channels) : request::UnSubscription(securities, channels);
		if (!Check((bSubscribe ? _TyRequest::MARKET_SUBSCRIBE : _TyRequest::MARKET_UNSUBSCRIBE) == subscription.GetType(), "subscription operation type lost") || !Check(subscription.GetExtraData("subscriptions", parsedRows) && (4 == parsedRows.size()) && (request::RequestData::PAYLOAD_NOT_SET == subscription.GetData().payload_case()), "subscription parameters must be JSON metadata"))
		{
			return 1;
		}
	}
	CRequest response;
	response.SetType(_TyRequest::AUTH_LOGIN);
	response.SetId(auth.GetId());
	response.SetExtraData("user", std::string("user"));
	response.SetExtraData("token", std::string("test-token"));
	response.SetErrorInfo(1001, "");
	std::string strWire;
	CRequest decoded;
	if (!Check(response.Serialize(&strWire) && decoded.Deserialize(strWire) && decoded.GetErrorInfo().has_value() && (1001 == decoded.GetErrorInfo()->first) && (response.GetType() == decoded.GetType()) && (response.GetId() == decoded.GetId()), "typed response or empty-message error lost in serialization"))
	{
		return 1;
	}
	if (!Check("user" == decoded.GetExtraData("user") && decoded.GetExtraData("account").empty(), "auth response must preserve user without account alias"))
	{
		return 1;
	}
	decoded.SetErrorInfo(0, "");
	if (!Check(!decoded.GetErrorInfo().has_value(), "success status incorrectly treated as error"))
	{
		return 1;
	}
	std::cout << "Request protocol tests passed\n";
	return 0;
}
