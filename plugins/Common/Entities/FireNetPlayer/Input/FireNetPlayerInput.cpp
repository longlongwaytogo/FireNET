// Copyright (C) 2016-2017 Ilya Chernetsov. All rights reserved. Contacts: <chernecoff@gmail.com>
// License: https://github.com/afrostalin/EasyShooter/blob/master/LICENCE.md

#include "StdAfx.h"
#include "FireNetPlayerInput.h"
#include "Entities/FireNetPlayer/FireNetPlayer.h"
#include "Entities/FireNetPlayer/Movement/FireNetPlayerMovement.h"
#include "Entities/ISimpleWeapon.h"

#include <CryAnimation/ICryAnimation.h>

CFireNetPlayerInput::CFireNetPlayerInput() : m_pPlayer(nullptr)
	, bGamePaused(false)
	, bPhysDebug(false)
{
	m_inputFlags = 0;
	m_inputValues = 0.f;
	m_moveSpeed = 0.f;
}

void CFireNetPlayerInput::PostInit(IGameObject *pGameObject)
{
	m_pPlayer = static_cast<CFireNetPlayer *>(pGameObject->QueryExtension("FireNetPlayer"));
	m_moveSpeed = m_pPlayer->GetCVars().m_moveSpeed;

	GetGameObject()->EnableUpdateSlot(this, 0);
}

void CFireNetPlayerInput::Update(SEntityUpdateContext &ctx, int updateSlot)
{
	if (m_pPlayer->IsLocalPlayer())
	{
		Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));

		ypr.x += m_mouseDeltaRotation.x * m_pPlayer->GetCVars().m_rotationSpeedYaw * ctx.fFrameTime;
		ypr.y = CLAMP(ypr.y + m_mouseDeltaRotation.y * m_pPlayer->GetCVars().m_rotationSpeedPitch * ctx.fFrameTime, m_pPlayer->GetCVars().m_rotationLimitsMinPitch, m_pPlayer->GetCVars().m_rotationLimitsMaxPitch);
		ypr.z = 0;

		m_lookOrientation = Quat(CCamera::CreateOrientationYPR(ypr));
		m_mouseDeltaRotation = ZERO;
	}

	if (auto pMovement = m_pPlayer->GetMovement())
	{
		pMovement->SetSprint(0.f);
	}
}

void CFireNetPlayerInput::OnPlayerRespawn()
{
	m_inputFlags = 0;
	m_mouseDeltaRotation = ZERO;
	m_lookOrientation = IDENTITY;
}

void CFireNetPlayerInput::HandleInputFlagChange(EFireNetClientInputFlags flags, int activationMode, EInputFlagType type)
{
	switch (type)
	{
		case eInputFlagType_Hold:
		{
			if (activationMode == eIS_Released)
			{
				m_inputFlags &= ~flags;
			}
			else
			{
				m_inputFlags |= flags;
			}
		}
		break;
		case eInputFlagType_Toggle:
		{
			if (activationMode == eIS_Released)
			{
				m_inputFlags ^= flags;
			}
		}
		break;
	}

	//! Send input to server if input changed
	if (m_pPlayer->IsLocalPlayer())
	{
		if (!gEnv->IsDedicated() && !gEnv->IsEditor() && gFireNet && gFireNet->pClient && gFireNet->pClient->IsConnected())
		{
			SFireNetClientInput input;
			input.m_flags = static_cast<EFireNetClientInputFlags>(m_inputFlags);
			input.m_LookOrientation = m_lookOrientation;

			if (m_LastClientInputFlags != input.m_flags || m_LastClientLookOrientation != input.m_LookOrientation)
			{
				gFireNet->pClient->SendUpdateInputRequest(input);
			}		
		}
	}
}

void CFireNetPlayerInput::SyncInput(const SFireNetClientInput & input)
{
	//! Sync flags
	m_inputFlags = input.m_flags;
	m_lookOrientation = input.m_LookOrientation;

	//! Sync jump
	if (m_inputFlags & E_FIRENET_INPUT_JUMP)
	{
		DoJump();
	}
	//! Sync sprint
	if (m_inputFlags & E_FIRENET_INPUT_SPRINT)
	{
		DoSprint();
	}
	//! Sync shoot
	if (m_inputFlags & E_FIRENET_INPUT_SHOOT)
	{
		DoShoot();
	}
}

