# XTrader

XTrade 的桌面交易终端，原项目名 Mary。配合 XEngine 策略服务、XBroker 交易柜台和 HQMarket 行情服务使用。

打开 `XTrader.sln`，使用 Visual Studio 的 Qt/MSBuild、Qt 6.11.2 MSVC 64 位及 C++20 工具链构建。主工程为 `XTrader/XTrader.vcxproj`，输出程序为 `XTrader.exe`；运行目录仍为 `XTrader/exec/Debug` 或 `XTrader/exec/Release`。

交易账户登录从 Broker 获取券商和站点目录，按 broker_id、site、account 登录；不使用旧模拟券商别名。账户页面显示已登录会话和分项资金查询，策略绑定保存完整账户身份，账号保留前导零。Broker、XEngine 和 XTrader 必须配套升级，旧账户编号和策略绑定不迁移。

共享协议采用 protoc 29.3 生成；四端协议检查可运行 `tests/test_trade_account_protocol.py`，通过 PROTOC 指定编译器路径。

系统登录身份使用 `user`，登录参数为 `m_strUser`，本地保存用户由 `CUserStorager` 读写 SQLite 的 `user` 表及 `user` 列；认证成功响应返回 `user_id/user/token`。交易账户继续使用 `account`。旧登录用户存储不迁移、不回退读取。
