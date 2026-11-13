#include "CUITable.h"
#include <QAbstractProxyModel>
#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QCursor>
#include <QApplication>
#include <QMenu>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QSignalBlocker>
#include <QScrollBar>
#include <algorithm>
#include <tuple>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>

class CTableSearchProxyModel final : public QSortFilterProxyModel
{
public:
	explicit CTableSearchProxyModel(QObject* parent) : QSortFilterProxyModel(parent)
	{
		setSortRole(TableRawValueRole);
		setDynamicSortFilter(true);
	}
	QString m_search;
	QList<int> m_columns;
	void Refresh()
	{
		beginFilterChange();
		endFilterChange();
	}

protected:
	bool filterAcceptsRow(int row, const QModelIndex& parent) const override
	{
		if (m_search.isEmpty())
		{
			return true;
		}
		for (int column = 0; column < sourceModel()->columnCount(); ++column)
		{
			if ((m_columns.isEmpty() || m_columns.contains(column)) && sourceModel()->index(row, column, parent).data().toString().contains(m_search, Qt::CaseInsensitive))
			{
				return true;
			}
		}
		return false;
	}
};

namespace
{
	class CTableDelegate final : public QStyledItemDelegate
	{
	public:
		explicit CTableDelegate(CUITable* table) : QStyledItemDelegate(table), m_table(table)
		{
		}
		void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
		{
			QStyledItemDelegate::initStyleOption(option, index);
			QVariant foreground = index.data(Qt::ForegroundRole);
			if (foreground.isValid())
			{
				option->palette.setBrush(QPalette::HighlightedText, foreground.value<QBrush>());
			}
			if (TableSequenceColumn == index.data(TableColumnIdRole).toULongLong())
			{
				option->text = QString::number(index.row() + 1);
			}
		}
		void ResetPress()
		{
			m_pressedIndex = QPersistentModelIndex();
			m_pressedAction.clear();
		}
		static QRect ActionRect(const QRect& rect, int count, int item)
		{
			int width = (rect.width() - 16 - (count - 1) * 6) / count;
			return QRect(rect.left() + 8 + item * (width + 6), rect.top() + 7, width, rect.height() - 14);
		}
		void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
		{
			_TyTableActions actions = index.data(TableActionsRole).value<_TyTableActions>();
			QStyledItemDelegate::paint(painter, option, index);
			for (int item = 0; item < actions.size(); ++item)
			{
				const CTableAction& action = actions[item];
				bool enabled = action.m_enabled && 0 != (index.flags() & Qt::ItemIsEnabled);
				QRect rect = ActionRect(option.rect, actions.size(), item).adjusted(0, 0, -1, -1);
				QPalette colors = m_table->palette();
				QColor accent = colors.color(QPalette::Link);
				QColor background = action.m_primary && enabled ? accent : colors.color(QPalette::Button);
				QColor foreground = enabled ? (action.m_primary ? QColor(Qt::white) : colors.color(QPalette::ButtonText)) : colors.color(QPalette::Disabled, QPalette::ButtonText);
				bool hovered = enabled && 0 != (option.state & QStyle::State_MouseOver) && rect.contains(m_table->viewport()->mapFromGlobal(QCursor::pos()));
				if (hovered)
				{
					background = action.m_primary ? background.darker(110) : background.lighter(110);
				}
				if (hovered && m_pressedIndex == index && m_pressedAction == action.m_id)
				{
					background = background.darker(110);
				}
				painter->save();
				painter->setRenderHint(QPainter::Antialiasing);
				painter->setPen(action.m_primary && enabled ? accent : colors.color(QPalette::Mid));
				painter->setBrush(background);
				painter->drawRoundedRect(rect, 4, 4);
				QRect textRect = rect.adjusted(6, 0, -6, 0);
				if (!action.m_icon.isNull())
				{
					action.m_icon.paint(painter, QRect(textRect.left(), rect.center().y() - 8, 16, 16), Qt::AlignCenter, enabled ? QIcon::Normal : QIcon::Disabled);
					textRect.adjust(20, 0, 0, 0);
				}
				painter->setPen(foreground);
				painter->setFont(option.font);
				painter->drawText(textRect, Qt::AlignCenter, option.fontMetrics.elidedText(action.m_text, Qt::ElideRight, textRect.width()));
				painter->restore();
			}
		}
		bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override
		{
			_TyTableActions actions = index.data(TableActionsRole).value<_TyTableActions>();
			if (!actions.isEmpty())
			{
				if (QEvent::MouseButtonPress == event->type() || QEvent::MouseButtonRelease == event->type())
				{
					QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
					if (Qt::LeftButton != mouse->button())
					{
						return false;
					}
					for (int item = 0; item < actions.size(); ++item)
					{
						if (!actions[item].m_enabled || !ActionRect(option.rect, actions.size(), item).contains(mouse->position().toPoint()))
						{
							continue;
						}
						if (QEvent::MouseButtonPress == event->type())
						{
							m_pressedIndex = index;
							m_pressedAction = actions[item].m_id;
							m_table->viewport()->update(option.rect);
							return true;
						}
						bool matched = m_pressedIndex == index && m_pressedAction == actions[item].m_id;
						m_pressedIndex = QPersistentModelIndex();
						m_pressedAction.clear();
						if (matched)
						{
							m_table->TriggerAction(index, actions[item].m_id);
							return true;
						}
					}
					if (QEvent::MouseButtonRelease == event->type())
					{
						m_pressedIndex = QPersistentModelIndex();
						m_pressedAction.clear();
					}
				}
				return false;
			}
			return QStyledItemDelegate::editorEvent(event, model, option, index);
		}
		QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
		{
			QStringList choices = index.data(TableChoicesRole).toStringList();
			if (!choices.isEmpty())
			{
				QComboBox* editor = new QComboBox(parent);
				editor->addItems(choices);
				QString current = index.data(Qt::EditRole).toString();
				if (!choices.contains(current))
				{
					editor->addItem(current);
				}
				return editor;
			}
			return QStyledItemDelegate::createEditor(parent, option, index);
		}
		void setEditorData(QWidget* editor, const QModelIndex& index) const override
		{
			QComboBox* combo = qobject_cast<QComboBox*>(editor);
			if (nullptr != combo)
			{
				combo->setCurrentText(index.data(Qt::EditRole).toString());
			}
			else
			{
				QStyledItemDelegate::setEditorData(editor, index);
			}
		}
		void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
		{
			QComboBox* combo = qobject_cast<QComboBox*>(editor);
			if (nullptr != combo)
			{
				model->setData(index, combo->currentText(), Qt::EditRole);
			}
			else
			{
				QStyledItemDelegate::setModelData(editor, model, index);
			}
		}

