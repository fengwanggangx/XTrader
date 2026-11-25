#pragma once

#include "TMessagePump.h"
#include <QMetaObject>
#include <QPointer>
#include <QObject>

template <typename Event, typename Owner>
_TyCallbackId BindToQt(TMessagePump<Event>& dispatcher, Owner* owner, std::function<void(const Event&)> callback)
{
	QPointer<Owner> safeOwner(owner);
	return dispatcher.Subscribe([safeOwner, callback = std::move(callback)](const Event& event)
	{
		if (safeOwner.isNull())
		{
			return;
		}
		QMetaObject::invokeMethod(safeOwner, [safeOwner, callback, event]()
		{
			if (!safeOwner.isNull() && callback)
			{
				callback(event);
			}
		}, Qt::QueuedConnection);
	});
}
