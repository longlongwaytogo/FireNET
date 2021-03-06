// Copyright (C) 2014-2017 Ilya Chernetsov. All rights reserved. Contacts: <chernecoff@gmail.com>
// License: https://github.com/afrostalin/FireNET/blob/master/LICENSE

#pragma once

#include <FireNet>

class CUdpPacket;

struct SFireNetSpawnPosition
{
	SFireNetSpawnPosition()
	{
		m_Pos(0, 0, 0);
		m_Rot.w = 1;
		m_Rot.v(0, 0, 0);
		m_Team = "none";
		b_Finded = false;
	}

	Vec3   m_Pos;
	Quat   m_Rot;
	string m_Team;

	bool   b_Finded;
};

class CReadQueue
{
public:
	CReadQueue(uint32 id) : m_ClientChannelID(id)
	{
		m_LastInputPacketNumber = 0;
		m_LastOutputPacketNumber = 0;
		m_LastPacketTime = gEnv->pTimer->GetAsyncCurTime();
	}
	~CReadQueue() {}
public:
	void                  ReadPacket(CUdpPacket &packet);
	float                 GetLastTime() { return m_LastPacketTime; }
private:
	void                  ReadAsk(CUdpPacket &packet, EFireNetUdpAsk ask);
	void                  ReadPing();
	void                  ReadRequest(CUdpPacket &packet, EFireNetUdpRequest request);
private:
	void                  SendPacket(CUdpPacket &packet);
	void                  SendPacketToAll(CUdpPacket &packet);
	void                  SendPacketToAllExcept(uint32 id, CUdpPacket &packet);
private:
	SFireNetSpawnPosition FindSpawnPosition();
private:
	uint32                m_ClientChannelID;

	int                   m_LastInputPacketNumber;
	int                   m_LastOutputPacketNumber;

	float                 m_LastPacketTime;
};