	private:
		CUITable* m_table;
		QPersistentModelIndex m_pressedIndex;
		QString m_pressedAction;
	};
} // namespace

CUITable::CUITable(QWidget* parent) : QTableView(parent)
{
	m_searchProxy = new CTableSearchProxyModel(this);
	m_defaultModel = new CDataTableModel(this);
	QTableView::setModel(m_searchProxy);
	setItemDelegate(new CTableDelegate(this));
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setAlternatingRowColors(true);
	setMouseTracking(true);
	setShowGrid(false);
	verticalHeader()->hide();
	verticalHeader()->setDefaultSectionSize(36);
	m_checkAll = new QCheckBox(horizontalHeader()->viewport());
	m_checkAll->setToolTip("全选当前结果");
	m_checkAll->hide();
	m_empty = new QLabel("暂无数据", viewport());
	m_empty->setAlignment(Qt::AlignCenter);
	m_empty->setAttribute(Qt::WA_TransparentForMouseEvents);
	connect(m_checkAll, &QCheckBox::clicked, this, [this](bool checked)
	{
		CDataTableModel* dataModel = DataModel();
		if (nullptr == dataModel)
		{
			return;
		}
		QSet<qulonglong> rows = dataModel->CheckedRows();
		for (int row = 0; row < m_searchProxy->rowCount(); ++row)
		{
			if (isRowHidden(row))
			{
				continue;
			}
			for (int column = 0; column < m_searchProxy->columnCount(); ++column)
			{
				QModelIndex cell = m_searchProxy->index(row, column);
				if (TableCheckColumn == cell.data(TableColumnIdRole).toULongLong() && 0 != (cell.flags() & Qt::ItemIsUserCheckable))
				{
					qulonglong id = cell.data(TableRowIdRole).toULongLong();
					if (checked)
					{
						rows.insert(id);
					}
					else
					{
						rows.remove(id);
					}
				}
			}
		}
		dataModel->SetCheckedRows(rows);
	});
	connect(horizontalHeader(), &QHeaderView::geometriesChanged, this, &CUITable::RefreshHeader);
	connect(horizontalScrollBar(), &QAbstractSlider::valueChanged, this, &CUITable::RefreshHeader);
	connect(selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex& index)
	{
		if (!m_restoring)
		{
			m_selectedRowId = index.data(TableRowIdRole).toULongLong();
			SendOperation(TableOperationKind::Selection, index);
		}
	});
	connect(this, &QTableView::clicked, this, [this](const QModelIndex& index)
	{
		SendOperation(TableOperationKind::Click, index);
	});
	connect(this, &QTableView::activated, this, [this](const QModelIndex& index)
	{
		SendOperation(TableOperationKind::Activate, index);
	});
	connect(m_searchProxy, &QAbstractItemModel::modelAboutToBeReset, this, [this]()
	{
		m_restoring = true;
	});
	connect(m_searchProxy, &QAbstractItemModel::modelReset, this, [this]()
	{
		ApplyHeaderLayout();
		RestoreSelection();
	});
	connect(m_searchProxy, &QAbstractItemModel::layoutAboutToBeChanged, this, [this]()
	{
		m_restoring = true;
	});
	connect(m_searchProxy, &QAbstractItemModel::layoutChanged, this, &CUITable::RestoreSelection);
	connect(m_searchProxy, &QAbstractItemModel::rowsInserted, this, &CUITable::NotifyResults);
	connect(m_searchProxy, &QAbstractItemModel::rowsRemoved, this, &CUITable::RestoreSelection);
	connect(m_searchProxy, &QAbstractItemModel::dataChanged, this, [this]()
	{
		RefreshHeader();
		viewport()->update();
	});
	setModel(m_defaultModel);
}

