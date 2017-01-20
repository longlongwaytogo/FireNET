// Copyright (C) 2014-2017 Ilya Chernetsov. All rights reserved. Contacts: <chernecoff@gmail.com>
// License: https://github.com/afrostalin/FireNET/blob/master/LICENSE

#include <QThread>

#include "global.h"
#include "mysqlconnector.h"
#include "dbworker.h"

#include "Tools/settings.h"

MySqlConnector::MySqlConnector(QObject *parent) : QObject(parent),
	connectStatus(false)
{
}

MySqlConnector::~MySqlConnector()
{
	qDebug() << "~MySqlConnector";
}

void MySqlConnector::run()
{
	gEnv->pDBWorker->m_Status = EDB_StartConnecting;

	if (Connect())
	{
		qInfo() << "MySQL connected. Work on" << QThread::currentThread();
		connectStatus = true;
		gEnv->pDBWorker->m_Status = EDB_Connected;
	}
	else
	{
		qCritical() << "Failed connect to MySQL! MySQL functions not be work!";
		gEnv->pDBWorker->m_Status = EDB_NoConnection;
		return;
	}
}

void MySqlConnector::Disconnect()
{
	gEnv->pDBWorker->m_Status = EDB_NoConnection;

	if (connectStatus)
	{
		qInfo() << "Disconnecting...";
		m_db.close();
	}
}

bool MySqlConnector::Connect()
{
	qDebug() << "Connecting to MySql host...(" << gEnv->pSettings->GetVariable("mysql_host").toString() << ":" << gEnv->pSettings->GetVariable("mysql_port").toInt() << ")";

	m_db = QSqlDatabase::addDatabase("QMYSQL", "mySqlDatabase");
	m_db.setHostName(gEnv->pSettings->GetVariable("mysql_host").toString());
	m_db.setPort(gEnv->pSettings->GetVariable("mysql_port").toInt());
	m_db.setDatabaseName(gEnv->pSettings->GetVariable("mysql_db_name").toString());
	m_db.setUserName(gEnv->pSettings->GetVariable("mysql_username").toString());
	m_db.setPassword(gEnv->pSettings->GetVariable("mysql_password").toString());

	return m_db.open();
}

QSqlDatabase MySqlConnector::GetDatabase()
{
	return m_db;
}


