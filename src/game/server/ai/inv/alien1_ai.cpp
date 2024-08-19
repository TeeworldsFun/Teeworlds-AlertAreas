/*#include <engine/shared/config.h>

#include <game/server/ai.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>

#include "alien1_ai.h"


CAIalien1::CAIalien1(CGameContext *pGameServer, CPlayer *pPlayer, int Level)
: CAI(pGameServer, pPlayer)
{
	m_SkipMoveUpdate = 0;
	m_StartPos = vec2(0, 0);
	m_ShockTimer = 0;
	m_Triggered = false;
	m_TriggerLevel = 5 + rand()%6;
	
	m_Level = Level;
	
	m_Skin = SKIN_ALIEN1+min(Level, 3);

	Player()->SetCustomSkin(m_Skin);
}


void CAIalien1::OnCharacterSpawn(CCharacter *pChr)
{
	CAI::OnCharacterSpawn(pChr);
	
	m_WaypointDir = vec2(0, 0);
	//Player()->SetRandomSkin();
	
	m_PowerLevel = 2;
	
	m_StartPos = Player()->GetCharacter()->m_Pos;
	m_TargetPos = Player()->GetCharacter()->m_Pos;
	
	if (frandom() < 0.4f)
		pChr->GetPlayer()->IncreaseGold(frandom()*4);
	
	if (m_Skin == SKIN_ALIEN3)
	{
		pChr->GiveCustomWeapon(GRENADE_GRENADE, 20);
		pChr->SetHealth(60+min((m_Level-1)*4, 300));
		pChr->SetArmor(60+min((m_Level-1)*4, 300));
		m_PowerLevel = 8;
		m_TriggerLevel = 15 + rand()%5;
	}
	else if (m_Skin == SKIN_ALIEN4)
	{
		pChr->GiveCustomWeapon(GUN_TASER);
		pChr->SetHealth(60+min((m_Level-1)*4, 200));
		pChr->SetArmor(60+min((m_Level-1)*4, 350));
		m_PowerLevel = 12;
		m_TriggerLevel = 15 + rand()%5;
	}
	else if (m_Skin == SKIN_ALIEN5)
	{
		pChr->GiveCustomWeapon(GRENADE_DOOMLAUNCHER);
		pChr->SetHealth(50+min((m_Level-1)*4, 150));
		pChr->SetArmor(60+min((m_Level-1)*4, 300));
		m_PowerLevel = 10;
		m_TriggerLevel = 15 + rand()%5;
	}
	else if (m_Skin == SKIN_ALIEN2)
	{
		pChr->GiveCustomWeapon(HAMMER_THUNDER);
		pChr->SetHealth(60+min((m_Level-1)*4, 300));
		pChr->SetArmor(60+min((m_Level-1)*4, 300));
		m_PowerLevel = 8;
		m_TriggerLevel = 15 + rand()%5;
	}
	else
	{
		if (frandom() < min(m_Level*0.1f, 1.0f))
			pChr->GiveCustomWeapon(GUN_TASER);
		else if (frandom() < min(m_Level*0.1f, 1.0f))
			pChr->GiveCustomWeapon(GUN_UZI);
		
		if (frandom() < 0.6f)
			pChr->GiveCustomWeapon(GUN_PISTOL);
		else
			pChr->GiveCustomWeapon(GUN_MAGNUM);
		
		pChr->SetHealth(60+min((m_Level-1)*3, 300));
	}
	
	m_ShockTimer = 10;
		
	if (!m_Triggered)
		m_ReactionTime = 100;
}


void CAIalien1::ReceiveDamage(int CID, int Dmg)
{
	if (CID >= 0 && frandom() < Dmg*0.02f)
		m_Triggered = true;

	if (frandom() < Dmg*0.03f)
		m_ShockTimer = 2 + Dmg/2;
	
	if (m_PowerLevel < 10)
		m_Attack = 0;
	
	if (m_AttackOnDamage)
	{
		m_Attack = 1;
		m_InputChanged = true;
		m_AttackOnDamageTick = GameServer()->Server()->Tick() + GameServer()->Server()->TickSpeed();
	}
}


void CAIalien1::DoBehavior()
{
	m_Attack = 0;
	
	if (m_ShockTimer > 0 && m_ShockTimer--)
	{
		m_ReactionTime = 1 + frandom()*3;
		return;
	}
	
	HeadToMovingDirection();
	SeekClosestEnemyInSight();
	bool Shooting = false;
	
	// if we see a player
	if (m_EnemiesInSight > 0)
	{
		ReactToPlayer();
		//m_Triggered = true;
		
		if (!m_MoveReactTime)
			m_MoveReactTime++;
		
		if (ShootAtClosestEnemy())
		{
			Shooting = true;
			
			if (WeaponShootRange() - m_PlayerDistance > 200)
			{
				m_TargetPos = normalize(m_Pos - m_PlayerPos) * WeaponShootRange();
				GameServer()->Collision()->IntersectLine(m_Pos, m_TargetPos, 0x0, &m_TargetPos);
				//MoveTowardsWaypoint(true);
				//
				
				int Weapon = Player()->GetCharacter()->GetActiveWeapon();
				if (Weapon == WEAPON_HAMMER)
				{
					Shooting = false;
					m_TargetPos = m_Pos - m_PlayerDirection*3;
				}
			}
		}
		else
		{
			if (SeekClosestEnemy())
			{
				m_TargetPos = m_PlayerPos;
				
				//if (WeaponShootRange() - m_PlayerDistance > 200)
				//	SeekRandomWaypoint();
			}
		}
	}
	else if (!m_Triggered)
	{
		m_TargetPos = m_StartPos;
	}
	else
	{
		// triggered, but no enemies in sight
		ShootAtClosestBuilding();
		ShootAtBlocks();
		
		if (SeekClosestEnemy())
			m_TargetPos = m_PlayerPos;
	}

	if ((Shooting && Player()->GetCharacter()->IsGrounded()) || (abs(m_Pos.x - m_TargetPos.x) < 40 && abs(m_Pos.y - m_TargetPos.y) < 40))
	{
		// stand still
		m_Move = 0;
		m_Jump = 0;
		m_Hook = 0;
	}
	else
	{
		if (!m_MoveReactTime || m_MoveReactTime++ > 9)
		{
			if (UpdateWaypoint())
			{
				MoveTowardsWaypoint();
			}
			else
			{
				m_WaypointPos = m_TargetPos;
				MoveTowardsWaypoint(true);
			}
		}
	}
	
	Player()->GetCharacter()->m_SkipPickups = 999;
	RandomlyStopShooting();
	
	if (m_AttackOnDamageTick > GameServer()->Server()->Tick())
		m_Attack = 1;
	
	// next reaction in
	m_ReactionTime = 1 + rand()%3;
}
*/