CUITable::~CUITable()
{
	// QTableView tears down models and headers after the derived destructor.
	// Stop child signals before that teardown can invoke derived member slots.
	disconnect(m_operationConnection);
	disconnect(m_searchProxy, nullptr, this, nullptr);
	disconnect(selectionModel(), nullptr, this, nullptr);
	disconnect(horizontalHeader(), nullptr, this, nullptr);
	disconnect(horizontalScrollBar(), nullptr, this, nullptr);
	disconnect(m_checkAll, nullptr, this, nullptr);
	disconnect(this, nullptr, this, nullptr);
}

void CUITable::setModel(QAbstractItemModel* model)
{
	if (m_searchProxy == model)
	{
		return;
	}
	disconnect(m_operationConnection);
	m_restoring = true;
	m_searchProxy->setSourceModel(model);
	CDataTableModel* dataModel = DataModel();
	if (nullptr != dataModel)
	{
		dataModel->SetHeaderTable(m_headerTable);
		m_operationConnection = connect(dataModel, &CDataTableModel::OperationRequested, this, [this](CTableOperation operation)
		{
			RefreshHeader();
			emit OperationRequested(std::move(operation));
		});
	}
	ApplyHeaderLayout();
	RestoreSelection();
}

QAbstractItemModel* CUITable::SourceModel() const
{
	return m_searchProxy->sourceModel();
}

