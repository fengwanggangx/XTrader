#include "components/CLoginWindow.h"
#include "components/CMainWindow.h"
#include "log/defines_log.h"
#include "system/CBootLoader.h"

#include <QtWidgets/QApplication>
#include <QIcon>

int main(int argc, char* argv[])
{
	QApplication application(argc, argv);
	application.setApplicationName(QStringLiteral("XTrader"));
	application.setWindowIcon(QIcon(":/branding/xtrader-app-icon.png"));
	CLogger::Initialize("xtrader");
	MLOG_INFO(md::modules::global, "Application starting");
	CBootLoader boot;
	if (!boot.Initialize())
	{
		GLOBAL_LOG_ERR("{}", boot.GetLastError());
		return boot.GetErrorCode();
	}

	if (!boot.Run())
	{
		GLOBAL_LOG_ERR("{}", boot.GetLastError());
		return boot.GetErrorCode();
	}

	int result = 0;
	{
		LoginWindow loginWindow(boot.GetServices());
		if (QDialog::Accepted == loginWindow.exec())
		{
			CMainWindow mainWindow(boot.GetServices());
			mainWindow.show();
			result = application.exec();
			boot.Stop();
		}
		else
		{
			boot.Stop();
		}
	}
	boot.Finalize();
	MLOG_INFO(md::modules::global, "Application stopped");
	return result;
}
