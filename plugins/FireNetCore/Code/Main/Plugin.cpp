// Copyright (C) 2014-2017 Ilya Chernetsov. All rights reserved. Contacts: <chernecoff@gmail.com>
// License: https://github.com/afrostalin/FireNET/blob/master/LICENSE

#include "StdAfx.h"
#include "Plugin.h"

#include "Network/TcpClient.h"
#include "Network/NetworkThread.h"
#include "Network/TcpPacket.h"

#include <CryExtension/ICryPluginManager.h>
#include <CryCore/Platform/platform_impl.inl>

USE_CRYPLUGIN_FLOWNODES

IEntityRegistrator *IEntityRegistrator::g_pFirst = nullptr;
IEntityRegistrator *IEntityRegistrator::g_pLast = nullptr;

CFireNetCorePlugin::~CFireNetCorePlugin()
{
	// Unregister entities
	IEntityRegistrator* pTemp = IEntityRegistrator::g_pFirst;
	while (pTemp != nullptr)
	{
		pTemp->Unregister();
		pTemp = pTemp->m_pNext;
	}

	// Unregister CVars
	IConsole* pConsole = gEnv->pConsole;
	if (pConsole)
	{
		pConsole->UnregisterVariable("firenet_ip");
		pConsole->UnregisterVariable("firenet_port");
		pConsole->UnregisterVariable("firenet_timeout");
#ifndef NDEBUG
		pConsole->UnregisterVariable("firenet_packet_debug");
#endif
	}

	// Close network thread
	if (mEnv->pNetworkThread && mEnv->pTcpClient)
		mEnv->pNetworkThread->SignalStopWork();
	SAFE_DELETE(mEnv->pNetworkThread);

	// Unregister listeners
	if (gEnv && gEnv->pSystem)
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	// Clear FireNet core pointer
	if (gEnv)
		gEnv->pFireNetCore = nullptr;
}

bool CFireNetCorePlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this);

	// Init FireNet core pointer
	gEnv->pFireNetCore = this;

	if (initParams.bEditor)
		gEnv->SetIsEditor(true);

	if (initParams.bDedicatedServer)
		gEnv->SetIsDedicated(true);
	
	return true;
}

void CFireNetCorePlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_GAME_POST_INIT:
	{
		// Register entities
		IEntityRegistrator* pTemp = IEntityRegistrator::g_pFirst;
		while (pTemp != nullptr)
		{
			pTemp->Register();
			pTemp = pTemp->m_pNext;
		}

		// Register CVars
		mEnv->net_ip = REGISTER_STRING("firenet_ip", "127.0.0.1", VF_NULL, "Sets the FireNet master server ip address");
		REGISTER_CVAR2("firenet_port", &mEnv->net_port, 3322, VF_CHEAT, "FireNet master server port");
		REGISTER_CVAR2("firenet_timeout", &mEnv->net_timeout, 10, VF_NULL, "FireNet master server timeout");

#ifndef  NDEBUG // Only in debug mode
		REGISTER_CVAR2("firenet_packet_debug", &mEnv->net_debug, 0, VF_NULL, "FireNet packet debugging");
#endif // ! NDEBUG

		break;
	}
	case ESYSTEM_EVENT_GAME_FRAMEWORK_INIT_DONE:
	{
		if (gEnv->pGameFramework)
		{
			gEnv->pGameFramework->RegisterListener(this, "FireNetCore_GameFrameworkListener", FRAMEWORKLISTENERPRIORITY_DEFAULT);
			CryLog(TITLE "Framework listener registered");
		}
		else
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE "Can't register framework listener!");

		break;
	}
	default:
		break;
	}
}

void CFireNetCorePlugin::OnPostUpdate(float fDeltaTime)
{
}

void CFireNetCorePlugin::ConnectToMasterServer()
{
	if (mEnv->pNetworkThread)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE  "FireNet core thread alredy spawned!");
		return;
	}

	mEnv->SendFireNetEvent(FIRENET_EVENT_MASTER_SERVER_START_CONNECTION);

	mEnv->pNetworkThread = new CNetworkThread();
	if (!gEnv->pThreadManager->SpawnThread(mEnv->pNetworkThread, "FireNetCore_Thread"))
	{
		mEnv->SendFireNetEvent(FIRENET_EVENT_MASTER_SERVER_CONNECTION_ERROR);

		SAFE_DELETE(mEnv->pNetworkThread);
	
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE  "Can't spawn FireNet core thread!");
	}
	else
		CryLog(TITLE "FireNet core thread spawned");
}

void CFireNetCorePlugin::Authorization(const std::string & login, const std::string & password)
{
	CryLog(TITLE "Try authorizate by login and password");

	if (!login.empty() && !password.empty())
	{
		CTcpPacket packet(EFireNetTcpPacketType::Query);
		packet.WriteQuery(EFireNetTcpQuery::Login);
		packet.WriteString(login.c_str());
		packet.WriteString(password.c_str());

		SEND_PACKET(packet);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE  "Can't authorizate. Login or password empty");
	}	
}

void CFireNetCorePlugin::Registration(const std::string & login, const std::string & password)
{
	CryLog(TITLE "Try register new account by login and password");

	if (!login.empty() && !password.empty())
	{
		CTcpPacket packet(EFireNetTcpPacketType::Query);
		packet.WriteQuery(EFireNetTcpQuery::Register);
		packet.WriteString(login.c_str());
		packet.WriteString(password.c_str());

		SEND_PACKET(packet);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE  "Can't register new account. Login or password empty");
	}
}

