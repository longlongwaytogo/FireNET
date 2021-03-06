// Copyright (C) 2014-2017 Ilya Chernetsov. All rights reserved. Contacts: <chernecoff@gmail.com>
// License: https://github.com/afrostalin/FireNET/blob/master/LICENSE

#include <QThread>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <Logger.h>
#include <FileAppender.h>
#include <QMetaObject>

#include "global.h"
#include "serverThread.h"

#include "Core/tcpserver.h"
#include "Core/remoteserver.h"

#include "Workers/Databases/dbworker.h"
#include "Workers/Databases/mysqlconnector.h"

#include "Tools/settings.h"
#include "Tools/scripts.h"

#include "UI/UILogger.h"
#include "UI/mainwindow.h"

FileAppender* pLogFileAppender;
UILogger* pLogUIAppender;

CServerThread::CServerThread(QObject *parent) : QObject(parent),
	m_loop(nullptr)
{
}

CServerThread::~CServerThread()
{
	SAFE_RELEASE(m_loop);
}

// !Callbacks

void UpdateFileLogLevel(QVariant variable)
{
	Logger::LogLevel logLevel;

	int lvl = variable.toInt();

	switch (lvl)
	{
	case 0:
	{
		logLevel = Logger::Info;
		pLogFileAppender->setFormat(QLatin1String("%{time}{<HH:mm:ss.zzz>} [%{type:-7}] %{message}\n"));
		break;
	}
	case 1:
	{
		logLevel = Logger::Debug;
		pLogFileAppender->setFormat(QLatin1String("%{time}{<HH:mm:ss.zzz>} [%{type:-7}] <%{function}> %{message}\n"));
	}
	default:
	{
		logLevel = Logger::Debug;
		pLogFileAppender->setFormat(QLatin1String("%{time}{<HH:mm:ss.zzz>} [%{type:-7}] <%{function}> %{message}\n"));
		break;
	}
	}

	pLogFileAppender->setDetailsLevel(logLevel);
	gEnv->m_FileLogLevel = lvl;
}

void UpdateUILogLevel(QVariant variable)
{
	Logger::LogLevel logLevel;

	int lvl = variable.toInt();

	switch (lvl)
	{
	case 0:
	{
		logLevel = Logger::Info;
		pLogUIAppender->setFormat(QLatin1String("[%{type:-7}] %{message}"));
		break;
	}
	case 1:
	{
		logLevel = Logger::Debug;
		pLogUIAppender->setFormat(QLatin1String("[%{type:-7}] <%{function}> %{message}"));
	}
	default:
	{
		logLevel = Logger::Debug;
		pLogUIAppender->setFormat(QLatin1String("[%{type:-7}] <%{function}> %{message}"));
		break;
	}
	}

	pLogUIAppender->setDetailsLevel(logLevel);
	gEnv->m_UILogLevel = lvl;
}

void UpdateMaxClientCount(QVariant variable)
{
	if (gEnv->pServer)
		gEnv->pServer->SetMaxConnections(variable.toInt());
}

void UpdateMaxRemoteClienCount(QVariant variable)
{
	if (gEnv->pRemoteServer)
		gEnv->pRemoteServer->SetMaxClientCount(variable.toInt());
}

// ~Callbacks

bool CServerThread::Init()
{
	if (!QFile::exists("key.key") || !QFile::exists("key.pem"))
	{
		qCritical() << "Server can't start - SSL key files not found. First create SSL keys for starting server!";
		return false;
	}

	if (!QFile::exists("FireNET.cfg"))
	{
		qWarning() << "Not found FireNET.cfg! Using default settings...";
	}

	if (!QFile::exists("scripts/shop.xml"))
	{
		qWarning() << "Not found shop.xml! Shop system not be work!";
	}

	return true;
}

