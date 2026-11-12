#ifndef XTRADER_COMPONENTS_CKEYBOARDSPRITE_H
#define XTRADER_COMPONENTS_CKEYBOARDSPRITE_H

#include "../request/MarketTypes.h"
#include <QWidget>
#include <QStringList>
#include <vector>
#include "CTableData.h"
#include <map>

class QLineEdit;

class QLabel;
class QStackedWidget;

class CKeyboardSprite final : public QWidget
{
	Q_OBJECT

	public:
	explicit CKeyboardSprite(QWidget* pParent = nullptr);
	void SetSecurities(const std::vector<CSecurity>& securities);
	void Open(const QString& strInitial = QString());

	signals:
	void SecuritySelected(const CSecurity& security);

	protected:
	bool eventFilter(QObject* pObject, QEvent* pEvent) override;

	private:
	struct CSearchEntry
	{
		QStringList m_values;
	};
	void Refresh();
	void ConfirmSelection(_TyDataRowId rowId = 0);
	CDataTable m_resultsData;
	std::map<std::string, _TyDataRowId> m_securityIds;
	_TyDataRowId m_nextSecurityId{ 1 };
	std::vector<CSecurity> m_securities;
	std::vector<CSearchEntry> m_searchEntries;
	QLineEdit* m_pSearch{ nullptr };
	CUITable* m_pResults{ nullptr };
	QLabel* m_pMessage{ nullptr };
	QStackedWidget* m_pContent{ nullptr };
	std::vector<std::size_t> m_resultIndices;
};

#endif
