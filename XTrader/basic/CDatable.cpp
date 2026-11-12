#include "CDatable.h"

#include <algorithm>
#include <utility>

namespace
{
	_TyDataColumnStorage MakeColumn(DataType t)
	{
		switch (t)
		{
		case DataType::Int64:
		case DataType::Timestamp: return std::vector<std::int64_t>();
		case DataType::UInt64: return std::vector<std::uint64_t>();
		case DataType::Double: return std::vector<double>();
		case DataType::Bool: return std::vector<bool>();
		case DataType::String: return std::vector<std::uint32_t>();
		}
		return std::vector<std::uint32_t>();
	}

	void ReserveColumn(_TyDataColumnStorage& column, std::size_t count)
	{
		std::visit([count](auto& values) { values.reserve(count); }, column);
	}

	void RemoveRow(_TyDataColumnStorage& column, _TyDataRowIndex row)
	{
		std::visit([row](auto& values)
		{
			if (row + 1 < values.size())
			{
				values[row] = std::move(values.back());
			}
			values.pop_back();
		}, column);
	}

	template <typename ValueType>
	bool StoreColumnValue(_TyDataColumnStorage& column, _TyDataRowIndex row, const _TyDataValue& value, bool bAppend)
	{
		if (!std::holds_alternative<ValueType>(value))
		{
			return false;
		}
		std::vector<ValueType>& values = std::get<std::vector<ValueType>>(column);
		if (bAppend)
		{
			values.emplace_back(std::get<ValueType>(value));
		}
		else
		{
			values[row] = std::get<ValueType>(value);
		}
		return true;
	}
}

bool CDataCellChange::operator<(const CDataCellChange& arg) const noexcept
{
	return (m_rowId < arg.m_rowId) || ((m_rowId == arg.m_rowId) && (m_columnId < arg.m_columnId));
}

bool CDataCellChange::operator==(const CDataCellChange& arg) const noexcept
{
	return (m_rowId == arg.m_rowId) && (m_columnId == arg.m_columnId);
}

CDataTableView::CDataTableView(std::shared_ptr<const CDataTableStorage> storage) : m_storage(std::move(storage))
{
}

std::uint64_t CDataTableView::GetSourceId() const noexcept
{
	return nullptr == m_storage ? 0 : m_storage->m_sourceId;
}

bool CDataTableView::IsValid() const noexcept
{
	return nullptr != m_storage;
}

_TyDataVersion CDataTableView::GetVersion() const noexcept
{
	return nullptr == m_storage ? 0 : m_storage->m_version;
}

std::size_t CDataTableView::GetRowCount() const noexcept
{
	return nullptr == m_storage ? 0 : m_storage->m_rows->m_rowIds.size();
}

std::size_t CDataTableView::GetColumnCount() const noexcept
{
	return nullptr == m_storage ? 0 : m_storage->m_schema.size();
}

const std::vector<CDataColumnSchema>& CDataTableView::GetSchema() const noexcept
{
	static const std::vector<CDataColumnSchema> empty;
	return nullptr == m_storage ? empty : m_storage->m_schema;
}

const std::vector<_TyDataRowId>& CDataTableView::GetRowIds() const noexcept
{
	static const std::vector<_TyDataRowId> empty;
	return nullptr == m_storage ? empty : m_storage->m_rows->m_rowIds;
}

bool CDataTableView::FindRow(_TyDataRowId rowId, _TyDataRowIndex& result) const
{
	if (nullptr == m_storage)
	{
		return false;
	}
	const auto mIter = m_storage->m_rows->m_rowIndexes.find(rowId);
	if (m_storage->m_rows->m_rowIndexes.end() == mIter)
	{
		return false;
	}
	result = mIter->second;
	return true;
}

