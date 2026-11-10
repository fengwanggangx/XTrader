#include "CKeyboardSprite.h"

#include <QApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
	int MatchRank(const QStringList& values, const QString& strQuery)
	{
		int nRank = 3;
		for (const QString& value : values)
		{
			QString strValue = value.trimmed();
			if (strValue == strQuery)
			{
				return 0;
			}
			if (strValue.startsWith(strQuery))
			{
				nRank = (std::min)(nRank, 1);
			}
			else if (strValue.contains(strQuery))
			{
				nRank = (std::min)(nRank, 2);
			}
		}
		return nRank;
	}
} // namespace

CKeyboardSprite::CKeyboardSprite(QWidget* pParent) : QWidget(pParent, Qt::Tool | Qt::FramelessWindowHint)
{
	setObjectName("keyboardSprite");
	setFixedSize(432, 462);
	QVBoxLayout* pLayout = new QVBoxLayout(this);
	pLayout->setContentsMargins(1, 1, 1, 1);
	pLayout->setSpacing(0);
	QWidget* pTitle = new QWidget(this);
	pTitle->setObjectName("keyboardSpriteTitle");
	pTitle->setFixedHeight(39);
	QHBoxLayout* pTitleLayout = new QHBoxLayout(pTitle);
	pTitleLayout->setContentsMargins(12, 0, 8, 0);
	pTitleLayout->addWidget(new QLabel(QStringLiteral("闪电量化键盘精灵"), pTitle));
	pTitleLayout->addStretch();
	QPushButton* pClose = new QPushButton(QStringLiteral("×"), pTitle);
	pClose->setObjectName("keyboardSpriteClose");
	pClose->setFixedSize(27, 27);
	connect(pClose, &QPushButton::clicked, this, &QWidget::hide);
	pTitleLayout->addWidget(pClose);
	pLayout->addWidget(pTitle);
	m_pSearch = new QLineEdit(this);
	m_pSearch->setObjectName("keyboardSpriteSearch");
	m_pSearch->setFixedHeight(31);
	m_pSearch->installEventFilter(this);
	connect(m_pSearch, &QLineEdit::textChanged, this, &CKeyboardSprite::Refresh);
	pLayout->addWidget(m_pSearch);
	m_pContent = new QStackedWidget(this);
	m_pMessage = new QLabel(this);
	m_pMessage->setObjectName("keyboardSpriteMessage");
	m_pMessage->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_pMessage->setWordWrap(true);
	m_pMessage->setContentsMargins(22, 40, 22, 0);
	m_pContent->addWidget(m_pMessage);
	m_pResults = new QTableWidget(this);
	m_pResults->setObjectName("keyboardSpriteResults");
	m_pResults->setColumnCount(3);
	m_pResults->horizontalHeader()->hide();
	m_pResults->verticalHeader()->hide();
	m_pResults->setShowGrid(false);
	m_pResults->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pResults->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pResults->installEventFilter(this);
	m_pResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
	m_pResults->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_pResults->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
	m_pResults->setColumnWidth(0, 110);
	m_pResults->setColumnWidth(2, 76);
	m_pResults->verticalHeader()->setDefaultSectionSize(30);
	connect(m_pResults, &QTableWidget::cellDoubleClicked, this, [this](int, int)
	{
		ConfirmSelection();
	});
	m_pContent->addWidget(m_pResults);
	pLayout->addWidget(m_pContent);
	Refresh();
}

void CKeyboardSprite::SetSecurities(const std::vector<CSecurity>& securities)
{
	m_securities = securities;
	m_searchEntries.clear();
	m_searchEntries.reserve(m_securities.size());
	for (const CSecurity& security : m_securities)
	{
		CSearchEntry entry;
		entry.m_values = { QString::fromStdString(security.m_strCode).toCaseFolded(), QString::fromStdString(security.m_strName).toCaseFolded() };
		for (const std::string& alias : security.m_pinyinFullAliases)
		{
			entry.m_values.append(QString::fromStdString(alias).toCaseFolded());
		}
		for (const std::string& alias : security.m_pinyinShortAliases)
		{
			entry.m_values.append(QString::fromStdString(alias).toCaseFolded());
		}
		m_searchEntries.emplace_back(std::move(entry));
	}
	Refresh();
}

void CKeyboardSprite::Open(const QString& strInitial)
{
	if (!isVisible())
	{
		m_pSearch->setText(strInitial);
		show();
	}
	else if (!strInitial.isEmpty())
	{
		m_pSearch->setText(strInitial);
	}
	raise();
	activateWindow();
	m_pSearch->setFocus();
	if (strInitial.isEmpty())
	{
		m_pSearch->selectAll();
	}
}

