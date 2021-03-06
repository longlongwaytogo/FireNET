// Copyright (C) 2014-2017 Ilya Chernetsov. All rights reserved. Contacts: <chernecoff@gmail.com>
// License: https://github.com/afrostalin/FireNET/blob/master/LICENSE

#ifndef TCPTHREAD_H
#define TCPTHREAD_H

#include <QObject>
#include <QThread>
#include <QRunnable>
#include <QEventLoop>
#include <QDebug>
#include <QReadWriteLock>
#include <QReadLocker>

#include "tcpthread.h"
#include "tcpconnection.h"

#include "Workers/Databases/redisconnector.h"

class TcpThread : public QObject, public QRunnable
{
    Q_OBJECT
public:
    explicit TcpThread(QObject *parent = nullptr);
    ~TcpThread();
public:
	void                  run();
	int                   Count();
	void                  SendGlobalMessage(CTcpPacket &packet);
private:
	TcpConnection*        CreateConnection();
	void                  AddSignals(TcpConnection* connection);
public slots:
	void                  connecting(qintptr handle, TcpThread *runnable, TcpConnection* connection);
	void                  closing();
	void                  opened();
	void                  closed();
signals:
	void                  started();
	void                  finished();
	void                  quit();
private:
	QEventLoop*           m_loop;
	QReadWriteLock        m_lock;
	QList<TcpConnection*> m_connections;
};

#endif // TCPTHREAD_H