bool CDataTableView::GetValue(_TyDataRowIndex row, _TyDataColumnId columnId, _TyDataValue& result) const
{
	if ((nullptr == m_storage) || (m_storage->m_rows->m_rowIds.size() <= row))
	{
		return false;
	}
	const auto mIter = m_storage->m_columnIndexes.find(columnId);
	if (m_storage->m_columnIndexes.end() == mIter)
	{
		return false;
	}
	std::size_t columnIndex = mIter->second;
	const _TyDataColumnStorage& column = *m_storage->m_columns[columnIndex];
	switch (m_storage->m_schema[columnIndex].m_type)
	{
	case DataType::Int64:
	case DataType::Timestamp: result = std::get<std::vector<std::int64_t>>(column)[row]; return true;
	case DataType::UInt64: result = std::get<std::vector<std::uint64_t>>(column)[row]; return true;
	case DataType::Double: result = std::get<std::vector<double>>(column)[row]; return true;
	case DataType::Bool: result = std::get<std::vector<bool>>(column)[row]; return true;
	case DataType::String:
	{
		std::uint32_t index = std::get<std::vector<std::uint32_t>>(column)[row];
		if (m_storage->m_stringStorage->m_strings.size() <= index)
		{
			return false;
		}
		result = m_storage->m_stringStorage->m_strings[index];
		return true;
	}
	}
	return false;
}

bool CDataTableView::GetValueById(_TyDataRowId rowId, _TyDataColumnId columnId, _TyDataValue& result) const
{
	_TyDataRowIndex row = 0;
	return FindRow(rowId, row) && GetValue(row, columnId, result);
}

CDataTableWriter::CDataTableWriter(CDataTable& table, std::unique_lock<std::mutex>&& writerLock, std::shared_ptr<CDataTableStorage>&& storage) : m_pTable(&table), m_writerLock(std::move(writerLock)), m_storage(std::move(storage))
{
}

CDataTableWriter::CDataTableWriter(CDataTableWriter&& arg) noexcept = default;
CDataTableWriter& CDataTableWriter::operator=(CDataTableWriter&& arg) noexcept = default;
CDataTableWriter::~CDataTableWriter()
{
	Cancel();
}

bool CDataTableWriter::ReserveRows(std::size_t count)
{
	if (m_bFinished || (nullptr == m_storage))
	{
		return false;
	}
	EnsureRowsWritable();
	m_storage->m_rows->m_rowIds.reserve(count);
	m_storage->m_rows->m_rowIndexes.reserve(count);
	for (std::size_t columnIndex = 0; columnIndex < m_storage->m_columns.size(); ++columnIndex)
	{
		EnsureColumnWritable(columnIndex);
		ReserveColumn(*m_storage->m_columns[columnIndex], count);
	}
	return true;
}

bool CDataTableWriter::AddRow(_TyDataRowId rowId, const std::vector<_TyDataValue>& values)
{
	if (m_bFinished || (nullptr == m_storage) || (0 == rowId) || (values.size() != m_storage->m_columns.size()) || m_storage->m_rows->m_rowIndexes.contains(rowId))
	{
		return false;
	}
	for (std::size_t column = 0; column < values.size(); ++column)
	{
		if (!StoreValue(0, column, values[column], true))
		{
			for (std::size_t rollback = 0; rollback < column; ++rollback)
			{
				std::visit([](auto& data) { data.pop_back(); }, *m_storage->m_columns[rollback]);
			}
			return false;
		}
	}
	EnsureRowsWritable();
	_TyDataRowIndex row = m_storage->m_rows->m_rowIds.size();
	m_storage->m_rows->m_rowIds.emplace_back(rowId);
	m_storage->m_rows->m_rowIndexes.emplace(rowId, row);
	m_changes.m_bStructureChanged = true;
	m_changes.m_insertedRows.emplace_back(rowId);
	return true;
}

