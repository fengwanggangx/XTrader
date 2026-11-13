#include "CDataTableModel.h"
#include "../database/IDataBase.h"
#include <algorithm>
#include <type_traits>
#include <limits>

namespace
{
	std::map<QString, std::map<_TyTableColumnId, CTableHeaderColumn>> headerTables;
}

bool CDataTableModel::LoadHeaderConfig(db::IDataBase& db)
{
	headerTables.clear();
	const db::CQueryTable& table = db.ExecQuery("SELECT c_table, c_caption, c_colid, c_pos, c_show, c_width_mode, c_width FROM table_header");
	if (7 != table.m_columns.size())
	{
		return false;
	}
	for (const auto& row : table.m_rows)
	{
		if (7 != row.size())
		{
			continue;
		}
		const std::string* name = std::get_if<std::string>(&row[0]);
		const std::string* title = std::get_if<std::string>(&row[1]);
		const std::int64_t* id = std::get_if<std::int64_t>(&row[2]);
		const std::int64_t* position = std::get_if<std::int64_t>(&row[3]);
		const std::int64_t* visible = std::get_if<std::int64_t>(&row[4]);
		const std::string* mode = std::get_if<std::string>(&row[5]);
		const std::int64_t* width = std::get_if<std::int64_t>(&row[6]);
		if (nullptr == name || nullptr == title || nullptr == id || nullptr == position || nullptr == visible || nullptr == mode)
		{
			continue;
		}
		CTableHeaderColumn config{ QString::fromStdString(*title), *position, 0 != *visible };
		if ("fixed" == *mode)
		{
			if (nullptr == width || 0 >= *width || std::numeric_limits<int>::max() < *width)
			{
				continue;
			}
			config.m_widthMode = QHeaderView::Fixed;
			config.m_width = static_cast<int>(*width);
		}
		else if ("contents" == *mode)
		{
			config.m_widthMode = QHeaderView::ResizeToContents;
		}
		else if ("stretch" != *mode)
		{
			continue;
		}
		headerTables[QString::fromStdString(*name)].emplace(static_cast<_TyTableColumnId>(*id), std::move(config));
	}
	return true;
}

void CDataTableModel::SetHeaderTable(const QString& table)
{
	if (table == m_headerTable)
	{
		return;
	}
	m_headerTable = table;
	if (0 < columnCount())
	{
		emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
	}
}

const CTableHeaderColumn* CDataTableModel::HeaderColumn(_TyTableColumnId id) const
{
	const auto tableIter = headerTables.find(m_headerTable);
	if (headerTables.end() == tableIter)
	{
		return nullptr;
	}
	const auto columnIter = tableIter->second.find(id);
	return tableIter->second.end() == columnIter ? nullptr : &columnIter->second;
}

CDataTableModel::CDataTableModel(QObject* parent) : QAbstractTableModel(parent)
{
	qRegisterMetaType<CTableOperation>();
}

int CDataTableModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(m_view.GetRowCount());
}

int CDataTableModel::columnCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(m_presentation.m_columns.empty() ? m_view.GetColumnCount() : m_presentation.m_columns.size());
}

const CTableCell* CDataTableModel::FindCell(_TyDataRowId row, _TyTableColumnId column) const
{
	const auto mIter = m_presentation.m_cells.find({ row, column });
	return m_presentation.m_cells.end() == mIter ? nullptr : &mIter->second;
}

