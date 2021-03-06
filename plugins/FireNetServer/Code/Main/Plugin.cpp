// Copyright (C) 2014-2017 Ilya Chernetsov. All rights reserved. Contacts: <chernecoff@gmail.com>
// License: https://github.com/afrostalin/FireNET/blob/master/LICENSE

#include "StdAfx.h"
#include "Plugin.h"

#include "Network/UdpServer.h"
#include "Network/NetworkThread.h"
#include "Network/SyncGameState.h"

#include <CryCore/Platform/platform_impl.inl>
#include <CryExtension/ICryPluginManager.h>

#include <FireNet.inl>

IEntityRegistrator *IEntityRegistrator::g_pFirst = nullptr;
IEntityRegistrator *IEntityRegistrator::g_pLast = nullptr;

CFireNetServerPlugin::~CFireNetServerPlugin()
{
	// Unregister entities
	IEntityRegistrator* pTemp = IEntityRegistrator::g_pFirst;
	while (pTemp != nullptr)
	{
		pTemp->Unregister();
		pTemp = pTemp->m_pNext;
	}

	// Stop and delete network thread
	if (mEnv->pNetworkThread)
	{
		mEnv->pNetworkThread->SignalStopWork();
		gEnv->pThreadManager->JoinThread(mEnv->pNetworkThread, eJM_Join);
	}

	SAFE_DELETE(mEnv->pNetworkThread);

	// Unregister listeners
	if (gEnv && gEnv->pSystem)
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	// Clear FireNet server pointer
	if (gFireNet)
		gFireNet->pServer = nullptr;

	CryLogAlways(TITLE "Unloaded.");
}

bool CFireNetServerPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	if (initParams.bEditor)
		gEnv->SetIsEditor(true);

	if (initParams.bDedicatedServer)
		gEnv->SetIsDedicated(true);

	if(!gEnv->IsDedicated())
		CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_ERROR, TITLE "Can't init CryFireNetServer.dll - Client/Editor not support server library!");
	else
	{
		//! Init FireNet client pointer
		if (auto pPluginManager = gEnv->pSystem->GetIPluginManager())
		{
			if (auto pPlugin = pPluginManager->QueryPlugin<IFireNetCorePlugin>())
			{
				if (gFireNet = pPlugin->GetFireNetEnv())
				{
					gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this);

					ICryPlugin::SetUpdateFlags(EUpdateType_Update);

					gFireNet->pServer = dynamic_cast<IFireNetServerCore*>(this);

					//! Register FireNet listener
					if (gFireNet->pCore)
						gFireNet->pCore->RegisterFireNetListener(this);
				}
				else
					CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_ERROR, TITLE "Error init FireNet - Can't get FireNet environment pointer!");
			}
			else
				CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_ERROR, TITLE "Error init FireNet - Can't get Plugin!");
		}
		else
			CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_ERROR, TITLE "Error init FireNet - Can't get factory!");
	}

	return true;
}

void CFireNetServerPlugin::OnPluginUpdate(EPluginUpdateType updateType)
{
	switch (updateType)
	{
	case IPluginUpdateListener::EUpdateType_Update:
	{
		//! Update UDP server here
		if (mEnv->pNetworkThread && mEnv->pUdpServer)
		{
			mEnv->pUdpServer->Update();
		}
		//! Automatic deleting network thread if it's ready to close
		if (mEnv->pNetworkThread && mEnv->pNetworkThread->IsReadyToClose())
		{
			SAFE_DELETE(mEnv->pNetworkThread);
		}
		break;
	}
	default:
		break;
	}
}

void CFireNetServerPlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_GAME_POST_INIT:
	{
		//! Register entities
		IEntityRegistrator* pTemp = IEntityRegistrator::g_pFirst;
		while (pTemp != nullptr)
		{
			pTemp->Register();
			pTemp = pTemp->m_pNext;
		}

		//! Start network thread
		mEnv->pNetworkThread = new CNetworkThread();
		if (!gEnv->pThreadManager->SpawnThread(mEnv->pNetworkThread, "FireNetServer_Thread"))
		{
			SAFE_DELETE(mEnv->pNetworkThread);

			CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_ERROR, TITLE  "Can't spawn FireNet server thread!");
		}
		else
			CryLog(TITLE "FireNet server thread spawned");		

		break;
	}
	case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
	{
		if (mEnv->pGameSync && wparam == 0)
			mEnv->pGameSync->Reset();
		break;
	}
	case ESYSTEM_EVENT_FULL_SHUTDOWN:
	{
		Quit();
		break;
	}
	case ESYSTEM_EVENT_FAST_SHUTDOWN:
	{
		Quit();
		break;
	}
	case ESYSTEM_EVENT_LEVEL_LOAD_START:
	{
		if (mEnv->pUdpServer)
			mEnv->pUdpServer->SetServerStatus(EFireNetUdpServerStatus::LevelLoading);
		break;
	}
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
	{
		if (mEnv->pUdpServer)
			mEnv->pUdpServer->SetServerStatus(EFireNetUdpServerStatus::LevelLoaded);
		break;
	}
	case ESYSTEM_EVENT_LEVEL_LOAD_ERROR:
	{
		if (mEnv->pUdpServer)
			mEnv->pUdpServer->SetServerStatus(EFireNetUdpServerStatus::LevelLoadError);
		break;
	}
	case ESYSTEM_EVENT_LEVEL_GAMEPLAY_START:
	{
		if (mEnv->pUdpServer)
			mEnv->pUdpServer->SetServerStatus(EFireNetUdpServerStatus::GameStart);
		break;
	}
	case ESYSTEM_EVENT_LEVEL_UNLOAD:
	{
		if (mEnv->pUdpServer)
			mEnv->pUdpServer->SetServerStatus(EFireNetUdpServerStatus::LevelUnloading);
		break;
	}
	case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
	{
		if (mEnv->pUdpServer)
			mEnv->pUdpServer->SetServerStatus(EFireNetUdpServerStatus::LevelUnloaded);
		break;
	}
	break;
	}
}

void CFireNetServerPlugin::RegisterGameServer()
{
}

void CFireNetServerPlugin::UpdateGameServerInfo()
{
}

EFireNetUdpServerStatus CFireNetServerPlugin::GetServerStatus()
{
	return mEnv->pUdpServer ? mEnv->pUdpServer->GetServerStatus() : EFireNetUdpServerStatus::None;
}

bool CFireNetServerPlugin::Quit()
{
	CryLogAlways(TITLE "Closing CryFireNetServer plugin...");

	if (mEnv->pNetworkThread)
	{
		mEnv->pNetworkThread->SignalStopWork();
		gEnv->pThreadManager->JoinThread(mEnv->pNetworkThread, eJM_Join);
	}

	SAFE_DELETE(mEnv->pNetworkThread);

	if (!mEnv->pNetworkThread)
	{
		CryLogAlways(TITLE "CryFireNetServer plugin ready to unload");
		return true;
	}
	else
		CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_ERROR, TITLE  "Can't normaly close plugin - network thread not deleted!");

	return false;
}

void CFireNetServerPlugin::OnFireNetEvent(EFireNetEvents event, SFireNetEventArgs & args)
{
	switch (event)
	{
	case FIRENET_EVENT_MASTER_SERVER_CONNECTED:
	{
		//! Load level if it set in config file
		auto pLevel = gEnv->pConsole->GetCVar("firenet_game_server_map");
		if (pLevel)
		{
			string m_Level = pLevel->GetString();

			if (!m_Level.IsEmpty())
				gEnv->pConsole->ExecuteString(string().Format("map %s s", m_Level), true, true);
			else
				CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_WARNING, TITLE  "Can't load level - level empty");
		}

		break;
	}
	default:
		break;
	}
}

CRYREGISTER_SINGLETON_CLASS(CFireNetServerPlugin)