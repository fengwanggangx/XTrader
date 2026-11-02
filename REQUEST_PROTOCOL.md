# 请求协议

XTrader、XEngine、HQMarket 使用相同 RequestData 定义，必须一起升级。本次为不兼容的协议调整，旧客户端不再支持。

- C++ 枚举定义为 `request::types`，通过全局别名 `using _TyRequest = request::types;` 使用。Protobuf 的 RequestType 及其数值保持不变。
- Type 唯一决定业务操作，不再发送 cmd。删除的字段编号和旧分类枚举编号均保留，不复用。
- 请求和响应使用相同 Type、id；行情主动推送外层 id=0。
- 所有响应使用 error_code / error_message。error_code=0 表示成功；失败为非零，客户端不能仅凭错误描述是否为空判断成功。
- ret 已移除；认证结果、心跳时间和普通返回参数通过 ExtraData 返回。
- 查询条件、登录凭据、订阅条件放入 ExtraData。复杂列表必须是 JSON 数组字符串，不使用逗号拼接。
- 结构化行数据、列表数据使用 payload。STRATEGY_ADD/MODIFY 沿用 StrategyInfo 作为完整策略配置。

## Type 与 payload

| Type | 请求 payload | 响应/推送 payload |
| --- | --- | --- |
| UNKNOWN (0) | 无 | 无 |
| AUTH_LOGIN (1) | 无 | 无 |
| AUTH_REGISTER (3) | 无 | 无 |
| HEARTBEAT (6) | 无 | 无 |
| TRADE_ACCOUNT_LOGIN (8) | 无 | TradeAccountRecord（通道接入后） |
| TRADE_ACCOUNT_LIST_QUERY (9) | 无 | TradeAccountList |
| TRADE_ACCOUNT_ADD (10) | 无 | TradeAccountRecord |
| TRADE_ACCOUNT_MODIFY (11) | 无 | TradeAccountRecord |
| TRADE_ACCOUNT_CONNECT (12) | 无 | TradeAccountRecord |
| TRADE_ACCOUNT_DISCONNECT (13) | 无 | TradeAccountRecord |
| TRADE_FUNDS_QUERY (14) | 无 | TradeAccountInfoList |
| TRADE_ACCOUNT_LOGS_QUERY (15) | 无 | TradeAccountLogList |
| STRATEGY_ADD (20) | StrategyInfo | StrategyInfo |
| STRATEGY_MODIFY (21) | StrategyInfo | StrategyInfo |
| STRATEGY_QUERY (22) | 无 | StrategyList |
| STRATEGY_DELETE (23) | 无 | 无 |
| STRATEGY_RUNTIME_QUERY (24) | 无 | StrategyRuntimeList |
| STRATEGY_START (25) | 无 | 无 |
| STRATEGY_PAUSE (26) | 无 | 无 |
| STRATEGY_STOP (27) | 无 | 无 |
| MARKET_SUBSCRIBE (40) | 无 | SubscriptionAck |
| MARKET_UNSUBSCRIBE (41) | 无 | SubscriptionAck |
| MARKET_QUOTE_QUERY (42) | 无 | QueryResponse |
| MARKET_BARS_QUERY (43) | 无 | QueryResponse |
| MARKET_SECURITIES_QUERY (44) | 无 | SecurityList |
| MARKET_SECTORS_QUERY (45) | 无 | SectorList |
| MARKET_SECTOR_CONSTITUENTS_QUERY (46) | 无 | SectorConstituents |
| MARKET_QUOTE_PUSH (47) | 服务端主动发送 | QuoteData |
| MARKET_DEPTH_PUSH (48) | 服务端主动发送 | DepthData |

## 认证

AUTH_LOGIN 的请求 ExtraData 使用 user/password，或者 token。成功响应 ExtraData 返回 user_id、user、token。Token 再认证成功时允许不返回新 token，此时客户端保留原令牌。失败响应只返回状态，不回传密码或 token。AUTH_REGISTER 使用 user/password，成功后仍需登录。

## 交易账户

- LOGIN：broker、site、account、password。
- QUERY：查询全部账户，无 scope。目前是共享账户池，尚未实现按用户归属隔离。
- MODIFY：account_id、name。
- CONNECT / DISCONNECT：account_id。
- INFO_QUERY：account_ids 为 JSON 正整数数组。单条 `[1001]`，多条 `[1001,1002]`，全部 `[]`。最多4096个输入ID，去重后按ID排序；始终返回 TradeAccountInfoList，空结果也带显式空列表。每行的 error_code/error_message 表示该账户是否可取得资金快照，外层状态表示请求是否成功受理。
- LOGS_QUERY：account_id；返回最近100条日志。

TradeAccountInfo 包含 account_id、balance、available、frozen、available_snapshot、server_time_ms、error_code、error_message，不含持仓或委托。

## 行情订阅

批量订阅的 ExtraData.subscriptions 是 JSON 对象数组，每行包含证券和通道，保留二者的配对关系：

```json
[{"security":"600000.SSE","channel":"quote"},{"security":"000001.SZSE","channel":"depth"}]
```

单项订阅也允许使用标量 security/channel。MARKET_SUBSCRIBE 和 MARKET_UNSUBSCRIBE 的响应保持原 Type；SubscriptionAck 为每条订阅返回 accepted/reason。取消订阅成功不能被客户端误标记为已订阅。

MARKET_BARS_QUERY 的 ExtraData：security、channel、begin_time_ms、end_time_ms。证券列表查询无参数；板块查询使用 sector_type，板块成分查询另加 sector_code。行情返回数据沿用 hqmarket.market.v1 中的行/列表 message。

## 实现边界

真实券商认证适配器仍未接入，TRADE_ACCOUNT_LOGIN 将返回明确错误，不会创建虚假登录状态。CONNECT 仍仅支持模拟交易通道。

## 验证

- `tests/test_trade_account_protocol.py`：跨三个项目编码解码、字段保留、认证元数据、资金批量响应、订阅确认和策略运行列表。
- `tests/RequestProtocolTests.cpp`：CRequest 的 JSON 参数、边界、序列化及请求构造。

在 Visual Studio x64 开发者命令行中，运行 `tests\run_request_protocol_tests.cmd`。测试产物写入临时目录，不加入项目源码目录。

## 统一交易柜台扩展

请求类型49–60、委托/成交/持仓行列表、账户快照与事件水位同步已加入三个项目。完整参数、状态和部署说明见 `XEngine/XEngine/counter/README.md`；本文件上表列出原基础协议，新增后共41个类型（含UNKNOWN）。
