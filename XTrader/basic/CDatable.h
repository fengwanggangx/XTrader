#ifndef XTRADER_BASIC_CDATATABLE_H
#define XTRADER_BASIC_CDATATABLE_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

using _TyDataRowId = std::uint64_t;
using _TyDataColumnId = std::uint32_t;
using _TyDataVersion = std::uint64_t;
using _TyDataRowIndex = std::size_t;

enum class DataType
{
	Int64,
	UInt64,
	Double,
	Bool,
	String,
	Timestamp
};

using _TyDataValue = std::variant<std::int64_t, std::uint64_t, double, bool, std::string>;
using _TyDataColumnStorage = std::variant<std::vector<std::int64_t>, std::vector<std::uint64_t>, std::vector<double>, std::vector<bool>, std::vector<std::uint32_t>>;

struct CDataColumnSchema
{
	_TyDataColumnId m_id{ 0 };
	std::string m_strName;
	DataType m_type{ DataType::String };
};

struct CDataCellChange
{
	_TyDataRowId m_rowId{ 0 };
	_TyDataColumnId m_columnId{ 0 };
	bool operator<(const CDataCellChange& arg) const noexcept;
	bool operator==(const CDataCellChange& arg) const noexcept;
};

struct CDataChangeSet
{
	_TyDataVersion m_version{ 0 };
	bool m_bStructureChanged{ false };
	std::vector<_TyDataRowId> m_insertedRows;
	std::vector<_TyDataRowId> m_deletedRows;
	std::vector<CDataCellChange> m_changedCells;
};

struct CDataTableStorage
{
	std::uint64_t m_sourceId{ 0 };
	_TyDataVersion m_version{ 0 };
	std::vector<CDataColumnSchema> m_schema;
	std::vector<std::shared_ptr<_TyDataColumnStorage>> m_columns;
	std::unordered_map<_TyDataColumnId, std::size_t> m_columnIndexes;
	std::unordered_map<std::string, _TyDataColumnId> m_columnNames;

	struct CRowStorage
	{
		std::vector<_TyDataRowId> m_rowIds;
		std::unordered_map<_TyDataRowId, _TyDataRowIndex> m_rowIndexes;
	};

	struct CStringStorage
	{
		std::vector<std::string> m_strings{ std::string() };
		std::unordered_map<std::string, std::uint32_t> m_stringIndexes{ { std::string(), 0 } };
	};

	std::shared_ptr<CRowStorage> m_rows{ std::make_shared<CRowStorage>() };
	std::shared_ptr<CStringStorage> m_stringStorage{ std::make_shared<CStringStorage>() };
};

class CDataTableView final
{
	public:
	CDataTableView() = default;
	bool IsValid() const noexcept;
	_TyDataVersion GetVersion() const noexcept;
	std::uint64_t GetSourceId() const noexcept;
	std::size_t GetRowCount() const noexcept;
	std::size_t GetColumnCount() const noexcept;
	const std::vector<CDataColumnSchema>& GetSchema() const noexcept;
	const std::vector<_TyDataRowId>& GetRowIds() const noexcept;
	bool FindRow(_TyDataRowId rowId, _TyDataRowIndex& result) const;
	bool GetValue(_TyDataRowIndex row, _TyDataColumnId columnId, _TyDataValue& result) const;
	bool GetValueById(_TyDataRowId rowId, _TyDataColumnId columnId, _TyDataValue& result) const;

	private:
	explicit CDataTableView(std::shared_ptr<const CDataTableStorage> storage);
	std::shared_ptr<const CDataTableStorage> m_storage;
	friend class CDataTable;
	friend class CDataTableWriter;
};

class CDataTable;

class CDataTableWriter final
{
	public:
	CDataTableWriter(CDataTableWriter&& arg) noexcept;
	CDataTableWriter& operator=(CDataTableWriter&& arg) noexcept;
	~CDataTableWriter();
	CDataTableWriter(const CDataTableWriter&) = delete;
	CDataTableWriter& operator=(const CDataTableWriter&) = delete;

	bool ReserveRows(std::size_t count);
	bool AddRow(_TyDataRowId rowId, const std::vector<_TyDataValue>& values);
	bool DeleteRow(_TyDataRowId rowId);
	bool SetValue(_TyDataRowId rowId, _TyDataColumnId columnId, const _TyDataValue& value);
	std::pair<CDataTableView, CDataChangeSet> Commit();
	void Cancel() noexcept;

	private:
	CDataTableWriter(CDataTable& table, std::unique_lock<std::mutex>&& writerLock, std::shared_ptr<CDataTableStorage>&& storage);
	void EnsureRowsWritable();
	void EnsureColumnWritable(std::size_t column);
	void EnsureStringsWritable();
	bool StoreValue(_TyDataRowIndex row, std::size_t column, const _TyDataValue& value, bool bAppend);
	std::uint32_t InternString(const std::string& value);

	CDataTable* m_pTable{ nullptr };
	std::unique_lock<std::mutex> m_writerLock;
	std::shared_ptr<CDataTableStorage> m_storage;
	CDataChangeSet m_changes;
	bool m_bFinished{ false };
	friend class CDataTable;
};

class CDataTable final
{
	public:
	CDataTable();
	CDataTable(const CDataTable&) = delete;
	CDataTable& operator=(const CDataTable&) = delete;
	bool AddColumn(const CDataColumnSchema& schema);
	CDataTableWriter BeginWrite();
	CDataTableView GetView() const;
	bool FindRow(_TyDataRowId rowId, _TyDataRowIndex& result) const;

	private:
	void Publish(std::shared_ptr<CDataTableStorage>&& storage);
	mutable std::mutex m_mtx_writer;
	std::atomic<std::shared_ptr<const CDataTableStorage>> m_currentStorage;
	friend class CDataTableWriter;
};

#endif