CDataTableModel* CUITable::DataModel() const
{
	QAbstractItemModel* model = m_searchProxy->sourceModel();
	while (nullptr != model)
	{
		CDataTableModel* dataModel = qobject_cast<CDataTableModel*>(model);
		if (nullptr != dataModel)
		{
			return dataModel;
		}
		QAbstractProxyModel* proxy = qobject_cast<QAbstractProxyModel*>(model);
		model = nullptr == proxy ? nullptr : proxy->sourceModel();
	}
	return nullptr;
}

void CUITable::SetHeaderTable(const QString& table, QList<_TyTableColumnId> defaultHidden)
{
	m_headerTable = table;
	m_defaultHiddenColumns = std::move(defaultHidden);
	m_defaultStretchLast = horizontalHeader()->stretchLastSection();
	CDataTableModel* dataModel = DataModel();
	if (nullptr != dataModel)
	{
		dataModel->SetHeaderTable(table);
	}
	ApplyHeaderLayout();
}

void CUITable::ApplyHeaderLayout()
{
	CDataTableModel* dataModel = DataModel();
	if (m_headerTable.isEmpty() || nullptr == dataModel)
	{
		return;
	}
	QHeaderView* header = horizontalHeader();
	using _TyOrderedColumn = std::tuple<std::int64_t, _TyTableColumnId, int>;
	std::vector<_TyOrderedColumn> configured;
	std::vector<int> unconfigured;
	int count = header->count();
	for (int logical = 0; count > logical; ++logical)
	{
		_TyTableColumnId id = dataModel->headerData(logical, Qt::Horizontal, TableColumnIdRole).toULongLong();
		const CTableHeaderColumn* config = dataModel->HeaderColumn(id);
		if (nullptr == config)
		{
			setColumnHidden(logical, m_defaultHiddenColumns.contains(id));
			unconfigured.push_back(logical);
			continue;
		}
		configured.emplace_back(config->m_position, id, logical);
	}
	if (configured.empty())
	{
		return;
	}

	std::sort(configured.begin(), configured.end());
	header->setStretchLastSection(false);
	int visual = 0;
	for (const auto& column : configured)
	{
		_TyTableColumnId id = std::get<1>(column);
		int logical = std::get<2>(column);
		header->moveSection(header->visualIndex(logical), visual++);
		const CTableHeaderColumn* config = dataModel->HeaderColumn(id);
		setColumnHidden(logical, !config->m_visible);
		header->setSectionResizeMode(logical, config->m_widthMode);
		if (QHeaderView::Fixed == config->m_widthMode)
		{
			header->resizeSection(logical, config->m_width);
		}
	}
	for (int logical : unconfigured)
	{
		header->moveSection(header->visualIndex(logical), visual++);
	}
	if (m_defaultStretchLast && !unconfigured.empty() && !isColumnHidden(unconfigured.back()))
	{
		header->setStretchLastSection(true);
	}
	RefreshHeader();
}

void CUITable::SetSearchColumns(const QList<int>& columns)
{
	m_searchProxy->m_columns = columns;
	Update();
}

void CUITable::Search(const QString& text)
{
	QString search = text.trimmed();
	if (search == m_searchProxy->m_search)
	{
		return;
	}
	m_searchProxy->m_search = search;
	Update();
	CTableOperation operation;
	operation.m_kind = TableOperationKind::SearchChanged;
	operation.m_generation = m_generation;
	if (nullptr != DataModel())
	{
		operation.m_version = DataModel()->GetView().GetVersion();
	}
	operation.m_value = search;
	emit OperationRequested(operation);
}

void CUITable::Update()
{
	m_searchProxy->Refresh();
	RestoreSelection();
}

int CUITable::ResultCount() const
{
	return m_searchProxy->rowCount();
}

_TyDataRowId CUITable::SelectedRowId() const
{
	return currentIndex().data(TableRowIdRole).toULongLong();
}

