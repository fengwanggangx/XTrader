#ifndef XTRADER_COMPONENTS_CTABLEDATA_H
#define XTRADER_COMPONENTS_CTABLEDATA_H

#include "CUITable.h"
#include <stdexcept>

struct CTableRow
{
	_TyDataRowId m_id{ 0 };
	std::vector<_TyDataValue> m_values;
};

inline void DefineTable(CDataTable& table, const std::vector<CDataColumnSchema>& columns)
{
	for (const auto& column : columns)
	{
		if (!table.AddColumn(column))
		{
			throw std::invalid_argument("Invalid table schema");
		}
	}
}

inline void PublishTable(CDataTable& table, CUITable& widget, const std::vector<CTableRow>& rows)
{
	CDataTableView before = table.GetView();
	bool structure = rows.size() != before.GetRowCount();
	if (!structure)
	{
		for (std::size_t row = 0; row < rows.size(); ++row)
		{
			structure = structure || rows[row].m_id != before.GetRowIds()[row];
		}
	}
	CDataTableWriter writer = table.BeginWrite();
	if (structure)
	{
		for (_TyDataRowId id : before.GetRowIds())
		{
			writer.DeleteRow(id);
		}
	}
	for (const auto& row : rows)
	{
		if (structure)
		{
			if (!writer.AddRow(row.m_id, row.m_values))
			{
				throw std::invalid_argument("Invalid table row");
			}
		}
		else
		{
			if (row.m_values.size() != before.GetColumnCount())
			{
				throw std::invalid_argument("Invalid table row width");
			}
			for (std::size_t column = 0; column < row.m_values.size(); ++column)
			{
				_TyDataValue oldValue;
				_TyDataColumnId id = before.GetSchema()[column].m_id;
				if (!before.GetValueById(row.m_id, id, oldValue) || oldValue != row.m_values[column])
				{
					if (!writer.SetValue(row.m_id, id, row.m_values[column]))
					{
						throw std::invalid_argument("Invalid table value");
					}
				}
			}
		}
	}
	std::pair<CDataTableView, CDataChangeSet> update = writer.Commit();
	widget.DataModel()->SetView(std::move(update.first), update.second);
}

#endif