void CServerThread::StartLogging()
{
	// Check exists logs folder
	if (!QDir("logs").exists())
		QDir().mkdir("logs");

	QDateTime* pDate = new QDateTime();
	QString m_Date = pDate->currentDateTime().toString("dd.MM.yyyy");
	SAFE_DELETE(pDate);

	QString m_LogFileName = "logs/FireNET["+ m_Date + "].log";

	// Backup old log file
	QFile::remove(m_LogFileName + ".bak");
	QFile::rename(m_LogFileName, m_LogFileName + ".bak");
	QFile::remove(m_LogFileName);

#ifdef QT_NO_DEBUG
	int m_LogLevel = 0;
#else
	int m_LogLevel = 2;
#endif

	// Init logging tool
	pLogFileAppender = new FileAppender(m_LogFileName);
	pLogUIAppender = new UILogger();

	UpdateFileLogLevel(m_LogLevel);
	UpdateUILogLevel(m_LogLevel);
	
	logger->registerAppender((AbstractAppender*)pLogFileAppender);
	logger->registerAppender((AbstractAppender*)pLogUIAppender);
}

void CServerThread::RegisterVariables()
{
	// Server vars
	gEnv->pSettings->RegisterVariable("sv_ip", "127.0.0.1", "Main server ip address", false);
	gEnv->pSettings->RegisterVariable("sv_port", 3322, "Main server port", false);
#ifdef QT_NO_DEBUG
	gEnv->pSettings->RegisterVariable("sv_file_log_level", 0, "Log level for control debugging messages in log file [0-1]", true, &UpdateFileLogLevel);
	gEnv->pSettings->RegisterVariable("sv_ui_log_level", 0, "Log level for control debugging messages in UI [0-2]", true, &UpdateUILogLevel);
#else
	gEnv->pSettings->RegisterVariable("sv_file_log_level", 1, "Log level for control debugging messages in log file [0-1]", true, &UpdateFileLogLevel);
	gEnv->pSettings->RegisterVariable("sv_ui_log_level", 2, "Log level for control debugging messages in UI [0-2]", true, &UpdateUILogLevel);
#endif
	gEnv->pSettings->RegisterVariable("sv_thread_count", 1, "Main server thread count for thread pooling", false);
	gEnv->pSettings->RegisterVariable("sv_max_players", 1000, "Maximum players count for connection", true, &UpdateMaxClientCount);
	gEnv->pSettings->RegisterVariable("sv_max_servers", 10, "Maximum game servers count for connection", true, &UpdateMaxRemoteClienCount);
	gEnv->pSettings->RegisterVariable("sv_tickrate", 30, "Main server tick rate speed (30 by Default)", false);
	// Remote server vars
	gEnv->pSettings->RegisterVariable("remote_root_user", "administrator", "Remote admin login", true);
	gEnv->pSettings->RegisterVariable("remote_root_password", "qwerty", "Remote admin password", true);
	gEnv->pSettings->RegisterVariable("remote_server_port", 64000, "Remote server port", false);
	// Database vars
	gEnv->pSettings->RegisterVariable("db_mode", "Redis", "Database mode [Redis, MySql, Redis+MySql]", false);
	// Redis vars
	gEnv->pSettings->RegisterVariable("redis_ip", "127.0.0.1", "Redis database ip address", false);
	gEnv->pSettings->RegisterVariable("redis_port", 6379, "Redis database port", false);
	gEnv->pSettings->RegisterVariable("redis_bg_saving", false, "Use redis background saving", true);
	// MySQL vars
	gEnv->pSettings->RegisterVariable("mysql_host", "127.0.0.1", "MySql database ip address", false);
	gEnv->pSettings->RegisterVariable("mysql_port", 3306, "MySql database port", false);
	gEnv->pSettings->RegisterVariable("mysql_db_name", "FireNET", "MySql database name", false);
	gEnv->pSettings->RegisterVariable("mysql_username", "admin", "MySql username", false);
	gEnv->pSettings->RegisterVariable("mysql_password", "password", "MySql password", false);
	// Network vars
	gEnv->pSettings->RegisterVariable("net_encryption_timeout", 3, "Network timeout for new connection", true);
	gEnv->pSettings->RegisterVariable("net_magic_key", 2016207, "Network magic key for check packets for validations", true);
	gEnv->pSettings->RegisterVariable("net_max_packet_read_size", 512 , "Maximum packet size for reading", true);
	gEnv->pSettings->RegisterVariable("net_max_bad_packets_count", 10, "Maximum bad packets count from client", true);
	gEnv->pSettings->RegisterVariable("net_max_packets_speed", 4, "Maximum packets per second count by client", true);
	gEnv->pSettings->RegisterVariable("net_packet_debug", false, "Enable/Disable packet debugging", true);
	// Utils
	gEnv->pSettings->RegisterVariable("stress_mode", false, "Changes server settings to work with stress test", false);
	gEnv->pSettings->RegisterVariable("bUseGlobalChat", false, "Enable/Disable global chat", true);
	// Gloval vars (This variables not need read from server.cfg)
	gEnv->pSettings->RegisterVariable("bUseRedis", true, "Enable/Disable using Redis database", false);
	gEnv->pSettings->RegisterVariable("bUseMySQL", false, "Enable/Disable using MySql database", false);
	gEnv->pSettings->RegisterVariable("bUseHttpAuth", false, "Enable/Disable using HTTP authorization", false);
}

