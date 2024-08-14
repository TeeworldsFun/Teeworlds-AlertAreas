#include <engine/shared/config.h>

#include <game/server/ai.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>

#include <base/math.h>
#include "inv_ai.h"

CAIinvasion::CAIinvasion(CGameContext *pGameServer, CPlayer *pPlayer, int Level, int Group)
	: CAI(pGameServer, pPlayer)
{
	m_SkipMoveUpdate = 0;
	m_StartPos = vec2(0, 0);
	m_Triggered = false;
	m_TriggerLevel = 5 + rand() % 6;

	m_Level = Level;
	m_EType = SKIN_ALIEN1 + min(m_Level, (int)NUM_SKINS);

	m_AttackOnDamage = false;
	m_AttackOnDamageTick = 0;

	m_MoveReactTime = 0;

	m_Group = Group;

	pPlayer->SetRandomSkin();
}

void CAIinvasion::GiveWeapon(CCharacter *pChr, int W)
{
	pChr->GiveCustomWeapon(W);
	pChr->SetCustomWeapon(W);
}

void CAIinvasion::OnCharacterSpawn(CCharacter *pChr)
{
	CAI::OnCharacterSpawn(pChr);

	m_WaypointDir = vec2(0, 0);
	// Player()->SetRandomSkin();

	m_PowerLevel = 10 + m_Level * 2;

	m_StartPos = Player()->GetCharacter()->m_Pos;
	m_TargetPos = Player()->GetCharacter()->m_Pos;

	switch (m_EType)
	{
	case SKIN_ALIEN1:
		if (frandom() < min(m_Level * 0.1f, 1.0f))
			GiveWeapon(pChr, GUN_PISTOL);
		else if (frandom() < min(m_Level * 0.1f, 1.0f))
			GiveWeapon(pChr, GUN_UZI);

		if (frandom() < 0.6f)
			GiveWeapon(pChr, GUN_MAGNUM);
		else
			GiveWeapon(pChr, GUN_TASER);

		pChr->SetHealth(60 + min((m_Level - 1) * 3, 300));
		break;

	case SKIN_ALIEN2:
		GiveWeapon(pChr, SHOTGUN_DOUBLEBARREL);
		pChr->SetHealth(60 + min((m_Level - 1) * 4, 300));
		pChr->SetArmor(60 + min((m_Level - 1) * 4, 300));
		m_TriggerLevel = 15 + rand() % 5;
		break;

	case SKIN_ALIEN3:
		GiveWeapon(pChr, GRENADE_GRENADELAUNCHER);
		pChr->SetHealth(60 + min((m_Level - 1) * 4, 300));
		pChr->SetArmor(60 + min((m_Level - 1) * 4, 300));
		m_TriggerLevel = 15 + rand() % 5;
		break;

	case SKIN_ALIEN4:
		GiveWeapon(pChr, GUN_MAGNUM);
		pChr->SetHealth(60 + min((m_Level - 1) * 4, 200));
		pChr->SetArmor(60 + min((m_Level - 1) * 4, 350));
		m_TriggerLevel = 15 + rand() % 5;
		break;

	case SKIN_ALIEN5:
		GiveWeapon(pChr, GRENADE_DOOMLAUNCHER);
		pChr->SetHealth(50 + min((m_Level - 1) * 4, 150));
		pChr->SetArmor(60 + min((m_Level - 1) * 4, 300));
		m_TriggerLevel = 15 + rand() % 5;
		break;

	case SKIN_BUNNY1:
		if (frandom() < 0.5f)
			GiveWeapon(pChr, GRENADE_GRENADELAUNCHER);
		else
			GiveWeapon(pChr, GRENADE_DOOMLAUNCHER);

		pChr->SetHealth(40 + min(m_Level * 3.0f, 220.0f));
		break;

	case SKIN_BUNNY2:
		if (frandom() < 0.5f)
			GiveWeapon(pChr, GRENADE_GRENADELAUNCHER);
		else
			GiveWeapon(pChr, RIFLE_DOOMRAY);

		GiveWeapon(pChr, GRENADE_ELECTROLAUNCHER);

		pChr->SetHealth(80 + min(m_Level * 4.0f, 320.0f));
		m_TriggerLevel = 15 + rand() % 5;
		break;

	case SKIN_BUNNY3:
		if (frandom() < 0.5f)
			GiveWeapon(pChr, GRENADE_GRENADELAUNCHER);
		else
			GiveWeapon(pChr, RIFLE_HEAVYRIFLE);

		pChr->SetHealth(80 + min(m_Level * 5.0f, 320.0f));
		m_TriggerLevel = 15 + rand() % 5;
		break;

	case SKIN_BUNNY4:
		if (frandom() < 0.5f)
			GiveWeapon(pChr, SWORD_KATANA);
		else
			GiveWeapon(pChr, SWORD_LIGHTNING);

		m_AttackOnDamage = true;

		pChr->SetHealth(80 + min(m_Level * 5.0f, 320.0f));
		m_TriggerLevel = 15 + rand() % 5;
		break;

	case SKIN_FOXY1:
		GiveWeapon(pChr, GUN_TASER);

		pChr->SetHealth(80 + min(m_Level * 5.0f, 320.0f));
		m_TriggerLevel = 15 + rand() % 5;
		break;

	case SKIN_PYRO1:
		if (frandom() < 0.5f)
			GiveWeapon(pChr, SWORD_LIGHTNING);
		else
			GiveWeapon(pChr, GUN_TASER);
		break;
	case SKIN_PYRO2:
		if (frandom() < 0.35f)
			GiveWeapon(pChr, HAMMER_THUNDER);
		else
			GiveWeapon(pChr, SHOTGUN_DOUBLEBARREL);
		break;

	case SKIN_PYRO3:
		if (frandom() < 0.35f)
			GiveWeapon(pChr, RIFLE_DOOMRAY);
		else
		{
			GiveWeapon(pChr, SHOTGUN_DOUBLEBARREL);
			m_AttackOnDamage = true;
		}
		break;

	case SKIN_ROBO1:
		pChr->SetHealth(50 + min(m_Level * 3.0f, 100.0f));
		pChr->SetArmor(50 + min(m_Level * 3.0f, 100.0f));
		GiveWeapon(pChr, GRENADE_ELECTROLAUNCHER);
		break;

	case SKIN_ROBO2:
		pChr->SetHealth(100 + min(m_Level * 3.0f, 200.0f));
		pChr->SetArmor(100 + min(m_Level * 3.0f, 200.0f));
		GiveWeapon(pChr, SWORD_KATANA);
		break;

	case SKIN_ROBO3:
		pChr->SetHealth(100 + min(m_Level * 4.0f, 200.0f));
		pChr->SetArmor(100 + min(m_Level * 4.0f, 300.0f));
		GiveWeapon(pChr, RIFLE_STORMRIFLE);
		GiveWeapon(pChr, GRENADE_DOOMLAUNCHER);
		pChr->m_Kits = 1;
		break;

	case SKIN_ROBO4:
		pChr->SetHealth(150 + min(m_Level * 5.0f, 200.0f));
		pChr->SetArmor(150 + min(m_Level * 5.0f, 300.0f));
		GiveWeapon(pChr, GRENADE_ELECTROLAUNCHER);
		GiveWeapon(pChr, GRENADE_DOOMLAUNCHER);
		pChr->m_Kits = 1;
		break;

	case SKIN_ROBO5:
		pChr->SetHealth(150 + min(m_Level * 4.0f, 200.0f));
		pChr->SetArmor(150 + min(m_Level * 4.0f, 300.0f));
		GiveWeapon(pChr, SWORD_LIGHTNING);
		m_AttackOnDamage = true;
		break;

	case SKIN_SKELETON1:
		GiveWeapon(pChr, GRENADE_ELECTROLAUNCHER);
		break;

	case SKIN_SKELETON2:
		if (frandom() < 0.5f)
			GiveWeapon(pChr, SWORD_KATANA);
		else
			GiveWeapon(pChr, SWORD_LIGHTNING);
		break;

	case SKIN_SKELETON3:
		if (frandom() < 0.5f)
			GiveWeapon(pChr, GUN_UZI);
		else
			GiveWeapon(pChr, GRENADE_DOOMLAUNCHER);
		break;

	default:
		break;
	}

	if (!m_Triggered)
		m_ReactionTime = 100;
}

