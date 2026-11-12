#include "CTableData.h"
#include <QApplication>
#include <QAbstractItemModelTester>
#include <QCheckBox>
#include <QComboBox>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTest>
#include <iostream>
#include <crtdbg.h>
#include <cstdlib>
#include <stdexcept>

namespace
{
	void Require(bool result, const char* message)
	{
		if (!result)
		{
			throw std::runtime_error(message);
		}
	}

	void RunTests()
	{
		CDataTable table;
		std::cerr << "Construct table\n";
		CUITable widget;
		QAbstractItemModelTester sourceTester(widget.DataModel(), QAbstractItemModelTester::FailureReportingMode::Fatal);
		QAbstractItemModelTester proxyTester(widget.model(), QAbstractItemModelTester::FailureReportingMode::Fatal);
		std::cerr << "Define data\n";
		DefineTable(table, { { 1, "Name", DataType::String }, { 2, "Value", DataType::Int64 } });
		PublishTable(table, widget, { { 101, { std::string("alpha"), std::int64_t(20) } }, { 202, { std::string("beta"), std::int64_t(3) } }, { 303, { std::string("alpha hidden"), std::int64_t(10) } } });
		CTablePresentation presentation;
		presentation.m_columns = { { TableCheckColumn, "" }, { 1, "Name" }, { 2, "Value" }, { TableActionColumn, "Actions" } };
		presentation.m_columns[1].m_editable = true;
		presentation.m_columns[1].m_choices = { "alpha", "gamma" };
		presentation.m_cells[{ 101, TableActionColumn }].m_actions = { { "go", "Go", true }, { "blocked", "Blocked", false } };
		std::cerr << "Set presentation\n";
		widget.DataModel()->SetPresentation(presentation);
		QSignalSpy operations(&widget, &CUITable::OperationRequested);
		std::cerr << "Show table\n";
		widget.resize(800, 350);
		widget.show();
		QApplication::processEvents();
		Require(widget.ResultCount() == 3, "initial rows");
		std::cerr << "Sorting\n";
		widget.setSortingEnabled(true);
		widget.sortByColumn(2, Qt::AscendingOrder);
		Require(widget.model()->index(0, 2).data(TableRowIdRole).toULongLong() == 202, "numeric sort");
		Require(widget.SelectRowId(101), "select stable ID");
		widget.sortByColumn(2, Qt::DescendingOrder);
		Require(widget.SelectedRowId() == 101, "selection survives sorting");
		std::cerr << "Checks\n";
		widget.SetCheckedRowIds({ 202 });
		widget.SetSearchColumns({ 1 });
		widget.Search("alpha");
		Require(widget.ResultCount() == 2, "search filters display values");
		QCheckBox* checkAll = widget.findChild<QCheckBox*>();
		Require(nullptr != checkAll && checkAll->isVisible(), "header checkbox");
		operations.clear();
		checkAll->click();
		int checkMessages = 0;
		for (const auto& args : operations)
		{
			if (TableOperationKind::ChecksChanged == args[0].value<CTableOperation>().m_kind)
			{
				++checkMessages;
			}
		}
		Require(checkMessages == 1, "one bulk check notification");
		Require(widget.CheckedRowIds() == QVector<qulonglong>({ 101, 202, 303 }), "hidden checks retained");
		checkAll->click();
		Require(widget.CheckedRowIds() == QVector<qulonglong>({ 202 }), "uncheck visible rows only");
		widget.Search("");
		widget.SetCheckedRowIds({ 101 });
		Require(checkAll->checkState() == Qt::PartiallyChecked, "partial selection");
		PublishTable(table, widget, { { 303, { std::string("alpha hidden"), std::int64_t(11) } }, { 101, { std::string("alpha"), std::int64_t(21) } } });
		Require(widget.CheckedRowIds() == QVector<qulonglong>({ 101 }), "checks survive reorder and delete");
		Require(widget.SelectedRowId() == 101, "selection survives reset");
		operations.clear();
		CDataTableModel* model = widget.DataModel();
		_TyDataRowIndex row = 0;
		Require(model->GetView().FindRow(101, row), "find row ID");
		Require(model->setData(model->index(static_cast<int>(row), 1), "gamma"), "edit request accepted");
		Require(model->data(model->index(static_cast<int>(row), 1)).toString() == "alpha", "edit must not write back");
		CTableOperation edit;
		for (const auto& args : operations)
		{
			CTableOperation message = args[0].value<CTableOperation>();
			if (TableOperationKind::Edit == message.m_kind)
			{
				edit = message;
			}
		}
		Require(edit.m_rowId == 101 && edit.m_columnId == 1 && edit.m_originalValue.toString() == "alpha" && edit.m_value.toString() == "gamma" && edit.m_requestId != 0, "owned edit message");
		Require(edit.m_generation == table.GetView().GetSourceId(), "source identity");
		operations.clear();
		widget.SelectRowId(101);
		QModelIndex action = widget.model()->index(widget.currentIndex().row(), 3);
		widget.TriggerAction(action, "blocked");
		Require(operations.empty(), "disabled action does not notify");
		widget.TriggerAction(action, "go");
		Require(operations.size() == 1 && operations[0][0].value<CTableOperation>().m_rowId == 101, "action targets stable ID after sorting");
		operations.clear();
		QRect actionRect = widget.visualRect(action);
		QPoint buttonPoint(actionRect.left() + 8 + (actionRect.width() - 22) / 4, actionRect.center().y());
		QTest::mouseClick(widget.viewport(), Qt::LeftButton, Qt::NoModifier, buttonPoint);
		int mouseActions = 0;
		for (const auto& args : operations)
		{
			if (TableOperationKind::Action == args[0].value<CTableOperation>().m_kind)
			{
				++mouseActions;
			}
		}
		Require(mouseActions == 1, "delegate emits one action per click");
		operations.clear();
		QTest::mouseRelease(widget.viewport(), Qt::LeftButton, Qt::NoModifier, buttonPoint);
		for (const auto& args : operations)
		{
			Require(TableOperationKind::Action != args[0].value<CTableOperation>().m_kind, "release without matching press activated action");
		}
		QSignalSpy changes(model, &QAbstractItemModel::dataChanged);
		CTableCell display;
		display.m_display = "formatted";
		model->SetCells({ { { 101, 2 }, display } });
		Require(!changes.empty() && model->data(model->index(static_cast<int>(row), 2)).toString() == "formatted", "presentation-only refresh");
		Require(model->data(model->index(static_cast<int>(row), 2), TableRawValueRole).toLongLong() == 21, "raw data independent of formatting");
		QAbstractItemDelegate* delegate = widget.itemDelegate();
		QWidget* editor = delegate->createEditor(&widget, QStyleOptionViewItem(), model->index(static_cast<int>(row), 1));
		Require(nullptr != qobject_cast<QComboBox*>(editor), "generic choice editor");
		delete editor;
		QSortFilterProxyModel businessProxy;
		businessProxy.setSourceModel(model);
		widget.setModel(&businessProxy);
		Require(widget.SelectRowId(101), "multi-proxy selection");
		operations.clear();
		widget.TriggerAction(widget.model()->index(widget.currentIndex().row(), 3), "go");
		Require(operations.size() == 1 && operations[0][0].value<CTableOperation>().m_rowId == 101, "multi-proxy action ID");
		widget.setModel(model);
		CDataTable replacement;
		DefineTable(replacement, { { 1, "Replacement", DataType::String }, { 2, "Value", DataType::Int64 } });
		PublishTable(replacement, widget, { { 101, { std::string("new source"), std::int64_t(8) } } });
		Require(widget.SelectedRowId() == 0 && widget.CheckedRowIds().empty(), "source replacement clears selection and checks");
		Require(widget.model()->index(0, 2).data().toLongLong() == 8, "old presentation cells cleared on replacement");
		QSignalSpy resets(model, &QAbstractItemModel::modelReset);
		CDataTable newSchema;
		DefineTable(newSchema, { { 9, "Other", DataType::String }, { 10, "Count", DataType::Int64 } });
		CTablePresentation automatic;
		model->SetPresentation(automatic);
		PublishTable(newSchema, widget, { { 909, { std::string("other"), std::int64_t(7) } } });
		Require(!resets.empty() && model->headerData(0, Qt::Horizontal).toString() == "Other", "same-width schema replacement");
		CDataTableWriter first = newSchema.BeginWrite();
		first.SetValue(909, 10, std::int64_t(9));
		first.Commit();
		CDataTableWriter second = newSchema.BeginWrite();
		second.SetValue(909, 9, std::string("latest"));
		std::pair<CDataTableView, CDataChangeSet> skipped = second.Commit();
		model->SetView(skipped.first, skipped.second);
		Require(model->data(model->index(0, 1)).toLongLong() == 9, "version gap gets complete snapshot");
		PublishTable(newSchema, widget, {});
		Require(widget.ResultCount() == 0 && widget.CheckedRowIds().empty(), "empty table");
		widget.setModel(nullptr);
	}
} // namespace

int main(int argc, char** argv)
{
	_set_error_mode(_OUT_TO_STDERR);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&, const QString& message)
	{
		std::cerr << message.toStdString() << '\n';
		if (QtFatalMsg == type)
		{
			std::_Exit(2);
		}
	});
	std::cerr << "Starting table tests\n";
	QApplication application(argc, argv);
	try
	{
		RunTests();
		std::cout << "Table component tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