void CServerThread::ReadServerCFG()
{
	QSettings settings(QString("FireNET.cfg"), QSettings::IniFormat);

	QStringList serverCFG = settings.allKeys();

	// Read server.cfg and try set new variable values
	for (int i = 0; i < serverCFG.size(); ++i)
	{
		if (serverCFG[i].contains("#") || serverCFG[i].contains("//"))
			continue;

		QVariant::Type var_type = gEnv->pSettings->GetVariable(serverCFG[i]).type();

		switch (var_type)
		{
		case QVariant::Bool:
		{
			bool value = (settings.value(serverCFG[i], gEnv->pSettings->GetVariable(serverCFG[i])).toInt() == 1) ? true : false;
			gEnv->pSettings->SetVariable(serverCFG[i], value);
			break;
		}
		case QVariant::Int:
		{
			int value = settings.value(serverCFG[i], gEnv->pSettings->GetVariable(serverCFG[i])).toInt();
			gEnv->pSettings->SetVariable(serverCFG[i], value);
			break;
		}
		case QVariant::Double:
		{
			double value = settings.value(serverCFG[i], gEnv->pSettings->GetVariable(serverCFG[i])).toDouble();
			gEnv->pSettings->SetVariable(serverCFG[i], value);
			break;
		}
		case QVariant::String:
		{
			QString value = settings.value(serverCFG[i], gEnv->pSettings->GetVariable(serverCFG[i])).toString();
			gEnv->pSettings->SetVariable(serverCFG[i], value);
			break;
		}
		default:
			break;
		}
	}

	if (gEnv->pSettings->GetVariable("stress_mode").toBool())
		EnableStressTest();
}

void CServerThread::EnableStressTest()
{
	gEnv->pSettings->SetVariable("sv_ui_log_level", 0);
	gEnv->pSettings->SetVariable("sv_file_log_level", 0);
	gEnv->pSettings->SetVariable("sv_max_players", 1000);
	gEnv->pSettings->SetVariable("net_max_bad_packets_count", 1000);
	gEnv->pSettings->SetVariable("net_max_packets_speed", 100);
	gEnv->pSettings->SetVariable("net_packet_debug", false);

	emit EnableStressMode();
}