QVariant CDataTableModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || 0 > index.row() || 0 > index.column() || rowCount() <= index.row() || columnCount() <= index.column())
	{
		return {};
	}
	_TyDataRowId rowId = GetRowId(index.row());
	CTableColumn defaultColumn;
	if (m_presentation.m_columns.empty())
	{
		defaultColumn.m_id = m_view.GetSchema()[index.column()].m_id;
	}
	const CTableColumn& column = m_presentation.m_columns.empty() ? defaultColumn : m_presentation.m_columns[index.column()];
	const CTableCell* cell = FindCell(rowId, column.m_id);
	switch (role)
	{
	case TableRowIdRole:
		return QVariant::fromValue<qulonglong>(rowId);
	case TableColumnIdRole:
		return QVariant::fromValue<qulonglong>(column.m_id);
	case TableGenerationRole:
		return QVariant::fromValue<qulonglong>(m_view.GetSourceId());
	case TableVersionRole:
		return QVariant::fromValue<qulonglong>(m_view.GetVersion());
	case TableActionsRole:
		return QVariant::fromValue(nullptr == cell ? _TyTableActions() : cell->m_actions);
	case TableChoicesRole:
		return column.m_choices;
	case Qt::TextAlignmentRole:
		return static_cast<int>(column.m_alignment);
	case Qt::ForegroundRole:
		if (nullptr != cell && Qt::NoBrush != cell->m_foreground.style())
		{
			return cell->m_foreground;
		}
		return Qt::NoBrush == column.m_foreground.style() ? QVariant() : QVariant(column.m_foreground);
	case Qt::CheckStateRole:
		return TableCheckColumn == column.m_id ? QVariant(m_checked.contains(rowId) ? Qt::Checked : Qt::Unchecked) : QVariant();
	default:
		break;
	}
	if (Qt::DisplayRole != role && Qt::EditRole != role && Qt::ToolTipRole != role && TableRawValueRole != role)
	{
		return {};
	}
	if (Qt::ToolTipRole == role && nullptr != cell && !cell->m_tooltip.isEmpty())
	{
		return cell->m_tooltip;
	}
	if (TableSequenceColumn == column.m_id)
	{
		return index.row() + 1;
	}
	_TyDataValue value;
	if (TableCheckColumn <= column.m_id || !m_view.GetValue(index.row(), static_cast<_TyDataColumnId>(column.m_id), value))
	{
		return {};
	}
	QVariant raw = ToVariant(value);
	if (TableRawValueRole == role || Qt::EditRole == role)
	{
		return raw;
	}
	if (nullptr != cell && cell->m_display.isValid())
	{
		return cell->m_display;
	}
	const auto textIter = column.m_texts.find(raw.toString());
	if (column.m_texts.end() != textIter)
	{
		return textIter->second;
	}
	if (0 <= column.m_precision)
	{
		double number = raw.toDouble();
		return (column.m_signed && 0.0 < number ? "+" : "") + QString::number(number, 'f', column.m_precision) + column.m_suffix;
	}
	return raw;
}

QVariant CDataTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (Qt::Horizontal != orientation || 0 > section || columnCount() <= section)
	{
		return {};
	}
	_TyTableColumnId id = m_presentation.m_columns.empty() ? m_view.GetSchema()[section].m_id : m_presentation.m_columns[section].m_id;
	if (TableColumnIdRole == role)
	{
		return QVariant::fromValue<qulonglong>(id);
	}
	if (Qt::DisplayRole != role)
	{
		return {};
	}
	const CTableHeaderColumn* config = HeaderColumn(id);
	if (nullptr != config)
	{
		return config->m_title;
	}
	return m_presentation.m_columns.empty() ? QString::fromStdString(m_view.GetSchema()[section].m_strName) : m_presentation.m_columns[section].m_title;
}

Qt::ItemFlags CDataTableModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return Qt::NoItemFlags;
	}
	Qt::ItemFlags result = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	_TyTableColumnId column = data(index, TableColumnIdRole).toULongLong();
	const CTableCell* cell = FindCell(GetRowId(index.row()), column);
	if (nullptr != cell && !cell->m_enabled)
	{
		return Qt::ItemIsSelectable;
	}
	if (TableCheckColumn == column && (nullptr == cell || cell->m_checkable))
	{
		result |= Qt::ItemIsUserCheckable;
	}
	if (!m_presentation.m_columns.empty() && m_presentation.m_columns[index.column()].m_editable)
	{
		result |= Qt::ItemIsEditable;
	}
	return result;
}

