# 交易账户登录

交易账户页的「登录账户」打开四行表单：券商、站点、账户、密码。登录按钮在左、取消在右。密码默认遮挡，可点击眼睛显示；请求失败后窗口保留并清空密码。

## 请求

- `type = TRADE_ACCOUNT_LOGIN (8)`
- `extra["broker"]`：券商配置键
- `extra["site"]`：所选券商下的站点配置键
- `extra["account"]`：交易账户
- `extra["password"]`：密码（不做 trim）
- 不使用 Protobuf 账户 payload；响应按 id 关联。

`TradeAccountInfo` 已移除。账户列表、策略账户选择等已有功能使用 `TradeAccountRecord` 响应记录，字段编号保持不变。账户名称修改也通过 extra["name"] 发送。

## 券商与站点

运行目录的 `ini/trade-accounts.ini` 定义选项。源文件示例位于 `XTrader/ini/trade-accounts.ini`；部署时复制到运行目录对应位置。没有配置时提供 XEngine 模拟服务和默认站点。

```ini
[brokers/wind_simulated]
name=XEngine 模拟服务

[brokers/wind_simulated/sites]
default=默认站点
```

## 服务端边界

XTrader 必须先完成 XEngine 用户认证。XEngine 按新的请求类型路由，校验必填字段与长度，错误响应清空密码。已配置的模拟账户通过统一异步柜台接口登录；这是模拟会话，不验证真实券商凭据。未配置的账户路由或真实券商适配器返回不支持，不会创建虚假实盘会话。详见 XEngine/XEngine/counter/README.md。

XTrader、XEngine、HQMarket 的枚举及生成协议同步更新；XTrader 和 XEngine 的响应类型需一起重新编译部署。

## 查询全部交易账户

请求仅含 `type=TRADE_ACCOUNT_LIST_QUERY (9)` 和 `id`；协议不再包含 cmd，不传 scope，也不传其他 extra。XEngine 按类型路由，要求用户已经认证，返回当前服务管理的全部交易账户（现有存储是共享账户池，没有按用户划分所有权）。

响应同 type、同 id，payload.account_list 为 TradeAccountList，元素为 TradeAccountRecord。成功时 error_code=0、error_message 为空；无账户也返回显式空 account_list。失败时 error_code 非零，error_message 为错误说明。所有请求统一使用 error_code/error_message 返回状态，普通返回字段使用 extra。协议已删除 cmd 和 ret，保留其字段编号；XTrader、XEngine、HQMarket 必须同步升级。
