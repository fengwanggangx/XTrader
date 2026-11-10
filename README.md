# XTrader

XTrade 的桌面交易终端，原项目名 Mary。配合 XEngine 策略服务、XBroker 交易柜台和 HQMarket 行情服务使用。

打开 `XTrader.sln`，使用 Visual Studio 的 Qt/MSBuild、Qt 6.11.2 MSVC 64 位及 C++20 工具链构建。主工程为 `XTrader/XTrader.vcxproj`，输出程序为 `XTrader.exe`；运行目录仍为 `XTrader/exec/Debug` 或 `XTrader/exec/Release`。

交易账户登录从 Broker 获取券商和站点目录，按 broker_id、site、account 登录；不使用旧模拟券商别名。账户页面显示已登录会话和分项资金查询，策略绑定保存完整账户身份，账号保留前导零。Broker、XEngine 和 XTrader 必须配套升级，旧账户编号和策略绑定不迁移。

共享协议采用 protoc 29.3 生成；四端协议检查可运行 `tests/test_trade_account_protocol.py`，通过 PROTOC 指定编译器路径。

系统登录身份使用 `user`，登录参数为 `m_strUser`，本地保存用户由 `CUserStorager` 读写 SQLite 的 `user` 表及 `user` 列；认证成功响应返回 `user_id/user/token`。交易账户继续使用 `account`。旧登录用户存储不迁移、不回退读取。


### 客户端请求收发分层

`CBootLoader` 通过裸指针独占管理 `CSession`、`CTraderCenter`、服务引用集合以及登录、策略、行情、账户 Service，在 `Initialize` 中创建，在 `Finalize` 中按依赖顺序释放并置空。Center 和 Service 通过显式注入建立关联，不再使用单例。界面通过 `CTraderServices` 借用这些服务；Qt Designer 创建的页面在创建后调用 `Initialize` 注入依赖。
业务请求统一经过 `Service → CTraderCenter → CSession → CTcpClient`。
业务回包经过 `CSession → CTraderCenter::OnRequest`，按协议类型路由到行情、账户或策略 Service，不再广播给全部业务处理器。
认证、心跳、重连与待处理请求超时仍由 Session 管理；业务解析、缓存和界面通知仍由各 Service 管理。
退出时先停止并等待会话线程及网络分发任务，再销毁界面及其订阅，解除 Center 和 Service 的关联，最后清理网络环境。
本次仅执行静态检查，未编译或运行测试。

账户身份统一使用 `(broker_id, account)`：broker_id 唯一标识柜台账户域，账号保留前导零。site 仅作为登录接入节点参数及账户展示信息，不参与资金回包匹配、账户选择或策略绑定。节点变化不产生新账户；当前 Broker 对已连接账户复用会话，重新选择 site 登录不会自动切换节点。后端账户和策略持久化已使用二元键，无需迁移为三元键。终端原三元编码仅用于界面选择，本次改为二元编码。

本次新增节点变化身份一致、不同券商隔离、二元编解码和无 site 策略绑定的回归测试源码；仅做静态检查，未编译或运行测试。

## 正式程序构建入口

正式程序统一从 Visual Studio 解决方案构建，以 `.vcxproj` 为准。仓库中的 CMake 入口仅用于辅助验证，不作为正式程序构建、发布或验收依据。

打开 `XTrader.sln`，选择 `XTrader/XTrader.vcxproj`，使用 Qt/MSBuild 和 Windows MSVC x64 工具链，配置为 Debug 或 Release。
本次仅核对解决方案及工程文件，不执行编译、链接或运行测试。