bool CDataTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (0 == (flags(index) & Qt::ItemIsEnabled))
	{
		return false;
	}
	if (Qt::CheckStateRole == role && 0 != (flags(index) & Qt::ItemIsUserCheckable))
	{
		QSet<qulonglong> checked = m_checked;
		if (Qt::Checked == value.toInt())
		{
			checked.insert(GetRowId(index.row()));
		}
		else
		{
			checked.remove(GetRowId(index.row()));
		}
		SetCheckedRows(checked);
		return true;
	}
	if (Qt::EditRole != role || 0 == (flags(index) & Qt::ItemIsEditable))
	{
		return false;
	}
	CTableOperation operation = MakeOperation(TableOperationKind::Edit, index);
	operation.m_requestId = m_nextRequestId++;
	operation.m_originalValue = data(index, TableRawValueRole);
	operation.m_value = value;
	emit OperationRequested(operation);
	return true;
}

_TyDataRowId CDataTableModel::GetRowId(int row) const
{
	return 0 > row || rowCount() <= row ? 0 : m_view.GetRowIds()[row];
}

const CDataTableView& CDataTableModel::GetView() const noexcept
{
	return m_view;
}

const CTablePresentation& CDataTableModel::GetPresentation() const noexcept
{
	return m_presentation;
}

const QSet<qulonglong>& CDataTableModel::CheckedRows() const
{
	return m_checked;
}

void CDataTableModel::NotifyData()
{
	if (0 < rowCount() && 0 < columnCount())
	{
		emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
	}
}

void CDataTableModel::SetView(CDataTableView view, const CDataChangeSet& changes)
{
	bool sourceChanged = m_view.GetSourceId() != view.GetSourceId();
	if (!sourceChanged && view.GetVersion() < m_view.GetVersion())
	{
		return;
	}
	bool structure = sourceChanged || changes.m_bStructureChanged || m_view.GetRowIds() != view.GetRowIds() || m_view.GetSchema().size() != view.GetSchema().size();
	if (!structure)
	{
		for (std::size_t column = 0; column < view.GetSchema().size(); ++column)
		{
			const CDataColumnSchema& before = m_view.GetSchema()[column];
			const CDataColumnSchema& after = view.GetSchema()[column];
			structure = structure || before.m_id != after.m_id || before.m_type != after.m_type || before.m_strName != after.m_strName;
		}
	}
	_TyDataVersion previous = m_view.GetVersion();
	if (structure)
	{
		beginResetModel();
	}
	m_view = std::move(view);
	QSet<qulonglong> retained;
	if (!sourceChanged)
	{
		for (qulonglong id : m_checked)
		{
			_TyDataRowIndex row = 0;
			if (m_view.FindRow(id, row))
			{
				retained.insert(id);
			}
		}
	}
	bool checksChanged = retained != m_checked;
	m_checked = std::move(retained);
	if (sourceChanged)
	{
		m_presentation.m_cells.clear();
	}
	else if (structure)
	{
		std::erase_if(m_presentation.m_cells, [this](const auto& entry)
		{
			_TyDataRowIndex row = 0;
			return !m_view.FindRow(entry.first.first, row);
		});
	}
	if (structure)
	{
		endResetModel();
	}
	else if (previous + 1 != changes.m_version)
	{
		NotifyData();
	}
	else
	{
		std::map<_TyTableColumnId, int> columns = ColumnIndexes();
		int minRow = (std::numeric_limits<int>::max)();
		int maxRow = -1;
		int minColumn = (std::numeric_limits<int>::max)();
		int maxColumn = -1;
		for (const auto& change : changes.m_changedCells)
		{
			_TyDataRowIndex row = 0;
			const auto column = columns.find(change.m_columnId);
			if (!m_view.FindRow(change.m_rowId, row) || columns.end() == column)
			{
				continue;
			}
			minRow = (std::min)(minRow, static_cast<int>(row));
			maxRow = (std::max)(maxRow, static_cast<int>(row));
			minColumn = (std::min)(minColumn, column->second);
			maxColumn = (std::max)(maxColumn, column->second);
		}
		if (0 <= maxRow)
		{
			emit dataChanged(index(minRow, minColumn), index(maxRow, maxColumn));
		}
	}
	if (checksChanged)
	{
		NotifyChecks();
	}
}

