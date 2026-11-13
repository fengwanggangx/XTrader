#ifndef XTRADER_COMPONENTS_CUITABLE_H
#define XTRADER_COMPONENTS_CUITABLE_H

#include "CDataTableModel.h"
#include <QTableView>
class CTableSearchProxyModel;
class QCheckBox;
class QLabel;

class CUITable final : public QTableView
{
	Q_OBJECT
public:
	explicit CUITable(QWidget* parent = nullptr);
	~CUITable() override;
	void setModel(QAbstractItemModel* model) override;
	QAbstractItemModel* SourceModel() const;
	CDataTableModel* DataModel() const;
	void SetHeaderTable(const QString& table, QList<_TyTableColumnId> defaultHidden = {});
	void SetSearchColumns(const QList<int>& columns);
	void Search(const QString& text);
	void Update();
	int ResultCount() const;
	_TyDataRowId SelectedRowId() const;
	bool SelectRowId(_TyDataRowId rowId);
	QVector<qulonglong> CheckedRowIds() const;
	void SetCheckedRowIds(const QVector<qulonglong>& rows);
	void SetEmptyText(const QString& text);
	void TriggerAction(const QModelIndex& index, const QString& actionId);

signals:
	void OperationRequested(CTableOperation operation);

protected:
	void resizeEvent(QResizeEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void leaveEvent(QEvent* event) override;

private:
	void NotifyResults();
	void RefreshHeader();
	void ApplyHeaderLayout();
	void RestoreSelection();
	void SendOperation(TableOperationKind kind, const QModelIndex& index);
	QString m_headerTable;
	QList<_TyTableColumnId> m_defaultHiddenColumns;
	bool m_defaultStretchLast{ false };
	CTableSearchProxyModel* m_searchProxy{ nullptr };
	CDataTableModel* m_defaultModel{ nullptr };
	QCheckBox* m_checkAll{ nullptr };
	QLabel* m_empty{ nullptr };
	QMetaObject::Connection m_operationConnection;
	_TyDataRowId m_selectedRowId{ 0 };
	std::uint64_t m_generation{ 0 };
	bool m_restoring{ false };
};

#endif
