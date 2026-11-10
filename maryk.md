# XTrader 行情图表与表格高性能实现方案

## 1. 最终选型

XTrader 当前是 Qt 6 + C++ Widgets，采用统一数据层、独立展示层：

```text
HQMarket
   ↓
CMarketDataGateway
   ↓
内存行情缓存 / 历史数据缓存
   ├── CQuoteModel       → QTableView
   ├── CKlineDataModel   → QwtPlotTradingCurve
   └── CIntradayModel    → QwtPlotCurve + 成交量柱状图
```

- K 线图：QWT `QwtPlotTradingCurve`
- 分时图：QWT `QwtPlotCurve` + 成交量曲线/柱状图
- 行情表格：`QTableView + QAbstractTableModel`
- 极限性能：后续按实测结果增加 `QOpenGLWidget` 或 `QRhi` 自定义渲染

推荐 QWT 仓库：<https://github.com/czyt1988/QWT>

## 2. K 线图

### 类型命名约定

实时行情快照类型统一命名为 `CQuote`，不再使用 `CQuoteSnapshot`。现有 XTrader 原型中的 `CMarketQuoteSnapshot` 后续统一迁移为 `CQuote`。

QWT 原生提供 `QwtPlotTradingCurve`，支持 Bar 和 CandleStick 两种金融交易曲线，适合日 K、周 K、月 K 和分钟 K。

建议布局：

```text
QwtPlot
 ├── 主图：QwtPlotTradingCurve
 ├── 均线：QwtPlotCurve × N
 ├── 买卖点：QwtPlotMarker / 自定义符号
 ├── 副图：成交量柱状图
 ├── 副图：MACD / KDJ / RSI
 └── 十字光标：QwtPlotPicker + 自定义 Overlay
```

需要实现：红涨绿跌、缩放、平移、十字光标、数据提示、指标叠加和主题切换。

QWT 新版本包含像素级降采样、LTTB/MinMax 降采样、SIMD 加速和 K 线拾取能力，可用于大规模历史行情及实时刷新。参考：<https://github.com/czyt1988/QWT/releases>

## 3. 分时图

分时图使用普通时间序列曲线，不使用蜡烛实体：

```text
主图：QwtPlotCurve       价格线
均价线：QwtPlotCurve     均价
昨收线：QwtPlotMarker    昨收价横线
成交量：QwtPlotHistogram 或 QwtPlotBarChart
```

建议数据结构：

```cpp
struct CIntradayPoint
{
    double m_fTimestamp;
    double m_fPrice;
    double m_fAveragePrice;
    std::int64_t m_nVolume;
};
```

需要实现交易时段横轴、午休时间压缩/隐藏、实时点追加、自动滚动、成交量、十字光标和区间缩放。

K 线和分时图共用 `CUICurve`，只替换数据系列，统一主题、坐标轴、十字光标和交互。

## 4. 表格

将当前 `QTableWidget` 改为 Qt Model/View：

```text
CWatchlistModel : QAbstractTableModel
CDepthModel     : QAbstractTableModel
CStrategyModel  : QAbstractTableModel
        ↓
QTableView
        ↓
CMarketItemDelegate
```

规则：

- 不为单元格创建 QWidget
- 用 Delegate 绘制涨跌颜色、状态点、图标和操作按钮
- 只对变化单元格发出 `dataChanged`
- 高频行情合并批量刷新
- 固定行高并启用 uniform row heights
- 排序放在代理模型或后台线程
- 大数据量使用分页或 `fetchMore`

## 5. 数据服务拆分

`CSession` 负责连接、认证、token、心跳、自动重连、通用请求发送和响应分发，不持有行情订阅或查询等具体业务状态。`CHQService` 负责行情业务协议、订阅恢复、历史查询、数据转换、缓存和事件分发。

图表和表格不得直接调用 HQMarket，统一通过 `CHQService` 和模型获取数据。历史行情、五档盘口和指标计算不再拆成独立 Service，全部作为 `CHQService` 的内部能力：

```text
CHQService
 ├── 实时行情订阅、快照缓存和事件分发
 ├── K 线/分时历史请求与缓存
 ├── 五档盘口订阅与缓存
 └── 指标计算
```

当前已有 `CHQService`，负责订阅实时行情、接收最新价/昨收/成交量/延迟状态，并投递到 UI 线程。

## 6. 性能策略

- 网络请求、历史数据解析和指标计算放后台线程
- UI 线程只接收批量结果
- 实时行情按 20~50ms 合并刷新
- 根据屏幕像素宽度进行降采样
- 只绘制可视区域
- 历史数据使用有界缓存或环形缓冲
- 表格使用模型数据，不使用大量 QWidget
- K 线、分时和指标共享坐标范围，减少重复计算

## 7. 分阶段落地

### 第一阶段：基础功能

- 引入 QWT
- 新建 `CUICurve`
- 实现分时图、日/周/月 K 线和成交量
- 将自选列表、五档行情改为 `QTableView`
- 保持并扩展现有 `CHQService`

### 第二阶段：行情完整化

- 在 `CHQService` 中增加历史行情能力
- 接入历史 K 线和分时接口
- 在 `CHQService` 中增加五档盘口能力
- 在 `CHQService` 中增加指标计算和买卖点

### 第三阶段：性能优化

- 后台线程解析和计算
- 增量更新与批量信号
- 屏幕像素级降采样
- benchmark 测试不同数据量和刷新频率

### 第四阶段：极限性能

仅当 QWT 实测无法满足目标时，才切换为 `QOpenGLWidget/QRhi` 自定义批量渲染：

```text
QOpenGLWidget / QRhi
 ├── Candle VBO
 ├── Line VBO
 ├── Volume VBO
 ├── Indicator VBO
 └── GPU picking / crosshair
```

## 8. 备选方案

- KDChart：支持 Qt 6、Stock Charts，MIT 许可证，适合商业许可优先场景。<https://github.com/KDAB/KDChart>
- Qt Charts：提供官方 `QCandlestickSeries`，但复杂行情交互和大数据量刷新不作为首选。<https://doc.qt.io/qt-6/qcandlestickseries-qtcharts.html>
- QCustomPlot：功能丰富，但 GPL 许可证需谨慎评估商业发布风险。

## 9. 结论

XTrader 采用：

```text
K 线图：QWT QwtPlotTradingCurve
分时图：QWT QwtPlotCurve + Volume
行情表格：QTableView + QAbstractTableModel
数据层：CHQService（统一承载实时、历史、盘口和指标）
极限性能：后续按 benchmark 决定 QOpenGLWidget/QRhi
```