bool CUITable::SelectRowId(_TyDataRowId id)
{
	for (int row = 0; row < ResultCount(); ++row)
	{
		if (id == m_searchProxy->index(row, 0).data(TableRowIdRole).toULongLong())
		{
			setCurrentIndex(m_searchProxy->index(row, 0));
			selectRow(row);
			return true;
		}
	}
	selectionModel()->clear();
	return false;
}

void CUITable::RestoreSelection()
{
	CDataTableModel* dataModel = DataModel();
	std::uint64_t generation = nullptr == dataModel ? 0 : dataModel->GetView().GetSourceId();
	if (generation != m_generation)
	{
		m_selectedRowId = 0;
		m_generation = generation;
	}
	_TyDataRowId selected = m_selectedRowId;
	m_restoring = true;
	bool found = SelectRowId(selected);
	m_selectedRowId = found ? selected : 0;
	m_restoring = false;
	if (!found && 0 != selected)
	{
		SendOperation(TableOperationKind::Selection, QModelIndex());
	}
	NotifyResults();
}

QVector<qulonglong> CUITable::CheckedRowIds() const
{
	QVector<qulonglong> rows;
	CDataTableModel* model = DataModel();
	if (nullptr != model)
	{
		for (qulonglong row : model->CheckedRows())
		{
			rows.append(row);
		}
	}
	std::sort(rows.begin(), rows.end());
	return rows;
}

void CUITable::SetCheckedRowIds(const QVector<qulonglong>& rows)
{
	CDataTableModel* model = DataModel();
	if (nullptr != model)
	{
		model->SetCheckedRows(QSet<qulonglong>(rows.begin(), rows.end()));
	}
}

void CUITable::SendOperation(TableOperationKind kind, const QModelIndex& index)
{
	CTableOperation operation;
	operation.m_kind = kind;
	operation.m_generation = m_generation;
	if (nullptr != DataModel())
	{
		operation.m_version = DataModel()->GetView().GetVersion();
	}
	operation.m_rowId = index.data(TableRowIdRole).toULongLong();
	operation.m_columnId = index.data(TableColumnIdRole).toULongLong();
	emit OperationRequested(operation);
}

void CUITable::TriggerAction(const QModelIndex& index, const QString& actionId)
{
	if (0 == (index.flags() & Qt::ItemIsEnabled))
	{
		return;
	}
	_TyTableActions actions = index.data(TableActionsRole).value<_TyTableActions>();
	for (const auto& action : actions)
	{
		if (action.m_id == actionId && action.m_enabled)
		{
			CTableOperation operation;
			operation.m_kind = TableOperationKind::Action;
			operation.m_generation = index.data(TableGenerationRole).toULongLong();
			operation.m_version = index.data(TableVersionRole).toULongLong();
			operation.m_rowId = index.data(TableRowIdRole).toULongLong();
			operation.m_columnId = index.data(TableColumnIdRole).toULongLong();
			operation.m_actionId = actionId;
			emit OperationRequested(operation);
			return;
		}
	}
}

void CUITable::RefreshHeader()
{
	int checkColumn = -1;
	CDataTableModel* model = DataModel();
	if (nullptr != model)
	{
		const auto& columns = model->GetPresentation().m_columns;
		for (int column = 0; column < static_cast<int>(columns.size()); ++column)
		{
			if (TableCheckColumn == columns[column].m_id)
			{
				checkColumn = column;
				break;
			}
		}
	}
	m_checkAll->setVisible(0 <= checkColumn && !isColumnHidden(checkColumn) && !horizontalHeader()->isHidden());
	if (0 > checkColumn)
	{
		return;
	}
	int total = 0;
	int checked = 0;
	for (int row = 0; row < ResultCount(); ++row)
	{
		if (isRowHidden(row))
		{
			continue;
		}
		QModelIndex cell = m_searchProxy->index(row, checkColumn);
		if (0 != (cell.flags() & Qt::ItemIsUserCheckable))
		{
			++total;
			if (Qt::Checked == cell.data(Qt::CheckStateRole).toInt())
			{
				++checked;
			}
		}
	}
	QSignalBlocker blocker(m_checkAll);
	m_checkAll->setTristate(0 < checked && checked < total);
	m_checkAll->setCheckState(0 == checked ? Qt::Unchecked : checked == total ? Qt::Checked
																			  : Qt::PartiallyChecked);
	m_checkAll->setEnabled(0 < total);
	QSize size = m_checkAll->sizeHint();
	m_checkAll->setGeometry(horizontalHeader()->sectionViewportPosition(checkColumn) + (horizontalHeader()->sectionSize(checkColumn) - size.width()) / 2, (horizontalHeader()->height() - size.height()) / 2, size.width(), size.height());
}

