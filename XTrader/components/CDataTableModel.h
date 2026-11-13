#ifndef XTRADER_COMPONENTS_CDATATABLEMODEL_H
#define XTRADER_COMPONENTS_CDATATABLEMODEL_H

#include "CTableTypes.h"
#include <QAbstractTableModel>
#include <QSet>

namespace db
{
	class IDataBase;
}

class CDataTableModel final : public QAbstractTableModel
{
	Q_OBJECT
public:
	explicit CDataTableModel(QObject* pParent = nullptr);
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	// Success means an edit request was emitted; only the page can publish accepted data.
	bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
	void SetHeaderTable(const QString& table);
	static bool LoadHeaderConfig(db::IDataBase& db);
	const CTableHeaderColumn* HeaderColumn(_TyTableColumnId id) const;
	_TyDataRowId GetRowId(int row) const;
	const CDataTableView& GetView() const noexcept;
	const CTablePresentation& GetPresentation() const noexcept;
	void SetView(CDataTableView view, const CDataChangeSet& changes);
	void SetPresentation(CTablePresentation presentation);
	void SetCells(std::map<std::pair<_TyDataRowId, _TyTableColumnId>, CTableCell> cells);
	void SetCheckedRows(const QSet<qulonglong>& rows, bool notify = true);
	const QSet<qulonglong>& CheckedRows() const;
	CTableOperation MakeOperation(TableOperationKind kind, const QModelIndex& index = QModelIndex()) const;

signals:
	void OperationRequested(CTableOperation operation);

private:
	QVariant ToVariant(const _TyDataValue& value) const;
	const CTableCell* FindCell(_TyDataRowId row, _TyTableColumnId column) const;
	void NotifyChecks();
	std::map<_TyTableColumnId, int> ColumnIndexes() const;
	void NotifyData();
	QString m_headerTable;
	CDataTableView m_view;
	CTablePresentation m_presentation;
	QSet<qulonglong> m_checked;
	std::uint64_t m_nextRequestId{ 1 };
};

#endif
