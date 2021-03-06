// Copyright (C) 2014-2017 Ilya Chernetsov. All rights reserved. Contacts: <chernecoff@gmail.com>
// License: https://github.com/afrostalin/FireNET/blob/master/LICENSE

#pragma once

#include "Entities/ISimpleExtension.h"

#include <CrySystem/ICryPlugin.h>
#include <CryGame/IGameFramework.h>
#include <CryEntitySystem/IEntityClass.h>

#include <FireNet>

class CFireNetServerPlugin 
	: public ICryPlugin
	, public ISystemEventListener
	, public IFireNetServerCore
	, public IFireNetListener
{
public:
	CRYINTERFACE_SIMPLE(ICryPlugin)
	CRYGENERATE_SINGLETONCLASS(CFireNetServerPlugin, "FireNetServer_Plugin", 0x53E3A145AFE44782, 0xA6939B01846615AB)

	virtual ~CFireNetServerPlugin();
	
	// ICryPlugin
	virtual const char* GetName() const override { return "FireNet-Server"; }
	virtual const char* GetCategory() const override { return "Network"; }
	virtual bool        Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
	virtual void        OnPluginUpdate(EPluginUpdateType updateType) override;
	// ~ICryPlugin

	// ISystemEventListener
	virtual void        OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	// IFireNetServerCore
	virtual void                    RegisterGameServer() override;
	virtual void                    UpdateGameServerInfo() override;
	virtual EFireNetUdpServerStatus GetServerStatus() override;
	virtual bool                    Quit() override;
	// ~IFireNetServerCore

	//! IFireNetListener
	virtual void OnFireNetEvent(EFireNetEvents event, SFireNetEventArgs& args = SFireNetEventArgs()) override;
	//! IFireNetListener
public:
	template<class T>
	struct CObjectCreator : public IGameObjectExtensionCreatorBase
	{
		IGameObjectExtension* Create(IEntity* pEntity)
		{
			return pEntity->CreateComponentClass<T>();
		}

		void GetGameObjectExtensionRMIData(void** ppRMI, size_t* nCount)
		{
			*ppRMI = nullptr;
			*nCount = 0;
		}
	};

	template<class T>
	static void RegisterEntityWithDefaultComponent(const char *name, const char* category = nullptr, const char* icon = nullptr, bool iconOnTop = false)
	{
		IEntityClassRegistry::SEntityClassDesc clsDesc;
		clsDesc.sName = name;

		clsDesc.editorClassInfo.sCategory = category;
		clsDesc.editorClassInfo.sIcon = icon;
		clsDesc.editorClassInfo.bIconOnTop = iconOnTop;

		static CObjectCreator<T> _creator;

		gEnv->pGameFramework->GetIGameObjectSystem()->RegisterExtension(name, &_creator, &clsDesc);
	}

	template<class T>
	static void RegisterEntityComponent(const char *name)
	{
		static CObjectCreator<T> _creator;

		gEnv->pGameFramework->GetIGameObjectSystem()->RegisterExtension(name, &_creator, nullptr);
		T::SetExtensionId(gEnv->pGameFramework->GetIGameObjectSystem()->GetID(name));
	}
};

struct IEntityRegistrator
{
	IEntityRegistrator()
	{
		if (g_pFirst == nullptr)
		{
			g_pFirst = this;
			g_pLast = this;
		}
		else
		{
			g_pLast->m_pNext = this;
			g_pLast = g_pLast->m_pNext;
		}
	}

	virtual void Register() = 0;
	virtual void Unregister() = 0;

public:
	IEntityRegistrator *m_pNext;

	static IEntityRegistrator *g_pFirst;
	static IEntityRegistrator *g_pLast;
};