void CFireNetCorePlugin::CreateProfile(const std::string & nickname, const std::string & character)
{
	CryLog(TITLE "Try create profile by nickname and character model");

	if (!nickname.empty() && !character.empty())
	{
		CTcpPacket packet(EFireNetTcpPacketType::Query);
		packet.WriteQuery(EFireNetTcpQuery::CreateProfile);
		packet.WriteString(nickname.c_str());
		packet.WriteString(character.c_str());

		SEND_PACKET(packet);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE  "Can't create new profile. Nickname or character model empty");
	}
}

void CFireNetCorePlugin::GetProfile(int uid)
{
	CryLog(TITLE "Try get profile");

	CTcpPacket packet(EFireNetTcpPacketType::Query);
	packet.WriteQuery(EFireNetTcpQuery::GetProfile);

	SEND_PACKET(packet);
}

SFireNetProfile * CFireNetCorePlugin::GetLocalProfile()
{
	// TODO
	return nullptr;
}

void CFireNetCorePlugin::GetShop()
{
	CryLog(TITLE "Try get shop");

	CTcpPacket packet(EFireNetTcpPacketType::Query);
	packet.WriteQuery(EFireNetTcpQuery::GetShop);

	SEND_PACKET(packet);
}

void CFireNetCorePlugin::BuyItem(const std::string & item)
{
	CryLog(TITLE "Try buy item by name");

	if (!item.empty())
	{
		CTcpPacket packet(EFireNetTcpPacketType::Query);
		packet.WriteQuery(EFireNetTcpQuery::BuyItem);
		packet.WriteString(item.c_str());

		SEND_PACKET(packet);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE  "Can't buy item. Item name empty");
	}
}

void CFireNetCorePlugin::RemoveItem(const std::string & item)
{
	CryLog(TITLE "Try remove item by name");

	if (!item.empty())
	{
		CTcpPacket packet(EFireNetTcpPacketType::Query);
		packet.WriteQuery(EFireNetTcpQuery::RemoveItem);
		packet.WriteString(item.c_str());

		SEND_PACKET(packet);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE  "Can't remove item. Item name empty");
	}
}

void CFireNetCorePlugin::SendInvite(EFireNetInviteType type, int uid)
{
	CryLog(TITLE "Try send invite");

	if (uid > 0)
	{
		CTcpPacket packet(EFireNetTcpPacketType::Query);
		packet.WriteQuery(EFireNetTcpQuery::SendInvite);
		packet.WriteInt(type);
		packet.WriteInt(uid);

		SEND_PACKET(packet);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE  "Can't send invite. Wrong uid");
	}
}

void CFireNetCorePlugin::DeclineInvite()
{
	CryLog(TITLE "Try decline invite");

	CTcpPacket packet(EFireNetTcpPacketType::Query);
	packet.WriteQuery(EFireNetTcpQuery::DeclineInvite);

	SEND_PACKET(packet);
}

void CFireNetCorePlugin::AcceptInvite()
{
	CryLog(TITLE "Try accept invite");

	CTcpPacket packet(EFireNetTcpPacketType::Query);
	packet.WriteQuery(EFireNetTcpQuery::AcceptInvite);

	SEND_PACKET(packet);
}

void CFireNetCorePlugin::RemoveFriend(int uid)
{
	CryLog(TITLE "Try remove friend");

	if (uid > 0)
	{
		CTcpPacket packet(EFireNetTcpPacketType::Query);
		packet.WriteQuery(EFireNetTcpQuery::RemoveFriend);
		packet.WriteInt(uid);

		SEND_PACKET(packet);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE  "Can't remove friend. Wrong uid");
	}
}

void CFireNetCorePlugin::SendChatMessage(EFireNetChatMsgType type, int uid)
{
	CryLog(TITLE "Try send chat message");

	CTcpPacket packet(EFireNetTcpPacketType::Query);
	packet.WriteQuery(EFireNetTcpQuery::SendChatMsg);
	packet.WriteInt(type);

	if (type == EMsg_PrivateChat)
	{
		packet.WriteInt(uid);
	}

	SEND_PACKET(packet);
}

void CFireNetCorePlugin::GetGameServer(const std::string & map, const std::string & gamerules)
{
	CryLog(TITLE "Try get game server");

	if (!map.empty() && !gamerules.empty())
	{
		CTcpPacket packet(EFireNetTcpPacketType::Query);
		packet.WriteQuery(EFireNetTcpQuery::GetServer);
		packet.WriteString(map.c_str());
		packet.WriteString(gamerules.c_str());

		SEND_PACKET(packet);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE  "Can't get game server. Map or gamerules empty");
	}
}

void CFireNetCorePlugin::SendRawRequestToMasterServer(CTcpPacket &packet)
{
	SEND_PACKET(packet);
}

void CFireNetCorePlugin::RegisterFireNetListener(IFireNetListener * listener)
{
	if (listener == nullptr)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE "Failed register FireNetEventListener. Null pointer");
		return;
	}

	for (auto it = mEnv->m_Listeners.begin(); it != mEnv->m_Listeners.end(); ++it)
	{
		if ((*it) == listener)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, TITLE "Failed register FireNetEventListener. Listener alredy registered");
			return;
		}
	}

	mEnv->m_Listeners.push_back(listener);
	CryLog(TITLE "FireNet event listener registered");
}

void CFireNetCorePlugin::SendFireNetEvent(EFireNetEvents event, SFireNetEventArgs & args)
{
	mEnv->SendFireNetEvent(event, args);
}

CRYREGISTER_SINGLETON_CLASS(CFireNetCorePlugin)