void CFireNetPlayerInput::DoJump()
{
	auto pMovement = m_pPlayer->GetMovement();
	auto pEntity = m_pPlayer->GetEntity();
	auto pPhysics = GetEntity()->GetPhysics();

	if (pPhysics && pEntity &&  pMovement->IsOnGround())
	{
		pe_action_impulse impulseAction;
		Vec3 impulsePos = pEntity->GetWorldPos();
		impulsePos.x = 0;
		impulsePos.y = 0;
		impulsePos.z = impulsePos.z + 400 * m_pPlayer->GetCVars().m_jumpHeightMultiplier;
		impulseAction.impulse = impulsePos;

		pPhysics->Action(&impulseAction);
	}
}

void CFireNetPlayerInput::DoSprint()
{
	if (auto pMovement = m_pPlayer->GetMovement())
	{
		pMovement->SetSprint(30.f);
	}
}

void CFireNetPlayerInput::DoMouseYaw(float value)
{
	m_mouseDeltaRotation.x -= value;
}

void CFireNetPlayerInput::DoMousePitch(float value)
{
	m_mouseDeltaRotation.y -= value;
}

void CFireNetPlayerInput::DoShoot()
{
	auto pWeapon = m_pPlayer->GetCurrentWeapon();
	auto pCharacter = GetEntity()->GetCharacter(CFireNetPlayer::eGeometry_Default);

	if (pWeapon != nullptr && pCharacter != nullptr)
	{
		auto pBarrelOutAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName("barrel_out");

		if (pBarrelOutAttachment != nullptr)
		{
			QuatTS bulletOrigin = pBarrelOutAttachment->GetAttWorldAbsolute();

			pWeapon->RequestFire(bulletOrigin.t, bulletOrigin.q);
		}
	}
}

void CFireNetPlayerInput::StartActionCapture()
{
	if (!gEnv->IsDedicated())
	{
		IActionMapManager *pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();
		pActionMapManager->InitActionMaps("Libs/config/defaultprofile.xml");
		pActionMapManager->Enable(true);
		pActionMapManager->EnableActionMap("player", true);

		if (IActionMap *pActionMap = pActionMapManager->GetActionMap("player"))
		{
			pActionMap->SetActionListener(GetEntityId());
		}

		GetGameObject()->CaptureActions(this);
		GetGameObject()->EnableUpdateSlot(this, 0);

		InitializeActionHandler();
	}
}

void CFireNetPlayerInput::InitializeActionHandler()
{
	m_actionHandler.AddHandler(ActionId("moveleft"), &CFireNetPlayerInput::OnActionMoveLeft);
	m_actionHandler.AddHandler(ActionId("moveright"), &CFireNetPlayerInput::OnActionMoveRight);
	m_actionHandler.AddHandler(ActionId("moveforward"), &CFireNetPlayerInput::OnActionMoveForward);
	m_actionHandler.AddHandler(ActionId("moveback"), &CFireNetPlayerInput::OnActionMoveBack);

	// --------------
	m_actionHandler.AddHandler(ActionId("jump"), &CFireNetPlayerInput::OnActionJump);
	m_actionHandler.AddHandler(ActionId("sprint"), &CFireNetPlayerInput::OnActionSprint);
	m_actionHandler.AddHandler(ActionId("toggleTP"), &CFireNetPlayerInput::OnActionToggleThirdPersonMode);
	m_actionHandler.AddHandler(ActionId("gamePause"), &CFireNetPlayerInput::OnActionGamePaused);

	// Debug actions
	m_actionHandler.AddHandler(ActionId("phys_debug"), &CFireNetPlayerInput::OnActionPhysicDebug);
	// --------------

	m_actionHandler.AddHandler(ActionId("mouse_rotateyaw"), &CFireNetPlayerInput::OnActionMouseRotateYaw);
	m_actionHandler.AddHandler(ActionId("mouse_rotatepitch"), &CFireNetPlayerInput::OnActionMouseRotatePitch);

	m_actionHandler.AddHandler(ActionId("shoot"), &CFireNetPlayerInput::OnActionShoot);
}