bool CKeyboardSprite::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (((m_pSearch == pObject) || (m_pResults == pObject)) && (QEvent::KeyPress == pEvent->type()))
	{
		QKeyEvent* pKey = static_cast<QKeyEvent*>(pEvent);
		if ((Qt::ControlModifier == pKey->modifiers()) && (Qt::Key_K == pKey->key()))
		{
			m_pSearch->setFocus();
			m_pSearch->selectAll();
			return true;
		}
		if ((Qt::Key_Down == pKey->key()) || (Qt::Key_Up == pKey->key()))
		{
			int nRow = m_pResults->currentRow() + (Qt::Key_Down == pKey->key() ? 1 : -1);
			if ((0 <= nRow) && (m_pResults->rowCount() > nRow))
			{
				m_pResults->selectRow(nRow);
				m_pResults->scrollToItem(m_pResults->item(nRow, 0));
			}
			return true;
		}
		if ((Qt::Key_Return == pKey->key()) || (Qt::Key_Enter == pKey->key()))
		{
			ConfirmSelection();
			return true;
		}
		if (Qt::Key_Escape == pKey->key())
		{
			hide();
			return true;
		}
	}
	if (QEvent::WindowDeactivate == pEvent->type())
	{
		hide();
	}
	return QWidget::eventFilter(pObject, pEvent);
}

void CKeyboardSprite::Refresh()
{
	QString strQuery = m_pSearch->text().trimmed().toCaseFolded();
	m_resultIndices.clear();
	if (strQuery.isEmpty())
	{
		m_pMessage->setText(QStringLiteral("1. 输入代码、名称、全拼或简拼搜索A股证券\n\n2. 使用↑↓选择结果，按回车打开\n\n3. 按 Esc 关闭键盘精灵"));
		m_pContent->setCurrentWidget(m_pMessage);
		return;
	}
	std::size_t nSize = m_securities.size();
	for (std::size_t nIndex = 0; nSize > nIndex; ++nIndex)
	{
		if ((Exchange::sse != m_securities[nIndex].m_hq) && (Exchange::szse != m_securities[nIndex].m_hq) && (Exchange::bse != m_securities[nIndex].m_hq))
		{
			continue;
		}
		if (3 > MatchRank(m_searchEntries[nIndex].m_values, strQuery))
		{
			m_resultIndices.emplace_back(nIndex);
		}
	}
	std::sort(m_resultIndices.begin(), m_resultIndices.end(), [this, &strQuery](std::size_t nLeft, std::size_t nRight)
	{
		const CSecurity& left = m_securities[nLeft];
		const CSecurity& right = m_securities[nRight];
		int nLeftRank = MatchRank(m_searchEntries[nLeft].m_values, strQuery);
		int nRightRank = MatchRank(m_searchEntries[nRight].m_values, strQuery);
		if (nLeftRank != nRightRank)
		{
			return nLeftRank < nRightRank;
		}
		if (left.m_strCode != right.m_strCode)
		{
			return left.m_strCode < right.m_strCode;
		}
		return left.m_hq < right.m_hq;
	});
	if (m_resultIndices.empty())
	{
		m_pMessage->setText(m_securities.empty() ? QStringLiteral("证券列表尚未加载") : QStringLiteral("没有匹配的证券"));
		m_pContent->setCurrentWidget(m_pMessage);
		return;
	}
	m_pResults->setRowCount(static_cast<int>(m_resultIndices.size()));
	int nRow = 0;
	for (std::size_t nIndex : m_resultIndices)
	{
		const CSecurity& security = m_securities[nIndex];
		QString strMarket = Exchange::sse == security.m_hq ? QStringLiteral("沪A") : Exchange::szse == security.m_hq ? QStringLiteral("深A")
																													 : QStringLiteral("北交所");
		QString values[] = { QString::fromStdString(security.m_strCode), QString::fromStdString(security.m_strName), strMarket };
		for (int nColumn = 0; 3 > nColumn; ++nColumn)
		{
			QTableWidgetItem* pItem = new QTableWidgetItem(values[nColumn]);
			pItem->setToolTip(values[nColumn]);
			if (2 == nColumn)
			{
				pItem->setForeground(QColor("#398bc7"));
				pItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
			}
			m_pResults->setItem(nRow, nColumn, pItem);
		}
		++nRow;
	}
	m_pContent->setCurrentWidget(m_pResults);
	m_pResults->selectRow(0);
}

void CKeyboardSprite::ConfirmSelection()
{
	int nRow = m_pResults->currentRow();
	if ((0 > nRow) || (m_resultIndices.size() <= static_cast<std::size_t>(nRow)))
	{
		return;
	}
	CSecurity security = m_securities[m_resultIndices[static_cast<std::size_t>(nRow)]];
	hide();
	emit SecuritySelected(security);
}