void CServerThread::start()
{
	m_loop = new QEventLoop();

	// Init global environment
	gEnv->pServer = new TcpServer;
	gEnv->pRemoteServer = new RemoteServer;
	gEnv->pDBWorker = new DBWorker;
	gEnv->pTimer = new QTimer;
	gEnv->pSettings = new SettingsManager;
	gEnv->pScripts = new Scripts;

	// Connect pTimer with Update functions
	QObject::connect(gEnv->pTimer, &QTimer::timeout, gEnv->pServer, &TcpServer::Update);
	QObject::connect(gEnv->pTimer, &QTimer::timeout, gEnv->pRemoteServer, &RemoteServer::Update);

	if (Init())
	{
		StartLogging();

		qInfo() << gEnv->m_serverFullName.toStdString().c_str();
		qInfo() << "Created by Ilya Chernetsov";
		qInfo() << "Copyright (c) 2014-2017. All rights reserved";

		RegisterVariables();
		ReadServerCFG();

		// Load scripts 
		gEnv->pScripts->LoadShopScript();
		gEnv->pScripts->LoadTrustedServerList();

		if (gEnv->pSettings->GetVariable("db_mode").toString() == "Redis")
		{
			gEnv->pSettings->SetVariable("bUseRedis", true);
			gEnv->pSettings->SetVariable("bUseMySQL", false);
		}
		else if (gEnv->pSettings->GetVariable("db_mode").toString() == "MySql")
		{
			gEnv->pSettings->SetVariable("bUseRedis", false);
			gEnv->pSettings->SetVariable("bUseMySQL", true);
		}
		else if (gEnv->pSettings->GetVariable("db_mode").toString() == "Redis+MySql")
		{
			gEnv->pSettings->SetVariable("bUseRedis", true);
			gEnv->pSettings->SetVariable("bUseMySQL", true);
		}

		gEnv->m_ServerStatus.m_DBMode = gEnv->pSettings->GetVariable("db_mode").toString();

		// Block online update for some variables
		gEnv->pSettings->BlockOnlineUpdate();

		qInfo() << "Start server on" << gEnv->pSettings->GetVariable("sv_ip").toString();

		gEnv->pServer->SetMaxThreads(gEnv->pSettings->GetVariable("sv_thread_count").toInt());

		if (gEnv->pServer->Listen(QHostAddress(gEnv->pSettings->GetVariable("sv_ip").toString()), gEnv->pSettings->GetVariable("sv_port").toInt()))
		{
			qInfo() << "Server started. Main thread " << QThread::currentThread();

			// Start remote server
			gEnv->pRemoteServer->run();		

			// Set server tickrate
			qInfo() << "Server tickrate set to" << gEnv->pSettings->GetVariable("sv_tickrate").toInt() << "per/sec.";
			int tick = 1000 / gEnv->pSettings->GetVariable("sv_tickrate").toInt();
			gEnv->pTimer->start(tick);

			// Init connection to databases
			gEnv->pDBWorker->Init();
		}
		else
		{
			qCritical() << "Server can't start. Reason = " << gEnv->pServer->errorString();
		}
	}

	m_loop->exec();
}

void CServerThread::stop()
{
	if (gEnv->isQuiting)
		return;
	else
		gEnv->isQuiting = true;

	qInfo() << "FireNET quiting...";

	if (gEnv->pTimer)
		gEnv->pTimer->stop();

	SAFE_CLEAR(gEnv->pServer);
	SAFE_CLEAR(gEnv->pRemoteServer);
	SAFE_CLEAR(gEnv->pSettings);
	SAFE_CLEAR(gEnv->pScripts);
	SAFE_CLEAR(gEnv->pDBWorker);

	while (!gEnv->pServer->IsClosed())
	{
		QEventLoop loop;
		QTimer::singleShot(33, &loop, &QEventLoop::quit);
		loop.exec();
	}

	SAFE_RELEASE(gEnv->pTimer);
	SAFE_RELEASE(gEnv->pServer);
	SAFE_RELEASE(gEnv->pRemoteServer);
	SAFE_RELEASE(gEnv->pSettings);
	SAFE_RELEASE(gEnv->pScripts);
	SAFE_RELEASE(gEnv->pDBWorker);

	QThreadPool::globalInstance()->waitForDone(1000);	

	if (m_loop)
		m_loop->exit();	

	gEnv->isReadyToClose = true;
}