void CUITable::NotifyResults()
{
	RefreshHeader();
	m_empty->setVisible(0 == ResultCount());
	m_empty->setGeometry(viewport()->rect());
	CTableOperation operation;
	operation.m_kind = TableOperationKind::ResultsChanged;
	operation.m_generation = m_generation;
	if (nullptr != DataModel())
	{
		operation.m_version = DataModel()->GetView().GetVersion();
	}
	operation.m_value = ResultCount();
	emit OperationRequested(operation);
}

void CUITable::SetEmptyText(const QString& text)
{
	m_empty->setText(text);
}

void CUITable::resizeEvent(QResizeEvent* event)
{
	QTableView::resizeEvent(event);
	m_empty->setGeometry(viewport()->rect());
	RefreshHeader();
}

void CUITable::mouseMoveEvent(QMouseEvent* event)
{
	QModelIndex index = indexAt(event->position().toPoint());
	_TyTableActions actions = index.data(TableActionsRole).value<_TyTableActions>();
	bool actionable = false;
	for (int item = 0; item < actions.size(); ++item)
	{
		if (actions[item].m_enabled && CTableDelegate::ActionRect(visualRect(index), actions.size(), item).contains(event->position().toPoint()))
		{
			actionable = true;
			break;
		}
	}
	viewport()->setCursor(actionable ? Qt::PointingHandCursor : Qt::ArrowCursor);
	QTableView::mouseMoveEvent(event);
}

void CUITable::mouseReleaseEvent(QMouseEvent* event)
{
	QPointer<CUITable> self(this);
	QTableView::mouseReleaseEvent(event);
	if (nullptr != self)
	{
		CTableDelegate* delegate = dynamic_cast<CTableDelegate*>(itemDelegate());
		if (nullptr != delegate)
		{
			delegate->ResetPress();
		}
		viewport()->update();
	}
}

void CUITable::leaveEvent(QEvent* event)
{
	viewport()->unsetCursor();
	QTableView::leaveEvent(event);
}

void CUITable::keyPressEvent(QKeyEvent* event)
{
	if (Qt::Key_Return == event->key() || Qt::Key_Enter == event->key() || Qt::Key_Space == event->key())
	{
		_TyTableActions actions = currentIndex().data(TableActionsRole).value<_TyTableActions>();
		if (1 < actions.size())
		{
			QPersistentModelIndex index(currentIndex());
			QMenu menu;
			for (const auto& action : actions)
			{
				QAction* item = menu.addAction(action.m_icon, action.m_text);
				item->setEnabled(action.m_enabled);
				item->setData(action.m_id);
			}
			QPointer<CUITable> self(this);
			QAction* chosen = menu.exec(viewport()->mapToGlobal(visualRect(index).bottomLeft()));
			if (nullptr != self && nullptr != chosen && index.isValid())
			{
				TriggerAction(index, chosen->data().toString());
			}
			event->accept();
			return;
		}
		for (const auto& action : actions)
		{
			if (action.m_enabled)
			{
				TriggerAction(currentIndex(), action.m_id);
				event->accept();
				return;
			}
		}
	}
	QTableView::keyPressEvent(event);
}
