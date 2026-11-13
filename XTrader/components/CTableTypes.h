#ifndef XTRADER_COMPONENTS_CTABLETYPES_H
#define XTRADER_COMPONENTS_CTABLETYPES_H

#include "../basic/CDatable.h"
#include <QBrush>
#include <QIcon>
#include <QHeaderView>
#include <QMetaType>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <map>

using _TyTableColumnId = std::uint64_t;
inline constexpr _TyTableColumnId TableCheckColumn = 0x100000000ULL;
inline constexpr _TyTableColumnId TableSequenceColumn = TableCheckColumn + 1;
inline constexpr _TyTableColumnId TableActionColumn = TableCheckColumn + 2;

enum class TableOperationKind
{
	Selection,
	Click,
	Activate,
	ChecksChanged,
	Edit,
	Action,
	ResultsChanged,
	SearchChanged
};

// Owned UI notification. IDs identify rows and columns, never display positions.
struct CTableOperation
{
	TableOperationKind m_kind{ TableOperationKind::Selection };
	std::uint64_t m_generation{ 0 };
	_TyDataVersion m_version{ 0 };
	_TyDataRowId m_rowId{ 0 };
	_TyTableColumnId m_columnId{ 0 };
	QVector<qulonglong> m_rowIds;
	QString m_actionId;
	std::uint64_t m_requestId{ 0 };
	QVariant m_originalValue;
	QVariant m_value;
};
Q_DECLARE_METATYPE(CTableOperation)

struct CTableAction
{
	QString m_id;
	QString m_text;
	bool m_enabled{ true };
	bool m_primary{ false };
	QIcon m_icon;
};
Q_DECLARE_METATYPE(CTableAction)
using _TyTableActions = QVector<CTableAction>;
Q_DECLARE_METATYPE(_TyTableActions)

enum TableRole
{
	TableRowIdRole = Qt::UserRole + 1,
	TableColumnIdRole,
	TableRawValueRole,
	TableActionsRole,
	TableChoicesRole,
	TableGenerationRole,
	TableVersionRole
};

struct CTableColumn
{
	_TyTableColumnId m_id{ 0 };
	QString m_title;
	Qt::Alignment m_alignment{ Qt::AlignLeft | Qt::AlignVCenter };
	int m_precision{ -1 };
	bool m_signed{ false };
	QString m_suffix;
	bool m_editable{ false };
	QStringList m_choices;
	QBrush m_foreground;
	std::map<QString, QString> m_texts;
};

struct CTableCell
{
	QVariant m_display;
	QString m_tooltip;
	QBrush m_foreground;
	_TyTableActions m_actions;
	bool m_checkable{ true };
	bool m_enabled{ true };
};

struct CTableHeaderColumn
{
	QString m_title;
	std::int64_t m_position{ 0 };
	bool m_visible{ true };
	QHeaderView::ResizeMode m_widthMode{ QHeaderView::Stretch };
	int m_width{ 0 };
};

struct CTablePresentation
{
	std::vector<CTableColumn> m_columns;
	std::map<std::pair<_TyDataRowId, _TyTableColumnId>, CTableCell> m_cells;
};

#endif
