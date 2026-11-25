#include "CDatable.h"
#include "TMessagePump.h"

#include <cassert>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace
{
	constexpr _TyDataColumnId SecurityColumn = 1;
	constexpr _TyDataColumnId PriceColumn = 2;
	constexpr _TyDataColumnId VolumeColumn = 3;

	void TestViewIsolation()
	{
		CDataTable table;
		assert(table.AddColumn(CDataColumnSchema{ SecurityColumn, "security", DataType::String }));
		assert(table.AddColumn(CDataColumnSchema{ PriceColumn, "price", DataType::Double }));
		assert(table.AddColumn(CDataColumnSchema{ VolumeColumn, "volume", DataType::Int64 }));
		CDataTableWriter insert = table.BeginWrite();
		assert(insert.AddRow(1, { std::string("600519.SSE"), 1500.0, std::int64_t(100) }));
		CDataTableView before = insert.Commit().first;
		CDataTableWriter update = table.BeginWrite();
		assert(update.SetValue(1, PriceColumn, 1501.0));
		CDataTableView after = update.Commit().first;
		_TyDataValue value;
		assert(before.GetValueById(_TyDataRowId(1), PriceColumn, value));
		assert(1500.0 == std::get<double>(value));
		assert(after.GetValueById(_TyDataRowId(1), PriceColumn, value));
		assert(1501.0 == std::get<double>(value));
		assert(before.GetVersion() < after.GetVersion());
	}

	void TestStableRowId()
	{
		CDataTable table;
		assert(table.AddColumn(CDataColumnSchema{ SecurityColumn, "security", DataType::String }));
		CDataTableWriter insert = table.BeginWrite();
		assert(insert.AddRow(10, { std::string("000001.SZSE") }));
		assert(insert.AddRow(20, { std::string("600000.SSE") }));
		assert(insert.AddRow(30, { std::string("920001.BSE") }));
		auto [insertView, insertChanges] = insert.Commit();
		assert(3 == insertView.GetRowCount());
		assert(3 == insertChanges.m_insertedRows.size());
		CDataTableWriter remove = table.BeginWrite();
		assert(remove.DeleteRow(20));
		auto [view, changes] = remove.Commit();
		assert((std::vector<_TyDataRowId>{ 20 }) == changes.m_deletedRows);
		_TyDataRowIndex row = 0;
		assert(view.FindRow(10, row));
		assert(view.FindRow(30, row));
		assert(!view.FindRow(20, row));
	}

	void TestHighCardinalityStrings()
	{
		CDataTable table;
		assert(table.AddColumn(CDataColumnSchema{ SecurityColumn, "security", DataType::String }));
		CDataTableWriter insert = table.BeginWrite();
		insert.ReserveRows(10000);
		for (_TyDataRowId rowId = 1; 10000 >= rowId; ++rowId)
		{
			assert(insert.AddRow(rowId, { std::string("security-") + std::to_string(rowId) }));
		}
		CDataTableView before = insert.Commit().first;
		CDataTableWriter update = table.BeginWrite();
		for (_TyDataRowId rowId = 1; 10000 >= rowId; rowId += 2)
		{
			assert(update.SetValue(rowId, SecurityColumn, std::string("updated-") + std::to_string(rowId)));
		}
		CDataTableView after = update.Commit().first;
		_TyDataValue value;
		assert(before.GetValueById(9999, SecurityColumn, value));
		assert("security-9999" == std::get<std::string>(value));
		assert(after.GetValueById(9999, SecurityColumn, value));
		assert("updated-9999" == std::get<std::string>(value));
	}

	void TestConcurrentSnapshots()
	{
		CDataTable table;
		assert(table.AddColumn(CDataColumnSchema{ PriceColumn, "price", DataType::Double }));
		CDataTableWriter insert = table.BeginWrite();
		insert.ReserveRows(1000);
		for (_TyDataRowId rowId = 1; 1000 >= rowId; ++rowId)
		{
			assert(insert.AddRow(rowId, { 0.0 }));
		}
		insert.Commit();

		std::atomic_bool stopping{ false };
		std::atomic_uint64_t readCount{ 0 };
		std::vector<std::thread> readers;
		readers.reserve(4);
		for (int nThread = 0; 4 > nThread; ++nThread)
		{
			readers.emplace_back([&table, &stopping, &readCount]()
			{
				_TyDataVersion lastVersion = 0;
				while (!stopping.load())
				{
					CDataTableView view = table.GetView();
					assert(lastVersion <= view.GetVersion());
					lastVersion = view.GetVersion();
					_TyDataValue value;
					assert(view.GetValueById(500, PriceColumn, value));
					assert(std::holds_alternative<double>(value));
					++readCount;
				}
			});
		}
		for (int nBatch = 1; 100 >= nBatch; ++nBatch)
		{
			CDataTableWriter update = table.BeginWrite();
			for (_TyDataRowId rowId = 1; 1000 >= rowId; ++rowId)
			{
				assert(update.SetValue(rowId, PriceColumn, static_cast<double>(nBatch)));
			}
			update.Commit();
		}
		stopping.store(true);
		for (std::thread& reader : readers)
		{
			reader.join();
		}
		assert(0 < readCount.load());
	}

	void TestMessagePump()
	{
		TMessagePump<int> dispatcher;
		int nFirstTotal = 0;
		int nSecondTotal = 0;
		_TyCallbackId firstId = dispatcher.Subscribe([&nFirstTotal](const int& value)
		{
			nFirstTotal += value;
		});
		dispatcher.Subscribe([&nSecondTotal](const int& value)
		{
			nSecondTotal += value;
		});
		dispatcher.Notify(3);
		dispatcher.Unsubscribe(firstId);
		dispatcher.Notify(5);
		assert(3 == nFirstTotal);
		assert(8 == nSecondTotal);
	}

	void RunBenchmark()
	{
		CDataTable table;
		assert(table.AddColumn(CDataColumnSchema{ SecurityColumn, "security", DataType::String }));
		assert(table.AddColumn(CDataColumnSchema{ PriceColumn, "price", DataType::Double }));
		assert(table.AddColumn(CDataColumnSchema{ VolumeColumn, "volume", DataType::Int64 }));
		CDataTableWriter insert = table.BeginWrite();
		insert.ReserveRows(5000);
		for (_TyDataRowId rowId = 1; 5000 >= rowId; ++rowId)
		{
			assert(insert.AddRow(rowId, { std::to_string(rowId), 10.0, std::int64_t(0) }));
		}
		insert.Commit();
		std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
		for (int batch = 0; 20 > batch; ++batch)
		{
			CDataTableWriter update = table.BeginWrite();
			for (_TyDataRowId rowId = 1; 5000 >= rowId; ++rowId)
			{
				assert(update.SetValue(rowId, PriceColumn, 10.0 + static_cast<double>(batch)));
			}
			update.Commit();
		}
		std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin);
		std::cout << "CDataTable 100000 updates: " << elapsed.count() << " ms\n";
	}
} // namespace

int main()
{
	TestViewIsolation();
	TestStableRowId();
	TestHighCardinalityStrings();
	TestConcurrentSnapshots();
	TestMessagePump();
	RunBenchmark();
	return 0;
}