void CFireNetPlayerInput::OnAction(const ActionId &action, int activationMode, float value)
{
	m_actionHandler.Dispatch(this, GetEntityId(), action, activationMode, value);
}

bool CFireNetPlayerInput::OnActionMoveLeft(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	HandleInputFlagChange(E_FIRENET_INPUT_MOVE_LEFT, activationMode);
	return true;
}

bool CFireNetPlayerInput::OnActionMoveRight(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	HandleInputFlagChange(E_FIRENET_INPUT_MOVE_RIGHT, activationMode);
	return true;
}

bool CFireNetPlayerInput::OnActionMoveForward(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	HandleInputFlagChange(E_FIRENET_INPUT_MOVE_FORWARD, activationMode);
	return true;
}

bool CFireNetPlayerInput::OnActionMoveBack(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	HandleInputFlagChange(E_FIRENET_INPUT_MOVE_BACK, activationMode);
	return true;
}

bool CFireNetPlayerInput::OnActionJump(EntityId entityId, const ActionId & actionId, int activationMode, float value)
{
	HandleInputFlagChange(E_FIRENET_INPUT_JUMP, activationMode);

	if (activationMode == eIS_Pressed)
	{
		DoJump();
	}
	return true;
}

bool CFireNetPlayerInput::OnActionSprint(EntityId entityId, const ActionId & actionId, int activationMode, float value)
{	
	HandleInputFlagChange(E_FIRENET_INPUT_SPRINT, activationMode);

	DoSprint();

	return true;
}

bool CFireNetPlayerInput::OnActionToggleThirdPersonMode(EntityId entityId, const ActionId & actionId, int activationMode, float value)
{
	if (activationMode == eIS_Released)
	{
		if (m_pPlayer->IsThirdPerson())
			m_pPlayer->SetThirdPerson(false);
		else
			m_pPlayer->SetThirdPerson(true);
	}

	return true;
}

bool CFireNetPlayerInput::OnActionGamePaused(EntityId entityId, const ActionId & actionId, int activationMode, float value)
{
	// Load InGame page if game paused and unload it when game resumed
	if (!gEnv->IsEditor() && activationMode == eIS_Released && !bGamePaused)
	{
		bGamePaused = true;

		// Block input
		IActionMapManager* pAMM = gEnv->pGameFramework->GetIActionMapManager();
		if (pAMM)
		{
			pAMM->EnableFilter("no_move", true);	
		}

		gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_GAME_PAUSED, 0, 0);
	}
	else if (!gEnv->IsEditor() && activationMode == eIS_Released && bGamePaused)
	{
		bGamePaused = false;

		// Unblock input
		IActionMapManager* pAMM = gEnv->pGameFramework->GetIActionMapManager();
		if (pAMM)
		{
			pAMM->EnableFilter("no_move", false);
		}

		gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_GAME_RESUMED, 0, 0);
	}

	return false;
}

// Debug functions
bool CFireNetPlayerInput::OnActionPhysicDebug(EntityId entityId, const ActionId & actionId, int activationMode, float value)
{
	if (activationMode == eIS_Released && !bPhysDebug)
	{
		CryLog("Physic debugging enabled");

		gEnv->pConsole->ExecuteString("p_draw_helpers 1");	
		bPhysDebug = true;
	}
	else if (activationMode == eIS_Released && bPhysDebug)
	{
		CryLog("Physic debugging disabled");

		gEnv->pConsole->ExecuteString("p_draw_helpers 0");
		bPhysDebug = false;
	}

	return false;
}
//

bool CFireNetPlayerInput::OnActionMouseRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	HandleInputFlagChange(E_FIRENET_INPUT_MOUSE_ROTATE_YAW, activationMode);
	m_inputValues = value;

	DoMouseYaw(value);

	return true;
}

bool CFireNetPlayerInput::OnActionMouseRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	HandleInputFlagChange(E_FIRENET_INPUT_MOUSE_ROTATE_PITCH, activationMode);
	m_inputValues = value;

	DoMousePitch(value);

	return true;
}

bool CFireNetPlayerInput::OnActionShoot(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	HandleInputFlagChange(E_FIRENET_INPUT_SHOOT, activationMode);

	if (activationMode == eIS_Down)
	{
		DoShoot();
	}

	return true;
}