void CDataTableModel::SetPresentation(CTablePresentation presentation)
{
	bool structure = m_presentation.m_columns.size() != presentation.m_columns.size();
	if (!structure)
	{
		for (std::size_t column = 0; column < presentation.m_columns.size(); ++column)
		{
			structure = structure || m_presentation.m_columns[column].m_id != presentation.m_columns[column].m_id;
		}
	}
	if (structure)
	{
		beginResetModel();
	}
	m_presentation = std::move(presentation);
	if (structure)
	{
		endResetModel();
	}
	else
	{
		if (0 < columnCount())
		{
			emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
		}
		NotifyData();
	}
}

std::map<_TyTableColumnId, int> CDataTableModel::ColumnIndexes() const
{
	std::map<_TyTableColumnId, int> columns;
	for (int column = 0; column < columnCount(); ++column)
	{
		_TyTableColumnId id = m_presentation.m_columns.empty() ? m_view.GetSchema()[column].m_id : m_presentation.m_columns[column].m_id;
		columns.emplace(id, column);
	}
	return columns;
}

void CDataTableModel::SetCells(std::map<std::pair<_TyDataRowId, _TyTableColumnId>, CTableCell> cells)
{
	std::map<_TyTableColumnId, int> columns = ColumnIndexes();
	int minRow = (std::numeric_limits<int>::max)();
	int maxRow = -1;
	int minColumn = (std::numeric_limits<int>::max)();
	int maxColumn = -1;
	for (auto& [key, value] : cells)
	{
		_TyDataRowIndex row = 0;
		const auto column = columns.find(key.second);
		if (!m_view.FindRow(key.first, row) || columns.end() == column)
		{
			continue;
		}
		m_presentation.m_cells.insert_or_assign(key, std::move(value));
		minRow = (std::min)(minRow, static_cast<int>(row));
		maxRow = (std::max)(maxRow, static_cast<int>(row));
		minColumn = (std::min)(minColumn, column->second);
		maxColumn = (std::max)(maxColumn, column->second);
	}
	if (0 <= maxRow)
	{
		emit dataChanged(index(minRow, minColumn), index(maxRow, maxColumn), { Qt::DisplayRole, Qt::ToolTipRole, Qt::ForegroundRole, TableActionsRole });
	}
}

CTableOperation CDataTableModel::MakeOperation(TableOperationKind kind, const QModelIndex& index) const
{
	CTableOperation operation;
	operation.m_kind = kind;
	operation.m_generation = m_view.GetSourceId();
	operation.m_version = m_view.GetVersion();
	if (index.isValid())
	{
		operation.m_rowId = data(index, TableRowIdRole).toULongLong();
		operation.m_columnId = data(index, TableColumnIdRole).toULongLong();
	}
	return operation;
}

void CDataTableModel::NotifyChecks()
{
	CTableOperation operation = MakeOperation(TableOperationKind::ChecksChanged);
	for (qulonglong id : m_checked)
	{
		operation.m_rowIds.append(id);
	}
	std::sort(operation.m_rowIds.begin(), operation.m_rowIds.end());
	emit OperationRequested(operation);
}

void CDataTableModel::SetCheckedRows(const QSet<qulonglong>& rows, bool notify)
{
	QSet<qulonglong> checked;
	for (_TyDataRowId id : m_view.GetRowIds())
	{
		if (rows.contains(id))
		{
			checked.insert(id);
		}
	}
	if (checked == m_checked)
	{
		return;
	}
	m_checked = std::move(checked);
	if (0 < rowCount() && 0 < columnCount())
	{
		emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), { Qt::CheckStateRole });
	}
	if (notify)
	{
		NotifyChecks();
	}
}

QVariant CDataTableModel::ToVariant(const _TyDataValue& value) const
{
	return std::visit([](const auto& item) -> QVariant
	{
		using _TyValue = std::decay_t<decltype(item)>;
		if constexpr (std::is_same_v<_TyValue, std::string>)
		{
			return QString::fromStdString(item);
		}
		else
		{
			return QVariant::fromValue(item);
		}
	}, value);
}
