/*#ifndef GAME_SERVER_AI_PYRO1_AI_H
#define GAME_SERVER_AI_PYRO1_AI_H
#include <game/server/ai.h>
#include <game/server/gamecontext.h>

class CAIpyro1 : public CAI
{
public:
	CAIpyro1(CGameContext *pGameServer, CPlayer *pPlayer, int Level);

	virtual void DoBehavior();
	void OnCharacterSpawn(class CCharacter *pChr);
	void OnCharacterDeath();
	void ReceiveDamage(int CID, int Dmg);

private:
	int m_SkipMoveUpdate;
	vec2 m_StartPos;
	
	int m_ShockTimer;
	int m_Skin;
};

#endif
*/