bool CDataTableWriter::DeleteRow(_TyDataRowId rowId)
{
	if (m_bFinished || (nullptr == m_storage))
	{
		return false;
	}
	const auto mIter = m_storage->m_rows->m_rowIndexes.find(rowId);
	if (m_storage->m_rows->m_rowIndexes.end() == mIter)
	{
		return false;
	}
	EnsureRowsWritable();
	const auto writableIter = m_storage->m_rows->m_rowIndexes.find(rowId);
	_TyDataRowIndex row = writableIter->second;
	_TyDataRowIndex last = m_storage->m_rows->m_rowIds.size() - 1;
	_TyDataRowId movedId = m_storage->m_rows->m_rowIds[last];
	for (std::size_t columnIndex = 0; columnIndex < m_storage->m_columns.size(); ++columnIndex)
	{
		EnsureColumnWritable(columnIndex);
		RemoveRow(*m_storage->m_columns[columnIndex], row);
	}
	if (row != last)
	{
		m_storage->m_rows->m_rowIds[row] = movedId;
		m_storage->m_rows->m_rowIndexes[movedId] = row;
	}
	m_storage->m_rows->m_rowIds.pop_back();
	m_storage->m_rows->m_rowIndexes.erase(writableIter);
	m_changes.m_bStructureChanged = true;
	m_changes.m_deletedRows.emplace_back(rowId);
	return true;
}

bool CDataTableWriter::SetValue(_TyDataRowId rowId, _TyDataColumnId columnId, const _TyDataValue& value)
{
	if (m_bFinished || (nullptr == m_storage))
	{
		return false;
	}
	const auto rowIter = m_storage->m_rows->m_rowIndexes.find(rowId);
	const auto columnIter = m_storage->m_columnIndexes.find(columnId);
	if ((m_storage->m_rows->m_rowIndexes.end() == rowIter) || (m_storage->m_columnIndexes.end() == columnIter) || !StoreValue(rowIter->second, columnIter->second, value, false))
	{
		return false;
	}
	m_changes.m_changedCells.emplace_back(CDataCellChange{ rowId, columnId });
	return true;
}

std::pair<CDataTableView, CDataChangeSet> CDataTableWriter::Commit()
{
	if (m_bFinished || (nullptr == m_pTable) || (nullptr == m_storage))
	{
		return { CDataTableView(), CDataChangeSet() };
	}
	std::sort(m_changes.m_changedCells.begin(), m_changes.m_changedCells.end());
	m_changes.m_changedCells.erase(std::unique(m_changes.m_changedCells.begin(), m_changes.m_changedCells.end()), m_changes.m_changedCells.end());
	++m_storage->m_version;
	m_changes.m_version = m_storage->m_version;
	CDataTableView view{ std::shared_ptr<const CDataTableStorage>(m_storage) };
	m_pTable->Publish(std::move(m_storage));
	m_bFinished = true;
	m_pTable = nullptr;
	m_writerLock.unlock();
	return { std::move(view), std::move(m_changes) };
}

void CDataTableWriter::Cancel() noexcept
{
	if (!m_bFinished)
	{
		m_bFinished = true;
		m_storage.reset();
		m_pTable = nullptr;
		if (m_writerLock.owns_lock())
		{
			m_writerLock.unlock();
		}
	}
}

void CDataTableWriter::EnsureRowsWritable()
{
	if (1 != m_storage->m_rows.use_count())
	{
		m_storage->m_rows = std::make_shared<CDataTableStorage::CRowStorage>(*m_storage->m_rows);
	}
}

void CDataTableWriter::EnsureColumnWritable(std::size_t column)
{
	if (1 != m_storage->m_columns[column].use_count())
	{
		m_storage->m_columns[column] = std::make_shared<_TyDataColumnStorage>(*m_storage->m_columns[column]);
	}
}

void CDataTableWriter::EnsureStringsWritable()
{
	if (1 != m_storage->m_stringStorage.use_count())
	{
		m_storage->m_stringStorage = std::make_shared<CDataTableStorage::CStringStorage>(*m_storage->m_stringStorage);
	}
}