void CAIinvasion::ReceiveDamage(int CID, int Dmg)
{
	if (CID >= 0 && frandom() < Dmg * 0.02f)
		m_Triggered = true;

	if (m_AttackOnDamage)
	{
		m_Attack = 1;
		m_InputChanged = true;
		m_AttackOnDamageTick = GameServer()->Server()->Tick() + GameServer()->Server()->TickSpeed();
	}
}

void CAIinvasion::DoBehavior()
{
	// reset jump and attack
	m_Jump = 0;
	m_Attack = 0;

	
	HeadToMovingDirection();

	SeekClosestEnemyInSight();

	// if we see a player
	if (m_EnemiesInSight > 0)
	{
		ShootAtClosestEnemy();
		ReactToPlayer();
	}
	else
		m_AttackTimer = 0;


	//if (SeekClosestEnemy())
	if (SeekRandomEnemy())
	{
		m_TargetPos = m_PlayerPos;
				
		if (m_EnemiesInSight > 1)
		{
			// distance to the player
			if (m_PlayerPos.x < m_Pos.x)
				m_TargetPos.x = m_PlayerPos.x + WeaponShootRange()/2*(0.5f+frandom()*1.0f);
			else
				m_TargetPos.x = m_PlayerPos.x - WeaponShootRange()/2*(0.5f+frandom()*1.0f);
		}
	}

	
	if (UpdateWaypoint())
	{
		MoveTowardsWaypoint(20);
		HookMove();
		AirJump();
		
		// jump if waypoint is above us
		if (abs(m_WaypointPos.x - m_Pos.x) < 60 && m_WaypointPos.y < m_Pos.y - 100 && frandom()*20 < 4)
			m_Jump = 1;
	}
	else
	{
		m_Hook = 0;
	}
	
	
	DoJumping();
	Unstuck();

	RandomlyStopShooting();
	
	// next reaction in
	m_ReactionTime = 2 + frandom()*4;
	
}