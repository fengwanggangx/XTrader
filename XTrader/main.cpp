#include "components/CLoginWindow.h"
#include "components/CMainWindow.h"
#include "log/defines_log.h"
#include "system/CBootLoader.h"

#include <QtWidgets/QApplication>
#include <QIcon>

#include <iostream>

int main(int argc, char* argv[])
{
	QApplication application(argc, argv);
	application.setApplicationName(QStringLiteral("XTrader"));
	application.setWindowIcon(QIcon(":/branding/xtrader-app-icon.png"));
	CBootLoader boot;
	if (!boot.Initialize())
	{
		std::cerr << boot.GetLastError() << '\n';
		return boot.GetErrorCode();
	}

	if (!boot.Run())
	{
		std::cerr << boot.GetLastError() << '\n';
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
	CLogger::InstancePtr()->ShutDown();
	return result;
}