bool CDataTableWriter::StoreValue(_TyDataRowIndex row, std::size_t columnIndex, const _TyDataValue& value, bool bAppend)
{
	DataType t = m_storage->m_schema[columnIndex].m_type;
	EnsureColumnWritable(columnIndex);
	_TyDataColumnStorage& column = *m_storage->m_columns[columnIndex];
	if ((DataType::Int64 == t) || (DataType::Timestamp == t))
	{
		return StoreColumnValue<std::int64_t>(column, row, value, bAppend);
	}
	if (DataType::UInt64 == t)
	{
		return StoreColumnValue<std::uint64_t>(column, row, value, bAppend);
	}
	if (DataType::Double == t)
	{
		return StoreColumnValue<double>(column, row, value, bAppend);
	}
	if (DataType::Bool == t)
	{
		return StoreColumnValue<bool>(column, row, value, bAppend);
	}
	if ((DataType::String == t) && std::holds_alternative<std::string>(value))
	{
		std::vector<std::uint32_t>& values = std::get<std::vector<std::uint32_t>>(column);
		std::uint32_t index = InternString(std::get<std::string>(value));
		if (bAppend)
		{
			values.emplace_back(index);
		}
		else
		{
			values[row] = index;
		}
		return true;
	}
	return false;
}

std::uint32_t CDataTableWriter::InternString(const std::string& value)
{
	const auto mIter = m_storage->m_stringStorage->m_stringIndexes.find(value);
	if (m_storage->m_stringStorage->m_stringIndexes.end() != mIter)
	{
		return mIter->second;
	}
	EnsureStringsWritable();
	std::uint32_t index = static_cast<std::uint32_t>(m_storage->m_stringStorage->m_strings.size());
	m_storage->m_stringStorage->m_strings.emplace_back(value);
	m_storage->m_stringStorage->m_stringIndexes.emplace(m_storage->m_stringStorage->m_strings.back(), index);
	return index;
}

CDataTable::CDataTable()
{
	static std::atomic_uint64_t nextSourceId{ 1 };
	std::shared_ptr<CDataTableStorage> storage = std::make_shared<CDataTableStorage>();
	storage->m_sourceId = nextSourceId.fetch_add(1);
	m_currentStorage.store(std::move(storage));
}

bool CDataTable::AddColumn(const CDataColumnSchema& schema)
{
	if ((0 == schema.m_id) || schema.m_strName.empty())
	{
		return false;
	}
	std::lock_guard<std::mutex> writerLock(m_mtx_writer);
	std::shared_ptr<const CDataTableStorage> current = m_currentStorage.load();
	if (current->m_columnIndexes.contains(schema.m_id) || current->m_columnNames.contains(schema.m_strName))
	{
		return false;
	}
	std::shared_ptr<CDataTableStorage> storage = std::make_shared<CDataTableStorage>(*current);
	_TyDataColumnStorage column = MakeColumn(schema.m_type);
	ReserveColumn(column, storage->m_rows->m_rowIds.size());
	for (std::size_t row = 0; row < storage->m_rows->m_rowIds.size(); ++row)
	{
		std::visit([](auto& values) { values.emplace_back(); }, column);
	}
	std::size_t index = storage->m_schema.size();
	storage->m_schema.emplace_back(schema);
	storage->m_columns.emplace_back(std::make_shared<_TyDataColumnStorage>(std::move(column)));
	storage->m_columnIndexes.emplace(schema.m_id, index);
	storage->m_columnNames.emplace(schema.m_strName, schema.m_id);
	++storage->m_version;
	Publish(std::move(storage));
	return true;
}

CDataTableWriter CDataTable::BeginWrite()
{
	std::unique_lock<std::mutex> writerLock(m_mtx_writer);
	return CDataTableWriter(*this, std::move(writerLock), std::make_shared<CDataTableStorage>(*m_currentStorage.load()));
}

CDataTableView CDataTable::GetView() const
{
	return CDataTableView(m_currentStorage.load());
}

bool CDataTable::FindRow(_TyDataRowId rowId, _TyDataRowIndex& result) const
{
	return GetView().FindRow(rowId, result);
}

void CDataTable::Publish(std::shared_ptr<CDataTableStorage>&& storage)
{
	m_currentStorage.store(std::shared_ptr<const CDataTableStorage>(std::move(storage)));
}
