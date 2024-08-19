#ifndef GAME_SERVER_AI_INV_AI_H
#define GAME_SERVER_AI_INV_AI_H
#include <game/server/ai.h>
#include <game/server/gamecontext.h>

class CAIinvasion : public CAI
{
public:
	CAIinvasion(CGameContext *pGameServer, CPlayer *pPlayer, int Level, int Group);

	virtual void DoBehavior();
	void OnCharacterSpawn(class CCharacter *pChr);
	void ReceiveDamage(int CID, int Dmg);

	void GiveCustomWeapon(CCharacter *pChr, int W);

private:
	int m_SkipMoveUpdate;
	vec2 m_StartPos;
	
	int m_EType;
	int m_Level;

	int m_MoveReactTime;

	bool m_AttackOnDamage;
	int m_AttackOnDamageTick;

	int m_Group;
};

#endif
