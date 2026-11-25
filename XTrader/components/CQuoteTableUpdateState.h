#ifndef XTRADER_COMPONENTS_CQUOTETABLEUPDATESTATE_H
#define XTRADER_COMPONENTS_CQUOTETABLEUPDATESTATE_H

#include "../basic/CDatable.h"

#include <mutex>
#include <utility>

class CQuoteTableUpdateState final
{
public:
	void Publish(const CDataTableView& view, const CDataChangeSet& changes)
	{
		std::lock_guard<std::mutex> lock(m_mtx_state);
		m_view = view;
		m_changes = changes;
		m_bPending = true;
	}

	bool Take(CDataTableView& view, CDataChangeSet& changes)
	{
		std::lock_guard<std::mutex> lock(m_mtx_state);
		if (!m_bPending)
		{
			return false;
		}
		view = std::move(m_view);
		changes = std::move(m_changes);
		m_bPending = false;
		return true;
	}

private:
	std::mutex m_mtx_state;
	CDataTableView m_view;
	CDataChangeSet m_changes;
	bool m_bPending{ false };
};

#endif
