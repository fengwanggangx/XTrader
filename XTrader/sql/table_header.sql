CREATE TABLE IF NOT EXISTS table_header
(
	c_table TEXT NOT NULL,
	c_name TEXT NOT NULL,
	c_caption TEXT NOT NULL,
	c_colid INTEGER NOT NULL CHECK (typeof(c_colid) = 'integer' AND c_colid > 0),
	c_pos INTEGER NOT NULL CHECK (typeof(c_pos) = 'integer' AND c_pos >= 0),
	c_show INTEGER NOT NULL DEFAULT 1 CHECK (c_show IN (0, 1)),
	c_width_mode TEXT NOT NULL CHECK (c_width_mode IN ('fixed', 'stretch', 'contents')),
	c_width INTEGER CHECK (
		(c_width_mode = 'fixed' AND typeof(c_width) = 'integer' AND c_width BETWEEN 1 AND 2147483647)
		OR (c_width_mode <> 'fixed' AND c_width IS NULL)
	),
	PRIMARY KEY (c_table, c_name),
	UNIQUE (c_table, c_colid)
);

INSERT OR IGNORE INTO table_header (c_table, c_name, c_caption, c_colid, c_pos, c_show, c_width_mode, c_width)
VALUES
	('trade_accounts', 'check', '', 4294967296, 0, 1, 'fixed', 44),
	('trade_accounts', 'sequence', '序号', 4294967297, 1, 1, 'fixed', 60),
	('trade_accounts', 'broker', '券商', 1, 2, 1, 'stretch', NULL),
	('trade_accounts', 'site', '站点', 2, 3, 1, 'stretch', NULL),
	('trade_accounts', 'account', '账号', 3, 4, 1, 'stretch', NULL),
	('trade_accounts', 'account_name', '账户名称', 4, 5, 1, 'stretch', NULL),
	('trade_accounts', 'status', '状态', 5, 6, 1, 'stretch', NULL),
	('trade_accounts', 'action', '操作', 4294967298, 7, 1, 'fixed', 112);

INSERT OR IGNORE INTO table_header (c_table, c_name, c_caption, c_colid, c_pos, c_show, c_width_mode, c_width)
VALUES
	('trade_account_strategies', 'strategy', '关联策略', 1, 0, 1, 'stretch', NULL),
	('trade_account_strategies', 'strategy_type', '策略类型', 2, 1, 1, 'stretch', NULL);

INSERT OR IGNORE INTO table_header (c_table, c_name, c_caption, c_colid, c_pos, c_show, c_width_mode, c_width)
VALUES
	('market_ranking', 'rank', '排名', 1, 0, 1, 'stretch', NULL),
	('market_ranking', 'sector', '行业板块', 2, 1, 1, 'stretch', NULL),
	('market_ranking', 'percent', '涨跌幅', 3, 2, 1, 'stretch', NULL);

INSERT OR IGNORE INTO table_header (c_table, c_name, c_caption, c_colid, c_pos, c_show, c_width_mode, c_width)
VALUES
	('strategy_instances', 'instance', '实例名称', 1, 0, 1, 'contents', NULL),
	('strategy_instances', 'template', '策略模板', 2, 1, 1, 'contents', NULL),
	('strategy_instances', 'account', '交易账户', 3, 2, 1, 'contents', NULL),
	('strategy_instances', 'symbol', '交易标的', 4, 3, 1, 'contents', NULL),
	('strategy_instances', 'mode', '模式', 5, 4, 0, 'contents', NULL),
	('strategy_instances', 'status', '状态', 6, 5, 1, 'stretch', NULL),
	('strategy_instances', 'position', '总持仓', 7, 6, 0, 'contents', NULL),
	('strategy_instances', 'profit', '累计盈亏', 8, 7, 0, 'contents', NULL),
	('strategy_instances', 'runtime', '运行时间', 9, 8, 0, 'contents', NULL);

INSERT OR IGNORE INTO table_header (c_table, c_name, c_caption, c_colid, c_pos, c_show, c_width_mode, c_width)
VALUES
	('strategy_parameters', 'parameter', '参数', 1, 0, 1, 'contents', NULL),
	('strategy_parameters', 'value', '当前值', 2, 1, 1, 'contents', NULL),
	('strategy_parameters', 'description', '说明', 3, 2, 1, 'stretch', NULL);

INSERT OR IGNORE INTO table_header (c_table, c_name, c_caption, c_colid, c_pos, c_show, c_width_mode, c_width)
VALUES
	('strategy_logs', 'time', '时间', 1, 0, 1, 'contents', NULL),
	('strategy_logs', 'level', '级别', 2, 1, 1, 'contents', NULL),
	('strategy_logs', 'message', '内容', 3, 2, 1, 'stretch', NULL);

INSERT OR IGNORE INTO table_header (c_table, c_name, c_caption, c_colid, c_pos, c_show, c_width_mode, c_width)
VALUES
	('strategy_edit_parameters', 'name', '参数名', 1, 0, 1, 'fixed', 100),
	('strategy_edit_parameters', 'value', '参数值', 2, 1, 1, 'stretch', NULL);

INSERT OR IGNORE INTO table_header (c_table, c_name, c_caption, c_colid, c_pos, c_show, c_width_mode, c_width)
VALUES
	('strategy_edit_subscriptions', 'symbol', '证券代码', 1, 0, 1, 'fixed', 100),
	('strategy_edit_subscriptions', 'exchange', '交易所', 2, 1, 1, 'fixed', 100),
	('strategy_edit_subscriptions', 'channel', '行情通道', 3, 2, 1, 'stretch', NULL);

INSERT OR IGNORE INTO table_header (c_table, c_name, c_caption, c_colid, c_pos, c_show, c_width_mode, c_width)
VALUES
	('keyboard_results', 'symbol', '代码', 1, 0, 1, 'fixed', 110),
	('keyboard_results', 'name', '名称', 2, 1, 1, 'stretch', NULL),
	('keyboard_results', 'market', '市场', 3, 2, 1, 'fixed', 76);

WITH tables(c_table) AS
(
	VALUES ('market_a_share'), ('market_watchlist'), ('market_constituents'), ('market_index'), ('market_hong_kong')
),
columns(c_colid, c_name, c_caption) AS
(
	VALUES
		(1, 'security', '代码'),
		(2, 'name', '名称'),
		(3, 'last_price', '最新价'),
		(4, 'change', '涨跌额'),
		(5, 'percent', '涨跌幅'),
		(6, 'pre_close', '昨收'),
		(7, 'volume', '成交量'),
		(8, 'status', '状态'),
		(9, 'sequence', '序列'),
		(10, 'listing_status', '证券状态'),
		(11, 'market', '市场'),
		(12, 'pinyin_full', '拼音全称'),
		(13, 'pinyin_short', '拼音简称')
)
INSERT OR IGNORE INTO table_header (c_table, c_name, c_caption, c_colid, c_pos, c_show, c_width_mode, c_width)
SELECT c_table, c_name, c_caption, c_colid, c_colid - 1,
	CASE
		WHEN c_table = 'market_constituents' THEN c_colid IN (1, 2, 3, 4, 5, 7)
		ELSE c_colid IN (1, 2, 3, 5)
	END,
	'stretch', NULL
FROM tables CROSS JOIN columns;
