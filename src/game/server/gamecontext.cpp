/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <cstring>
#include <new>
#include <base/math.h>
#include <engine/shared/config.h>
#include <engine/map.h>
#include <engine/console.h>
#include "gamecontext.h"
#include <game/version.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include "gamemodes/dm.h"
#include "gamemodes/tdm.h"
#include "gamemodes/ctf.h"
#include "gamemodes/dom.h"
#include "gamemodes/cstt.h"
#include "gamemodes/csbb.h"
#include "gamemodes/run.h"

#include <game/server/entities/arrow.h>
#include <game/server/entities/block.h>

#include <game/server/ai_protocol.h>
#include <game/server/ai.h>

#include <engine/shared/datafile.h> // MapGen
#include <game/server/playerdata.h>

#include <teeuniverses/components/localization.h>

const char *aClassName[NUM_CLASSES] =
	{
		"Soldier",
		"Medic",
		"Technician"};

enum
{
	RESET,
	NO_RESET
};

void CGameContext::Construct(int Resetting)
{
	m_Resetting = 0;
	m_pServer = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
		m_apPlayers[i] = 0;

	m_BroadcastLockTick = 0;

	m_pController = 0;
	m_VoteCloseTime = 0;
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;
	m_LockTeams = 0;

	m_aMostInterestingPlayer[0] = -1;
	m_aMostInterestingPlayer[1] = -1;

	m_ShowWaypoints = false;
	m_FreezeCharacters = false;

	if (Resetting == NO_RESET)
		m_pVoteOptionHeap = new CHeap();

	m_pBlockSolve = new CBlockSolve(this);
}

CGameContext::CGameContext(int Resetting)
{
	Construct(Resetting);
}

CGameContext::CGameContext()
{
	Construct(NO_RESET);
}

CGameContext::~CGameContext()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		delete m_apPlayers[i];
	if (!m_Resetting)
		delete m_pVoteOptionHeap;
}

void CGameContext::Clear()
{
	CHeap *pVoteOptionHeap = m_pVoteOptionHeap;
	CVoteOptionServer *pVoteOptionFirst = m_pVoteOptionFirst;
	CVoteOptionServer *pVoteOptionLast = m_pVoteOptionLast;
	int NumVoteOptions = m_NumVoteOptions;
	CTuningParams Tuning = m_Tuning;

	m_Resetting = true;
	this->~CGameContext();
	mem_zero(this, sizeof(*this));
	new (this) CGameContext(RESET);

	m_pVoteOptionHeap = pVoteOptionHeap;
	m_pVoteOptionFirst = pVoteOptionFirst;
	m_pVoteOptionLast = pVoteOptionLast;
	m_NumVoteOptions = NumVoteOptions;
	m_Tuning = Tuning;
}

class CCharacter *CGameContext::GetPlayerChar(int ClientID)
{
	if (ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return 0;
	return m_apPlayers[ClientID]->GetCharacter();
}

void CGameContext::CreateDamageInd(vec2 Pos, float Angle, int Amount)
{
	int a = Amount / 4;
	if (a == 0)
		a = 1;

	for (int i = 0; i < a; i++)
	{
		CNetEvent_DamageInd *pEvent = (CNetEvent_DamageInd *)m_Events.Create(NETEVENTTYPE_DAMAGEIND, sizeof(CNetEvent_DamageInd));
		if (pEvent)
		{
			pEvent->m_X = (int)Pos.x;
			pEvent->m_Y = (int)Pos.y;
			pEvent->m_Angle = (int)(Angle * 256.0f + frandom() * 200 - frandom() * 200);
		}
	}
}

void CGameContext::CreateHammerHit(vec2 Pos)
{
	// create the event
	CNetEvent_HammerHit *pEvent = (CNetEvent_HammerHit *)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(CNetEvent_HammerHit));
	if (pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::GenerateArrows()
{
	m_pArrow = new CArrow(&m_World);
}

bool CGameContext::GotAbility(int ClientID, int Ability)
{
	if (ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;

	if (m_apPlayers[ClientID] && m_apPlayers[ClientID]->GotAbility(Ability))
		return true;

	return false;
}

void CGameContext::CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, bool Superdamage)
{
	// create the event
	CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion));
	if (pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	if (!NoDamage)
	{
		// deal damage
		CCharacter *apEnts[MAX_CLIENTS];
		float Radius = 135.0f;
		float InnerRadius = 48.0f;
		int Num = m_World.FindEntities(Pos, Radius, (CEntity **)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for (int i = 0; i < Num; i++)
		{
			vec2 Diff = apEnts[i]->m_Pos - Pos;
			vec2 ForceDir(0, 1);
			float l = length(Diff);
			if (l)
				ForceDir = normalize(Diff);
			l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
			float Dmg = 14 * l;

			if (Superdamage)
				Dmg *= 5;

			if (apEnts[i]->GetPlayer()->GotAbility(ANTIEXPLOSIONARMOR))
				Dmg -= 1.0f;

			if (Owner > 0 && Owner < MAX_CLIENTS)
			{
				if (m_apPlayers[Owner] && m_apPlayers[Owner]->GotAbility(EXPLOSION_DAMAGE1))
					Dmg += 1.0f;
			}

			if ((int)Dmg && Dmg > 0.0f)
				apEnts[i]->TakeDamage(ForceDir * Dmg * 0.9f, (int)Dmg, Owner, Weapon);
		}
	}
}

/*
void create_smoke(vec2 Pos)
{
	// create the event
	EV_EXPLOSION *pEvent = (EV_EXPLOSION *)events.create(EVENT_SMOKE, sizeof(EV_EXPLOSION));
	if(pEvent)
	{
		pEvent->x = (int)Pos.x;
		pEvent->y = (int)Pos.y;
	}
}*/

void CGameContext::CreatePlayerSpawn(vec2 Pos)
{
	// create the event
	CNetEvent_Spawn *ev = (CNetEvent_Spawn *)m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn));
	if (ev)
	{
		ev->m_X = (int)Pos.x;
		ev->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateDeath(vec2 Pos, int ClientID)
{
	// create the event
	CNetEvent_Death *pEvent = (CNetEvent_Death *)m_Events.Create(NETEVENTTYPE_DEATH, sizeof(CNetEvent_Death));
	if (pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientID = ClientID;
	}
}

void CGameContext::CreateSound(vec2 Pos, int Sound, int Mask)
{
	if (Sound < 0)
		return;

	// create a sound
	CNetEvent_SoundWorld *pEvent = (CNetEvent_SoundWorld *)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(CNetEvent_SoundWorld), Mask);
	if (pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_SoundID = Sound;
	}
}

void CGameContext::CreateSoundGlobal(int Sound, int Target)
{
	if (Sound < 0)
		return;

	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_SoundID = Sound;
	if (Target == -2)
		Server()->SendPackMsg(&Msg, MSGFLAG_NOSEND, -1);
	else
	{
		int Flag = MSGFLAG_VITAL;
		if (Target != -1)
			Flag |= MSGFLAG_NORECORD;
		Server()->SendPackMsg(&Msg, Flag, Target);
	}
}

bool CGameContext::IsBot(int ClientID)
{
	if (ClientID >= 0 && m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_pAI)
		return true;

	return false;
}

void CGameContext::SendChatTarget(int To, const char *pText, ...)
{
	// skip sending to bots
	if (IsBot(To))
		return;

	int Start = (To < 0 ? 0 : To);
	int End = (To < 0 ? MAX_CLIENTS : To + 1);

	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_ClientID = -1;

	dynamic_string Buffer;

	va_list VarArgs;
	va_start(VarArgs, pText);

	for (int i = Start; i < End; i++)
	{
		if (m_apPlayers[i])
		{
			Buffer.clear();
			Server()->Localization()->Format_VL(Buffer, m_apPlayers[i]->m_Language, pText, VarArgs);

			Msg.m_pMessage = Buffer.buffer();
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
		}
	}

	va_end(VarArgs);
}

void CGameContext::SendChat(int ChatterClientID, int Team, const char *pText)
{
	char aBuf[256];
	if (ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
		str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientID, Team, Server()->ClientName(ChatterClientID), pText);
	else
		str_format(aBuf, sizeof(aBuf), "*** %s", pText);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, Team != CHAT_ALL ? "teamchat" : "chat", aBuf);

	if (Team == CHAT_ALL)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = pText;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	}
	else
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 1;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = pText;

		// pack one for the recording only
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);

		// send to the clients
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i] && !IsBot(i) && m_apPlayers[i]->GetTeam() == Team)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
}

void CGameContext::SendEmoticon(int ClientID, int Emoticon)
{
	CNetMsg_Sv_Emoticon Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Emoticon = Emoticon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

void CGameContext::SendWeaponPickup(int ClientID, int Weapon)
{
	CNetMsg_Sv_WeaponPickup Msg;
	Msg.m_Weapon = Weapon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendBroadcast(const char *pText, int ClientID, bool Lock, ...)
{
	CNetMsg_Sv_Broadcast Msg;
	int Start = (ClientID < 0 ? 0 : ClientID);
	int End = (ClientID < 0 ? MAX_CLIENTS : ClientID + 1);

	dynamic_string Buffer;

	va_list VarArgs;
	va_start(VarArgs, pText);

	// only for server demo record
	if (ClientID < 0)
	{
		Server()->Localization()->Format_VL(Buffer, "en", _(pText), VarArgs);
		Msg.m_pMessage = Buffer.buffer();
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);
	}

	for (int i = Start; i < End; i++)
	{
		if (m_apPlayers[i])
		{
			Buffer.clear();
			Server()->Localization()->Format_VL(Buffer, m_apPlayers[i]->m_Language, _(pText), VarArgs);

			Msg.m_pMessage = Buffer.buffer();
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
		}
	}

	va_end(VarArgs);
	if (Lock)
		m_BroadcastLockTick = Server()->Tick() + g_Config.m_SvBroadcastLock * Server()->TickSpeed();
}

//
void CGameContext::StartVote(const char *pDesc, const char *pCommand, const char *pReason)
{
	// check if a vote is already running
	if (m_VoteCloseTime)
		return;

	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_apPlayers[i])
		{
			m_apPlayers[i]->m_Vote = 0;
			m_apPlayers[i]->m_VotePos = 0;
		}
	}

	// start vote
	m_VoteCloseTime = time_get() + time_freq() * 25;
	str_copy(m_aVoteDescription, pDesc, sizeof(m_aVoteDescription));
	str_copy(m_aVoteCommand, pCommand, sizeof(m_aVoteCommand));
	str_copy(m_aVoteReason, pReason, sizeof(m_aVoteReason));
	SendVoteSet(-1);
	m_VoteUpdate = true;
}

void CGameContext::EndVote()
{
	m_VoteCloseTime = 0;
	SendVoteSet(-1);
}

void CGameContext::SendVoteSet(int ClientID)
{
	CNetMsg_Sv_VoteSet Msg;
	if (m_VoteCloseTime)
	{
		Msg.m_Timeout = (m_VoteCloseTime - time_get()) / time_freq();
		Msg.m_pDescription = m_aVoteDescription;
		Msg.m_pReason = m_aVoteReason;
	}
	else
	{
		Msg.m_Timeout = 0;
		Msg.m_pDescription = "";
		Msg.m_pReason = "";
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendVoteStatus(int ClientID, int Total, int Yes, int No)
{
	CNetMsg_Sv_VoteStatus Msg = {0};
	Msg.m_Total = Total;
	Msg.m_Yes = Yes;
	Msg.m_No = No;
	Msg.m_Pass = Total - (Yes + No);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::AbortVoteKickOnDisconnect(int ClientID)
{
	if (m_VoteCloseTime && ((!str_comp_num(m_aVoteCommand, "kick ", 5) && str_toint(&m_aVoteCommand[5]) == ClientID) ||
							(!str_comp_num(m_aVoteCommand, "set_team ", 9) && str_toint(&m_aVoteCommand[9]) == ClientID)))
		m_VoteCloseTime = -1;
}

void CGameContext::CheckPureTuning()
{
	// might not be created yet during start up
	if (!m_pController)
		return;

	if (str_comp(m_pController->m_pGameType, "DM") == 0 ||
		str_comp(m_pController->m_pGameType, "TDM") == 0 ||
		str_comp(m_pController->m_pGameType, "CTF") == 0)
	{
		CTuningParams p;
		if (mem_comp(&p, &m_Tuning, sizeof(p)) != 0)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "resetting tuning due to pure server");
			m_Tuning = p;
		}
	}
}

void CGameContext::SendTuningParams(int ClientID)
{
	CheckPureTuning();

	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int *pParams = (int *)&m_Tuning;
	for (unsigned i = 0; i < sizeof(m_Tuning) / sizeof(int); i++)
		Msg.AddInt(pParams[i]);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SwapTeams()
{
	if (!m_pController->IsTeamplay())
		return;
	SendChatTarget(-1, _("Teams were swapped"));

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		/*
		if(m_apPlayers[i] && m_apPlayers[i]->m_WantedTeam != TEAM_SPECTATORS)
			m_apPlayers[i]->SetWantedTeam(m_apPlayers[i]->m_WantedTeam^1, false);
			//m_apPlayers[i]->SetTeam(m_apPlayers[i]->GetTeam()^1, false);
			*/
		if (m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
		{
			int t = m_apPlayers[i]->GetTeam() ^ 1;
			m_apPlayers[i]->SetTeam(t, false);
			m_apPlayers[i]->SetWantedTeam(t, false);
		}
	}

	(void)m_pController->CheckTeamBalance();
}

void CGameContext::UpdateSpectators()
{

	bool Found[2] = {false, false};

	// check validity
	for (int i = 0; i < 2; i++)
	{
		if (m_aMostInterestingPlayer[i] >= 0)
		{
			// player left or something
			if (!m_apPlayers[m_aMostInterestingPlayer[i]])
			{
				m_aMostInterestingPlayer[i] = -1;
			}
			else
			{
				// player is a spectator
				if (m_apPlayers[m_aMostInterestingPlayer[i]]->GetTeam() == TEAM_SPECTATORS)
					m_aMostInterestingPlayer[i] = -1;
			}
		}
	}

	// find the most interesting player of both teams
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		// player and character exists
		if (m_apPlayers[i] && m_apPlayers[i]->m_EnableAutoSpectating && m_apPlayers[i]->GetCharacter() && m_apPlayers[i]->GetCharacter()->IsAlive())
		{
			int Team = m_apPlayers[i]->GetTeam();

			// team is correct
			if (Team == TEAM_RED || Team == TEAM_BLUE)
			{
				// most interesting player exists

				int Points = -1;
				int Player = m_aMostInterestingPlayer[Team];

				if (Player >= 0)
					if (m_apPlayers[Player] && m_apPlayers[Player]->GetCharacter())
						Points = m_apPlayers[Player]->m_InterestPoints;

				if (m_apPlayers[i]->m_InterestPoints > Points)
				{
					// works
					// char aBuf[128]; str_format(aBuf, sizeof(aBuf), "i = %d, team = %d", i, Team);
					// Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "cstt", aBuf);

					m_aMostInterestingPlayer[Team] = i;
					Found[Team] = true;
				}

				/*
				if (m_aMostInterestingPlayer[Team] >= 0)
				{
					if (m_apPlayers[m_aMostInterestingPlayer[Team]]->GetCharacter() && m_apPlayers[i]->m_InterestPoints > m_apPlayers[m_aMostInterestingPlayer[Team]]->m_InterestPoints)
					{
						// found more interesting player
						m_aMostInterestingPlayer[m_apPlayers[i]->GetTeam()] = i;
						Found[m_apPlayers[i]->GetTeam()] = true;
					}
				}
				else
				if (m_apPlayers[i]->m_InterestPoints > 0)
				{
					m_aMostInterestingPlayer[Team] = i;
					Found[Team] = true;
				}
				*/
			}
		}
	}

	// update the spectator views
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		// if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && !m_apPlayers[i]->m_IsBot)
		if (m_apPlayers[i] && (m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || !m_apPlayers[i]->GetCharacter()) && !m_apPlayers[i]->m_IsBot)
		{
			if (!m_apPlayers[i]->m_LastSetSpectatorMode)
				m_apPlayers[i]->m_LastSetSpectatorMode = Server()->Tick() - Server()->TickSpeed() * g_Config.m_SvSpectatorUpdateTime;
			else
			{
				if (m_apPlayers[i]->m_LastSetSpectatorMode + Server()->TickSpeed() * g_Config.m_SvSpectatorUpdateTime < Server()->Tick())
				{
					int WantedPlayer = -1;

					/*
					if (!m_pController->IsTeamplay())
					{
						if (m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
						{



						}
					}
					else*/
					{
						int Team = m_apPlayers[i]->GetTeam();

						// get the correct player
						if (Team == TEAM_RED || Team == TEAM_BLUE)
						{
							WantedPlayer = m_aMostInterestingPlayer[Team];

							// update the view
							if (WantedPlayer >= 0 && m_apPlayers[i]->m_SpectatorID != WantedPlayer && Found[Team])
							{
								m_apPlayers[i]->m_LastSetSpectatorMode = Server()->Tick();
								m_apPlayers[i]->m_SpectatorID = WantedPlayer;
								Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "cstt", "Spectator id changed");
							}
						}
					}
				}
			}
		}
	}

	/*
			CNetMsg_Cl_SetSpectatorMode *pMsg = (CNetMsg_Cl_SetSpectatorMode *)pRawMsg;

			if(pPlayer->GetTeam() != TEAM_SPECTATORS || pPlayer->m_SpectatorID == pMsg->m_SpectatorID || ClientID == pMsg->m_SpectatorID ||
				(g_Config.m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode+Server()->TickSpeed()*3 > Server()->Tick()))
				return;

			pPlayer->m_LastSetSpectatorMode = Server()->Tick();
			if(pMsg->m_SpectatorID != SPEC_FREEVIEW && (!m_apPlayers[pMsg->m_SpectatorID] || m_apPlayers[pMsg->m_SpectatorID]->GetTeam() == TEAM_SPECTATORS))
				SendChatTarget(ClientID, "Invalid spectator id used");
			else
				pPlayer->m_SpectatorID = pMsg->m_SpectatorID;
	*/
}

void CGameContext::OnTick()
{
	// check tuning
	CheckPureTuning();

	// copy tuning
	m_World.m_Core.m_Tuning = m_Tuning;
	m_World.Tick();

	// if(world.paused) // make sure that the game object always updates
	m_pController->Tick();

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_apPlayers[i])
		{
			m_apPlayers[i]->Tick();
			m_apPlayers[i]->PostTick();
		}
	}

	// update voting
	if (m_VoteCloseTime)
	{
		// abort the kick-vote on player-leave
		if (m_VoteCloseTime == -1)
		{
			SendChatTarget(-1, _("Vote aborted"));
			EndVote();
		}
		else
		{
			int Total = 0, Yes = 0, No = 0;
			if (m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}};
				for (int i = 0; i < MAX_CLIENTS; i++)
					if (m_apPlayers[i] && !IsBot(i))
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
				bool aVoteChecked[MAX_CLIENTS] = {0};
				for (int i = 0; i < MAX_CLIENTS; i++)
				{
					if (!m_apPlayers[i] || IsBot(i) || m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || aVoteChecked[i]) // don't count in votes by spectators
						continue;

					int ActVote = m_apPlayers[i]->m_Vote;
					int ActVotePos = m_apPlayers[i]->m_VotePos;

					// check for more players with the same ip (only use the vote of the one who voted first)
					for (int j = i + 1; j < MAX_CLIENTS; ++j)
					{
						if (!m_apPlayers[j] || IsBot(i) || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]))
							continue;

						aVoteChecked[j] = true;
						if (m_apPlayers[j]->m_Vote && (!ActVote || ActVotePos > m_apPlayers[j]->m_VotePos))
						{
							ActVote = m_apPlayers[j]->m_Vote;
							ActVotePos = m_apPlayers[j]->m_VotePos;
						}
					}

					Total++;
					if (ActVote > 0)
						Yes++;
					else if (ActVote < 0)
						No++;
				}

				if (Yes >= Total / 2 + 1)
					m_VoteEnforce = VOTE_ENFORCE_YES;
				else if (No >= (Total + 1) / 2)
					m_VoteEnforce = VOTE_ENFORCE_NO;
			}

			if (m_VoteEnforce == VOTE_ENFORCE_YES)
			{
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				Console()->ExecuteLine(m_aVoteCommand, -1);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				EndVote();
				SendChatTarget(-1, _("Vote passed"));

				if (m_apPlayers[m_VoteCreator])
					m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;
			}
			else if (m_VoteEnforce == VOTE_ENFORCE_NO || time_get() > m_VoteCloseTime)
			{
				EndVote();
				SendChatTarget(-1, _("Vote failed"));
			}
			else if (m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}

#ifdef CONF_DEBUG
	if (g_Config.m_DbgDummies)
	{
		for (int i = 0; i < g_Config.m_DbgDummies; i++)
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (i & 1) ? -1 : 1;
			m_apPlayers[MAX_CLIENTS - i - 1]->OnPredictedInput(&Input);
		}
	}
#endif
}

void CGameContext::Format_L(const char *LanguageCode, char *Text, ...)
{
	va_list VarArgs;
	va_start(VarArgs, Text);

	dynamic_string buf;
	buf.append((const char *)Text);
	Server()->Localization()->Format_L(buf, LanguageCode, _(Text), VarArgs);
	str_copy(Text, buf.buffer(), sizeof(Text));
	va_end(VarArgs);
}

bool CGameContext::AIInputUpdateNeeded(int ClientID)
{
	if (m_apPlayers[ClientID])
		return m_apPlayers[ClientID]->AIInputChanged();

	return false;
}

void CGameContext::UpdateAI()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_apPlayers[i] && IsBot(i))
			m_apPlayers[i]->AITick();
	}
}

/*
enum InputList
{
	INPUT_MOVE = 0,
	INPUT_SHOOT = 4,
	INPUT_JUMP = 3,
	INPUT_HOOK = 5

	//1 & 2 vectors for weapon direction
};
*/

void CGameContext::AIUpdateInput(int ClientID, int *Data)
{
	if (m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_pAI)
		m_apPlayers[ClientID]->m_pAI->UpdateInput(Data);
}

// Server hooks
void CGameContext::AddZombie()
{
	Server()->AddZombie();
}

void CGameContext::OnClientDirectInput(int ClientID, void *pInput)
{
	if (!m_World.m_Paused)
		m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientPredictedInput(int ClientID, void *pInput)
{
	if (!m_World.m_Paused)
		m_apPlayers[ClientID]->OnPredictedInput((CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientEnter(int ClientID)
{
	// world.insert_entity(&players[client_id]);
	m_apPlayers[ClientID]->Respawn();
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "'%s' joined the fun", Server()->ClientName(ClientID));
	SendChatTarget(-1, _("'{%s}' joined the fun"), Server()->ClientName(ClientID));

	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientID, Server()->ClientName(ClientID), m_apPlayers[ClientID]->GetTeam());
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	if (str_comp(g_Config.m_SvGametype, "coop") == 0 && g_Config.m_SvMapGen)
	{
		switch (g_Config.m_SvInvFails)
		{
		case 0:
			SendBroadcast(_("Level {%d}"), -1, true, g_Config.m_SvMapGenLevel);
			break;

		case 1:
			SendBroadcast(_("Level {%d} - Second try"), -1, true, g_Config.m_SvMapGenLevel);
			break;

		case 2:
			SendBroadcast(_("Level {%d} - Last chance..."), -1, true, g_Config.m_SvMapGenLevel);
			break;

		default:
			break;
		}
	}

	m_VoteUpdate = true;
}

void CGameContext::OnClientConnected(int ClientID, bool AI)
{
	// Check which team the player should be on
	const int StartTeam = g_Config.m_SvTournamentMode ? TEAM_SPECTATORS : m_pController->GetAutoTeam(ClientID);

	m_apPlayers[ClientID] = new (ClientID) CPlayer(this, ClientID, StartTeam);
	m_apPlayers[ClientID]->m_IsBot = AI;

	(void)m_pController->CheckTeamBalance();

#ifdef CONF_DEBUG
	if (g_Config.m_DbgDummies)
	{
		if (ClientID >= MAX_CLIENTS - g_Config.m_DbgDummies)
			return;
	}
#endif

	// send active vote
	if (m_VoteCloseTime)
		SendVoteSet(ClientID);

	// send motd
	/* skip motd
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = g_Config.m_SvMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	*/
}

void CGameContext::OnClientDrop(int ClientID, const char *pReason)
{
	AbortVoteKickOnDisconnect(ClientID);
	m_apPlayers[ClientID]->OnDisconnect(pReason);
	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = 0;

	(void)m_pController->CheckTeamBalance();
	m_VoteUpdate = true;

	// update spectator modes
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (m_apPlayers[i] && m_apPlayers[i]->m_SpectatorID == ClientID)
			m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
	}
}

void CGameContext::OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID)
{
	void *pRawMsg = m_NetObjHandler.SecureUnpackMsg(MsgID, pUnpacker);
	CPlayer *pPlayer = m_apPlayers[ClientID];

	if (!pRawMsg)
	{
		if (g_Config.m_Debug)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "dropped weird message '%s' (%d), failed on '%s'", m_NetObjHandler.GetMsgName(MsgID), MsgID, m_NetObjHandler.FailedMsgOn());
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
		}
		return;
	}

	if (Server()->ClientIngame(ClientID))
	{
		if (MsgID == NETMSGTYPE_CL_SAY)
		{
			if (g_Config.m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat + Server()->TickSpeed() > Server()->Tick())
				return;

			CNetMsg_Cl_Say *pMsg = (CNetMsg_Cl_Say *)pRawMsg;
			int Team = pMsg->m_Team ? pPlayer->GetTeam() : CGameContext::CHAT_ALL;

			// trim right and set maximum length to 128 utf8-characters
			int Length = 0;
			const char *p = pMsg->m_pMessage;
			const char *pEnd = 0;
			while (*p)
			{
				const char *pStrOld = p;
				int Code = str_utf8_decode(&p);

				// check if unicode is not empty
				if (Code > 0x20 && Code != 0xA0 && Code != 0x034F && (Code < 0x2000 || Code > 0x200F) && (Code < 0x2028 || Code > 0x202F) &&
					(Code < 0x205F || Code > 0x2064) && (Code < 0x206A || Code > 0x206F) && (Code < 0xFE00 || Code > 0xFE0F) &&
					Code != 0xFEFF && (Code < 0xFFF9 || Code > 0xFFFC))
				{
					pEnd = 0;
				}
				else if (pEnd == 0)
					pEnd = pStrOld;

				if (++Length >= 127)
				{
					*(const_cast<char *>(p)) = 0;
					break;
				}
			}
			if (pEnd != 0)
				*(const_cast<char *>(pEnd)) = 0;

			// drop empty and autocreated spam messages (more than 16 characters per second)
			if (Length == 0 || (g_Config.m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat + Server()->TickSpeed() * ((15 + Length) / 16) > Server()->Tick()))
				return;

			bool SendToTeam = true;
			bool SkipSending = false;

			pPlayer->m_LastChat = Server()->Tick();

			// /help /weapon /smth
			if (pMsg->m_pMessage[0] == '/')
			{
				if (str_comp(pMsg->m_pMessage, "/help") == 0 || str_comp(pMsg->m_pMessage, "/info") == 0)
				{
					SendChatTarget(ClientID, _("Engine version 1.54"));
					SendChatTarget(ClientID, _(""));
					SendChatTarget(ClientID, _("Use voting system to do shopping, /cmdlist for commands"));
					SendChatTarget(ClientID, _("For updates and more info check teeworlds.com/forum"));
				}

				else if (str_comp(pMsg->m_pMessage, "/cmdlist") == 0 || str_comp(pMsg->m_pMessage, "/cmd") == 0 || str_comp(pMsg->m_pMessage, "/commands") == 0)
				{
					SendChatTarget(ClientID, _("Commands:"));
					SendChatTarget(ClientID, _("/dwc - Disable / enable weapon chat (Using: ...)"));
					SendChatTarget(ClientID, _("/dwb - Disable / enable weapon broadcast"));
					SendChatTarget(ClientID, _("/das - Disable / enable auto spectating"));
					SendChatTarget(ClientID, "/deg - Disable / enable throwing grenades with emoticons");
					SendChatTarget(ClientID, "/grenade - Throw a grenade");
				}

				else if (str_comp(pMsg->m_pMessage, "/buildtool") == 0)
				{
					if (pPlayer->GetCharacter())
						pPlayer->GetCharacter()->GiveCustomWeapon(HAMMER_BUILD);
				}

				else if (str_comp(pMsg->m_pMessage, "/dwc") == 0)
				{
					if (pPlayer->m_EnableWeaponInfo == 1)
						pPlayer->m_EnableWeaponInfo = 0;
					else
						pPlayer->m_EnableWeaponInfo = 1;

					if (pPlayer->m_EnableWeaponInfo)
						SendChatTarget(ClientID, _("Weapon chat info messages enabled"));
					else
						SendChatTarget(ClientID, _("Weapon chat info messages disabled"));
				}

				else if (str_comp(pMsg->m_pMessage, "/dwb") == 0)
				{
					if (pPlayer->m_EnableWeaponInfo == 2)
						pPlayer->m_EnableWeaponInfo = 0;
					else
						pPlayer->m_EnableWeaponInfo = 2;

					if (pPlayer->m_EnableWeaponInfo)
						SendChatTarget(ClientID, _("Weapon broadcast info messages enabled"));
					else
						SendChatTarget(ClientID, _("Weapon broadcast info messages disabled"));
				}

				else if (str_comp(pMsg->m_pMessage, "/deg") == 0)
				{
					pPlayer->m_EnableEmoticonGrenades = !pPlayer->m_EnableEmoticonGrenades;
					if (pPlayer->m_EnableEmoticonGrenades)
						SendChatTarget(ClientID, _("Throwing grenades with emoticons enabled"));
					else
						SendChatTarget(ClientID, _("Throwing grenades with emoticons disabled"));
				}

				else if (str_comp(pMsg->m_pMessage, "/das") == 0)
				{
					pPlayer->m_EnableAutoSpectating = !pPlayer->m_EnableAutoSpectating;
					if (pPlayer->m_EnableAutoSpectating)
						SendChatTarget(ClientID, _("Auto spectating enabled"));
					else
						SendChatTarget(ClientID, _("Auto spectating disabled"));
				}
				else if (str_comp(pMsg->m_pMessage, "/showwaypoints") == 0)
				{
					m_ShowWaypoints = !m_ShowWaypoints;
				}
				else if (!str_comp_num(pMsg->m_pMessage, "/timeout", 8))
				{
					char Timeout[256];
					if (sscanf(pMsg->m_pMessage, "/timeout %s", Timeout) == 1)
						str_copy(pPlayer->m_TimeoutID, Timeout, sizeof(pPlayer->m_TimeoutID));
					
					if(pPlayer->GetCharacter())
						pPlayer->GetCharacter()->GiveStartWeapon();
				}
				else if (!str_comp_num(pMsg->m_pMessage, "/mc;timeout", 11))
				{
					char Timeout[1024];
					char Useless[4];
					if (sscanf(pMsg->m_pMessage, "/mc;timeout %s;%s", Timeout, Useless) == 1)
						str_copy(pPlayer->m_TimeoutID, Timeout, sizeof(pPlayer->m_TimeoutID));

					if(pPlayer->GetCharacter())
						pPlayer->GetCharacter()->GiveStartWeapon();
				}
				else
					SendChatTarget(ClientID, _("No Such Command."));
				SkipSending = true;
			}

			if (!SkipSending)
			{
				if (SendToTeam)
					SendChat(ClientID, Team, pMsg->m_pMessage);
				else
					SendChatTarget(ClientID, pMsg->m_pMessage);
			}

			/*

			*/
		}
		else if (MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			if (g_Config.m_SvSpamprotection && pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry + Server()->TickSpeed() * 1 > Server()->Tick())
				return;

			int64 Now = Server()->Tick();
			pPlayer->m_LastVoteTry = Now;
			if (pPlayer->GetTeam() == TEAM_SPECTATORS)
			{
				SendChatTarget(ClientID, "Spectators aren't allowed to start a vote.");
				return;
			}

			if (m_VoteCloseTime)
			{
				SendChatTarget(ClientID, "Wait for current vote to end before calling a new one.");
				return;
			}

			int Timeleft = pPlayer->m_LastVoteCall + Server()->TickSpeed() * 60 - Now;
			if (pPlayer->m_LastVoteCall && Timeleft > 0)
			{
				char aChatmsg[512] = {0};
				str_format(aChatmsg, sizeof(aChatmsg), "You must wait %d seconds before making another vote", (Timeleft / Server()->TickSpeed()) + 1);
				SendChatTarget(ClientID, aChatmsg);
				return;
			}

			char aChatmsg[512] = {0};
			char aDesc[VOTE_DESC_LENGTH] = {0};
			char aCmd[VOTE_CMD_LENGTH] = {0};
			CNetMsg_Cl_CallVote *pMsg = (CNetMsg_Cl_CallVote *)pRawMsg;
			const char *pReason = pMsg->m_pReason[0] ? pMsg->m_pReason : "No reason given";

			if (str_comp_nocase(pMsg->m_pType, "option") == 0)
			{
				CVoteOptionServer *pOption = m_pVoteOptionFirst;

				while (pOption)
				{
					if (str_comp_nocase(pMsg->m_pValue, pOption->m_aDescription) == 0)
					{
						str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s' (%s)", Server()->ClientName(ClientID),
								   pOption->m_aDescription, pReason);
						str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aDescription);
						str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);
						break;
					}

					pOption = pOption->m_pNext;
				}

				if (!pOption)
				{
					// str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aDescription);
					// str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);

					str_format(aChatmsg, sizeof(aChatmsg), "'%s' isn't an option on this server", pMsg->m_pValue);
					SendChatTarget(ClientID, aChatmsg);

					return;
				}
			}
			else if (str_comp_nocase(pMsg->m_pType, "kick") == 0)
			{
				if (!g_Config.m_SvVoteKick)
				{
					SendChatTarget(ClientID, "Server does not allow voting to kick players");
					return;
				}

				if (g_Config.m_SvVoteKickMin)
				{
					int PlayerNum = 0;
					for (int i = 0; i < MAX_CLIENTS; ++i)
						if (m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
							++PlayerNum;

					if (PlayerNum < g_Config.m_SvVoteKickMin)
					{
						str_format(aChatmsg, sizeof(aChatmsg), "Kick voting requires %d players on the server", g_Config.m_SvVoteKickMin);
						SendChatTarget(ClientID, aChatmsg);
						return;
					}
				}

				int KickID = str_toint(pMsg->m_pValue);
				if (KickID < 0 || !m_apPlayers[KickID])
				{
					SendChatTarget(ClientID, "Invalid client id to kick");
					return;
				}
				if (KickID == ClientID)
				{
					SendChatTarget(ClientID, "You can't kick yourself");
					return;
				}
				if (Server()->IsAuthed(KickID))
				{
					SendChatTarget(ClientID, "You can't kick admins");
					char aBufKick[128];
					str_format(aBufKick, sizeof(aBufKick), "'%s' called for vote to kick you", Server()->ClientName(ClientID));
					SendChatTarget(KickID, aBufKick);
					return;
				}

				str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to kick '%s' (%s)", Server()->ClientName(ClientID), Server()->ClientName(KickID), pReason);
				str_format(aDesc, sizeof(aDesc), "Kick '%s'", Server()->ClientName(KickID));
				if (!g_Config.m_SvVoteKickBantime)
					str_format(aCmd, sizeof(aCmd), "kick %d Kicked by vote", KickID);
				else
				{
					char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
					Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
					str_format(aCmd, sizeof(aCmd), "ban %s %d Banned by vote", aAddrStr, g_Config.m_SvVoteKickBantime);
				}
			}
			else if (str_comp_nocase(pMsg->m_pType, "spectate") == 0)
			{
				if (!g_Config.m_SvVoteSpectate)
				{
					SendChatTarget(ClientID, "Server does not allow voting to move players to spectators");
					return;
				}

				int SpectateID = str_toint(pMsg->m_pValue);
				if (SpectateID < 0 || SpectateID >= MAX_CLIENTS || !m_apPlayers[SpectateID] || m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
				{
					SendChatTarget(ClientID, "Invalid client id to move");
					return;
				}
				if (SpectateID == ClientID)
				{
					SendChatTarget(ClientID, "You can't move yourself");
					return;
				}

				str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to move '%s' to spectators (%s)", Server()->ClientName(ClientID), Server()->ClientName(SpectateID), pReason);
				str_format(aDesc, sizeof(aDesc), "move '%s' to spectators", Server()->ClientName(SpectateID));
				str_format(aCmd, sizeof(aCmd), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
			}

			// do nothing
			if (str_comp(aCmd, "null") == 0)
			{
				return;
			}

			// class selecting
			for (int i = 0; i < NUM_CLASSES; i++)
			{
				if (str_comp(aCmd, aClassName[i]) == 0)
				{
					if (m_apPlayers[ClientID]->SelectClass(i))
						Server()->SetClientClan(ClientID, aClassName[i]);
					ResetVotes();
					return;
				}
			}

			// class abilities
			for (int i = 0; i < NUM_ABILITIES; i++)
			{
				if (str_comp(aCmd, aAbilities[i].m_aName) == 0)
				{
					if (m_apPlayers[ClientID]->SelectAbility(i))
						// char aBuf[256];
						// str_format(aBuf, sizeof(aBuf), "Not enough money for %s", aCustomWeapon[CustomWeapon].m_Name);
						SendChatTarget(ClientID, aAbilities[i].m_aChatMsg);
					else
						SendChatTarget(ClientID, "Not enough ability points.");

					ResetVotes();
					return;
				}
			}

			// buying
			for (int i = 0; i < NUM_CUSTOMWEAPONS; i++)
			{
				if (str_comp(aCmd, aCustomWeapon[i].m_BuyCmd) == 0)
				{
					m_apPlayers[ClientID]->BuyWeapon(i);
					ResetVotes();
					return;
				}
			}

			if (aCmd[0])
			{
				SendChatTarget(-1, aChatmsg);
				StartVote(aDesc, aCmd, pReason);
				pPlayer->m_Vote = 1;
				pPlayer->m_VotePos = m_VotePos = 1;
				m_VoteCreator = ClientID;
				pPlayer->m_LastVoteCall = Now;
			}
		}
		else if (MsgID == NETMSGTYPE_CL_VOTE)
		{
			CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;
			if (!pMsg->m_Vote)
				return;

			if (m_VoteCloseTime && pPlayer->m_Vote == 1)
				pPlayer->PressVote(pMsg->m_Vote);

			if (!m_VoteCloseTime)
				pPlayer->PressVote(pMsg->m_Vote);

			if (pPlayer->m_Vote == 0 && m_VoteCloseTime)
			{
				pPlayer->m_Vote = pMsg->m_Vote;
				pPlayer->m_VotePos = ++m_VotePos;
				m_VoteUpdate = true;
			}
		}
		else if (MsgID == NETMSGTYPE_CL_SETTEAM && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetTeam *pMsg = (CNetMsg_Cl_SetTeam *)pRawMsg;

			if (pPlayer->GetTeam() == pMsg->m_Team || (g_Config.m_SvSpamprotection && pPlayer->m_LastSetTeam && pPlayer->m_LastSetTeam + Server()->TickSpeed() * 1 > Server()->Tick()))
				return;

			pPlayer->m_LastSetTeam = Server()->Tick();
			if (pPlayer->GetTeam() == TEAM_SPECTATORS || pMsg->m_Team == TEAM_SPECTATORS)
				m_VoteUpdate = true;
			pPlayer->SetTeam(pMsg->m_Team);
			pPlayer->m_TeamChangeTick = Server()->Tick();

			// pPlayer->m_WantedTeam = pMsg->m_Team;

			/*
			if(pMsg->m_Team != TEAM_SPECTATORS)
			{
				pPlayer->SetTeam(TEAM_SPECTATORS);
				pPlayer->SetWantedTeam(pMsg->m_Team);
			}
			else
			{
				pPlayer->SetTeam(TEAM_SPECTATORS);
				pPlayer->m_WantedTeam = pMsg->m_Team;
			}
			*/

			/* 1.4 way
			pPlayer->m_LastSetTeam = Server()->Tick();
			if(str_comp(g_Config.m_SvGametype, "cstt") == 0)
			{
				pPlayer->SetTeam(TEAM_SPECTATORS, pMsg->m_Team == TEAM_SPECTATORS);
				pPlayer->SetWantedTeam(pMsg->m_Team);
			}
			else
			{
				pPlayer->SetTeam(pMsg->m_Team, true);
				pPlayer->SetWantedTeam(pMsg->m_Team);
			}
			*/

			/*
			if(pMsg->m_Team != TEAM_SPECTATORS && m_LockTeams)
			{
				pPlayer->m_LastSetTeam = Server()->Tick();
				SendBroadcast(_("Teams are locked", ClientID);
				return;
			}
			*/

			/*
			if(pPlayer->m_TeamChangeTick > Server()->Tick())
			{
				pPlayer->m_LastSetTeam = Server()->Tick();
				int TimeLeft = (pPlayer->m_TeamChangeTick - Server()->Tick())/Server()->TickSpeed();
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Time to wait before changing team: %02d:%02d", TimeLeft/60, TimeLeft%60);
				SendBroadcast(aBuf, ClientID);
				return;
			}


			// Switch team on given client and kill/respawn him
			if(m_pController->CanJoinTeam(pMsg->m_Team, ClientID))
			{
				if(m_pController->CanChangeTeam(pPlayer, pMsg->m_Team))
				{
					pPlayer->m_LastSetTeam = Server()->Tick();
					if(pPlayer->GetTeam() == TEAM_SPECTATORS || pMsg->m_Team == TEAM_SPECTATORS)
						m_VoteUpdate = true;
					pPlayer->SetTeam(pMsg->m_Team);
					//(void)m_pController->CheckTeamBalance();
					pPlayer->m_TeamChangeTick = Server()->Tick();
				}
				//else
				//	SendBroadcast(_("Teams must be balanced, please join other team", ClientID);
			}
			else
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Only %d active players are allowed", Server()->MaxClients()-g_Config.m_SvSpectatorSlots);
				SendBroadcast(aBuf, ClientID);
			}
			*/
		}
		else if (MsgID == NETMSGTYPE_CL_SETSPECTATORMODE && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetSpectatorMode *pMsg = (CNetMsg_Cl_SetSpectatorMode *)pRawMsg;

			if (pPlayer->GetTeam() != TEAM_SPECTATORS || pPlayer->m_SpectatorID == pMsg->m_SpectatorID || ClientID == pMsg->m_SpectatorID ||
				(g_Config.m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode + Server()->TickSpeed() * 1 > Server()->Tick()))
				return;

			pPlayer->m_LastSetSpectatorMode = Server()->Tick();
			if (pMsg->m_SpectatorID != SPEC_FREEVIEW && (!m_apPlayers[pMsg->m_SpectatorID] || m_apPlayers[pMsg->m_SpectatorID]->GetTeam() == TEAM_SPECTATORS))
				SendChatTarget(ClientID, "Invalid spectator id used");
			else
				pPlayer->m_SpectatorID = pMsg->m_SpectatorID;
		}
		else if (MsgID == NETMSGTYPE_CL_CHANGEINFO)
		{
			if (g_Config.m_SvSpamprotection && pPlayer->m_LastChangeInfo && pPlayer->m_LastChangeInfo + Server()->TickSpeed() * 5 > Server()->Tick())
				return;

			CNetMsg_Cl_ChangeInfo *pMsg = (CNetMsg_Cl_ChangeInfo *)pRawMsg;
			pPlayer->m_LastChangeInfo = Server()->Tick();

			// set infos
			char aOldName[MAX_NAME_LENGTH];
			str_copy(aOldName, Server()->ClientName(ClientID), sizeof(aOldName));
			Server()->SetClientName(ClientID, pMsg->m_pName);
			if (str_comp(aOldName, Server()->ClientName(ClientID)) != 0)
			{
				char aChatText[256];
				str_format(aChatText, sizeof(aChatText), "'%s' changed name to '%s'", aOldName, Server()->ClientName(ClientID));
				SendChatTarget(-1, aChatText);
			}
			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);
			str_copy(pPlayer->m_TeeInfos.m_SkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_SkinName));
			pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
			pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
			pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
			pPlayer->m_TeeInfos.m_ColorSkin = pMsg->m_ColorFeet * pMsg->m_ColorBody + ((int)pPlayer->m_TeeInfos.m_SkinName[2]);
			pPlayer->SetLanguage(Server()->Localization()->GetLanguageCode(pMsg->m_Country));

			m_pController->OnPlayerInfoChange(pPlayer);
		}
		else if (MsgID == NETMSGTYPE_CL_EMOTICON && !m_World.m_Paused)
		{
			CNetMsg_Cl_Emoticon *pMsg = (CNetMsg_Cl_Emoticon *)pRawMsg;

			if (g_Config.m_SvSpamprotection && pPlayer->m_LastEmote && pPlayer->m_LastEmote + Server()->TickSpeed() * 1 > Server()->Tick())
				return;

			pPlayer->m_LastEmote = Server()->Tick();

			// throwing grenades
			/*
			//if (pMsg->m_Emoticon == EMOTICON_DEVILTEE)
			{
				if (pPlayer->GetCharacter() && pPlayer->m_EnableEmoticonGrenades)
				{
					pPlayer->GetCharacter()->ThrowGrenade(pMsg->m_Emoticon*(360.0f / NUM_EMOTICONS));
				}
			}
			*/

			if (pMsg->m_Emoticon == EMOTICON_HEARTS)
			{
				if (pPlayer->GetCharacter() && pPlayer->GotAbility(STORE_HEALTH))
				{
					pPlayer->GetCharacter()->m_UseMedkit = true;
					/*
					int Explode = 0;
					if (pPlayer->GotAbility(EXPLOSIVE_HEARTS))
						Explode = 1;

					switch (pPlayer->GetCharacter()->m_HealthStored)
					{
						case 1:
							m_pController->DropPickup(pPlayer->GetCharacter()->m_Pos+vec2(0, -32), POWERUP_HEALTH, vec2(0, -12.0f), Explode, pPlayer->GetCID());
							break;
						case 2:
							m_pController->DropPickup(pPlayer->GetCharacter()->m_Pos+vec2(0, -32), POWERUP_HEALTH, vec2(-4, -11.0f), Explode, pPlayer->GetCID());
							m_pController->DropPickup(pPlayer->GetCharacter()->m_Pos+vec2(0, -32), POWERUP_HEALTH, vec2(4, -11.0f), Explode, pPlayer->GetCID());
							break;
						case 3:
							m_pController->DropPickup(pPlayer->GetCharacter()->m_Pos+vec2(0, -32), POWERUP_HEALTH, vec2(-5, -10.0f), Explode, pPlayer->GetCID());
							m_pController->DropPickup(pPlayer->GetCharacter()->m_Pos+vec2(0, -32), POWERUP_HEALTH, vec2(0, -12.0f), Explode, pPlayer->GetCID());
							m_pController->DropPickup(pPlayer->GetCharacter()->m_Pos+vec2(0, -32), POWERUP_HEALTH, vec2(5, -10.0f), Explode, pPlayer->GetCID());
							break;
					};

					pPlayer->GetCharacter()->m_HealthStored = 0;
					*/
				}
			}

			SendEmoticon(ClientID, pMsg->m_Emoticon);
		}
		else if (MsgID == NETMSGTYPE_CL_KILL && !m_World.m_Paused)
		{
			if (pPlayer->m_LastKill && pPlayer->m_LastKill + Server()->TickSpeed() * 1 > Server()->Tick())
				return;

			// pPlayer->m_LastKill = Server()->Tick();
			// pPlayer->KillCharacter(WEAPON_SELF);

			// reload instead of self kill
			if (pPlayer->GetCharacter())
				pPlayer->GetCharacter()->DoReloading(true);
		}
	}
	else
	{
		if (MsgID == NETMSGTYPE_CL_STARTINFO)
		{
			if (pPlayer->m_IsReady)
				return;

			CNetMsg_Cl_StartInfo *pMsg = (CNetMsg_Cl_StartInfo *)pRawMsg;
			pPlayer->m_LastChangeInfo = Server()->Tick();

			// set start infos
			Server()->SetClientName(ClientID, pMsg->m_pName);
			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);
			str_copy(pPlayer->m_TeeInfos.m_SkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_SkinName));
			pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
			pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
			pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
			pPlayer->SetLanguage(Server()->Localization()->GetLanguageCode(pMsg->m_Country));
			m_pController->OnPlayerInfoChange(pPlayer);

			// send vote options
			CNetMsg_Sv_VoteClearOptions ClearMsg;
			Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);

			CNetMsg_Sv_VoteOptionListAdd OptionMsg;
			int NumOptions = 0;
			OptionMsg.m_pDescription0 = "";
			OptionMsg.m_pDescription1 = "";
			OptionMsg.m_pDescription2 = "";
			OptionMsg.m_pDescription3 = "";
			OptionMsg.m_pDescription4 = "";
			OptionMsg.m_pDescription5 = "";
			OptionMsg.m_pDescription6 = "";
			OptionMsg.m_pDescription7 = "";
			OptionMsg.m_pDescription8 = "";
			OptionMsg.m_pDescription9 = "";
			OptionMsg.m_pDescription10 = "";
			OptionMsg.m_pDescription11 = "";
			OptionMsg.m_pDescription12 = "";
			OptionMsg.m_pDescription13 = "";
			OptionMsg.m_pDescription14 = "";
			CVoteOptionServer *pCurrent = m_pVoteOptionFirst;
			while (pCurrent)
			{
				switch (NumOptions++)
				{
				case 0:
					OptionMsg.m_pDescription0 = pCurrent->m_aDescription;
					break;
				case 1:
					OptionMsg.m_pDescription1 = pCurrent->m_aDescription;
					break;
				case 2:
					OptionMsg.m_pDescription2 = pCurrent->m_aDescription;
					break;
				case 3:
					OptionMsg.m_pDescription3 = pCurrent->m_aDescription;
					break;
				case 4:
					OptionMsg.m_pDescription4 = pCurrent->m_aDescription;
					break;
				case 5:
					OptionMsg.m_pDescription5 = pCurrent->m_aDescription;
					break;
				case 6:
					OptionMsg.m_pDescription6 = pCurrent->m_aDescription;
					break;
				case 7:
					OptionMsg.m_pDescription7 = pCurrent->m_aDescription;
					break;
				case 8:
					OptionMsg.m_pDescription8 = pCurrent->m_aDescription;
					break;
				case 9:
					OptionMsg.m_pDescription9 = pCurrent->m_aDescription;
					break;
				case 10:
					OptionMsg.m_pDescription10 = pCurrent->m_aDescription;
					break;
				case 11:
					OptionMsg.m_pDescription11 = pCurrent->m_aDescription;
					break;
				case 12:
					OptionMsg.m_pDescription12 = pCurrent->m_aDescription;
					break;
				case 13:
					OptionMsg.m_pDescription13 = pCurrent->m_aDescription;
					break;
				case 14:
				{
					OptionMsg.m_pDescription14 = pCurrent->m_aDescription;
					OptionMsg.m_NumOptions = NumOptions;
					Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
					OptionMsg = CNetMsg_Sv_VoteOptionListAdd();
					NumOptions = 0;
					OptionMsg.m_pDescription1 = "";
					OptionMsg.m_pDescription2 = "";
					OptionMsg.m_pDescription3 = "";
					OptionMsg.m_pDescription4 = "";
					OptionMsg.m_pDescription5 = "";
					OptionMsg.m_pDescription6 = "";
					OptionMsg.m_pDescription7 = "";
					OptionMsg.m_pDescription8 = "";
					OptionMsg.m_pDescription9 = "";
					OptionMsg.m_pDescription10 = "";
					OptionMsg.m_pDescription11 = "";
					OptionMsg.m_pDescription12 = "";
					OptionMsg.m_pDescription13 = "";
					OptionMsg.m_pDescription14 = "";
				}
				}
				pCurrent = pCurrent->m_pNext;
			}
			if (NumOptions > 0)
			{
				OptionMsg.m_NumOptions = NumOptions;
				Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
			}

			// send tuning parameters to client
			SendTuningParams(ClientID);

			// client is ready to enter
			pPlayer->m_IsReady = true;
			CNetMsg_Sv_ReadyToEnter m;
			Server()->SendPackMsg(&m, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);
		}
	}
}

void CGameContext::ConTuneParam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pParamName = pResult->GetString(0);
	float NewValue = pResult->GetFloat(1);

	if (pSelf->Tuning()->Set(pParamName, NewValue))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		pSelf->SendTuningParams(-1);
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
}

void CGameContext::ConTuneReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CTuningParams TuningParams;
	*pSelf->Tuning() = TuningParams;
	pSelf->SendTuningParams(-1);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "Tuning reset");
}

void CGameContext::ConTuneDump(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[256];
	for (int i = 0; i < pSelf->Tuning()->Num(); i++)
	{
		float v;
		pSelf->Tuning()->Get(i, &v);
		str_format(aBuf, sizeof(aBuf), "%s %.2f", pSelf->Tuning()->m_apNames[i], v);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
}

void CGameContext::ConPause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if (pSelf->m_pController->IsGameOver())
		return;

	pSelf->m_World.m_Paused ^= 1;
}

void CGameContext::ConChangeMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->ChangeMap(pResult->NumArguments() ? pResult->GetString(0) : "");
}

void CGameContext::ConRestart(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (pResult->NumArguments())
		pSelf->m_pController->DoWarmup(pResult->GetInteger(0));
	else
		pSelf->m_pController->StartRound();
}

void CGameContext::ConBroadcast(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendBroadcast(_(pResult->GetString(0)), -1);
}

void CGameContext::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChatTarget(-1, pResult->GetString(0));
}

void CGameContext::ConSetTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS - 1);
	int Team = clamp(pResult->GetInteger(1), -1, 1);
	int Delay = pResult->NumArguments() > 2 ? pResult->GetInteger(2) : 0;
	if (!pSelf->m_apPlayers[ClientID])
		return;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "moved client %d to team %d", ClientID, Team);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	pSelf->m_apPlayers[ClientID]->m_TeamChangeTick = pSelf->Server()->Tick() + pSelf->Server()->TickSpeed() * Delay * 60;
	pSelf->m_apPlayers[ClientID]->SetTeam(Team);
	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConSetTeamAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Team = clamp(pResult->GetInteger(0), -1, 1);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "All players were moved to the %s", pSelf->m_pController->GetTeamName(Team));
	pSelf->SendChatTarget(-1, aBuf);

	for (int i = 0; i < MAX_CLIENTS; ++i)
		if (pSelf->m_apPlayers[i])
			pSelf->m_apPlayers[i]->SetTeam(Team, false);

	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConSwapTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SwapTeams();
}

void CGameContext::ConShuffleTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!pSelf->m_pController->IsTeamplay())
		return;

	int CounterRed = 0;
	int CounterBlue = 0;
	int PlayerTeam = 0;
	for (int i = 0; i < MAX_CLIENTS; ++i)
		if (pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			++PlayerTeam;
	PlayerTeam = (PlayerTeam + 1) / 2;

	pSelf->SendChatTarget(-1, _("Teams were shuffled"));

	/*
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
		{
			if(CounterRed == PlayerTeam)
				pSelf->m_apPlayers[i]->SetTeam(TEAM_BLUE, false);
			else if(CounterBlue == PlayerTeam)
				pSelf->m_apPlayers[i]->SetTeam(TEAM_RED, false);
			else
			{
				if(rand() % 2)
				{
					pSelf->m_apPlayers[i]->SetTeam(TEAM_BLUE, false);
					++CounterBlue;
				}
				else
				{
					pSelf->m_apPlayers[i]->SetTeam(TEAM_RED, false);
					++CounterRed;
				}
			}
		}
	}
	*/

	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConLockTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_LockTeams ^= 1;
	if (pSelf->m_LockTeams)
		pSelf->SendChatTarget(-1, _("Teams were locked"));
	else
		pSelf->SendChatTarget(-1, _("Teams were unlocked"));
}

void CGameContext::ConAddVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);

	if (pSelf->m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "maximum number of vote options reached");
		return;
	}

	// check for valid option
	if (!pSelf->Console()->LineIsValid(pCommand) || str_length(pCommand) >= VOTE_CMD_LENGTH)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", pCommand);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}
	while (*pDescription && *pDescription == ' ')
		pDescription++;
	if (str_length(pDescription) >= VOTE_DESC_LENGTH || *pDescription == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid option '%s'", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// check for duplicate entry
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while (pOption)
	{
		if (str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "option '%s' already exists", pDescription);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
		pOption = pOption->m_pNext;
	}

	// add the option
	++pSelf->m_NumVoteOptions;
	int Len = str_length(pCommand);

	pOption = (CVoteOptionServer *)pSelf->m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
	pOption->m_pNext = 0;
	pOption->m_pPrev = pSelf->m_pVoteOptionLast;
	if (pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	pSelf->m_pVoteOptionLast = pOption;
	if (!pSelf->m_pVoteOptionFirst)
		pSelf->m_pVoteOptionFirst = pOption;

	str_copy(pOption->m_aDescription, pDescription, sizeof(pOption->m_aDescription));
	mem_copy(pOption->m_aCommand, pCommand, Len + 1);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "added option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// inform clients about added option
	CNetMsg_Sv_VoteOptionAdd OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);
}

void CGameContext::ConRemoveVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);

	// check for valid option
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while (pOption)
	{
		if (str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
			break;
		pOption = pOption->m_pNext;
	}
	if (!pOption)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "option '%s' does not exist", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// inform clients about removed option
	CNetMsg_Sv_VoteOptionRemove OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);

	// TODO: improve this
	// remove the option
	--pSelf->m_NumVoteOptions;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "removed option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	CHeap *pVoteOptionHeap = new CHeap();
	CVoteOptionServer *pVoteOptionFirst = 0;
	CVoteOptionServer *pVoteOptionLast = 0;
	int NumVoteOptions = pSelf->m_NumVoteOptions;
	for (CVoteOptionServer *pSrc = pSelf->m_pVoteOptionFirst; pSrc; pSrc = pSrc->m_pNext)
	{
		if (pSrc == pOption)
			continue;

		// copy option
		int Len = str_length(pSrc->m_aCommand);
		CVoteOptionServer *pDst = (CVoteOptionServer *)pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
		pDst->m_pNext = 0;
		pDst->m_pPrev = pVoteOptionLast;
		if (pDst->m_pPrev)
			pDst->m_pPrev->m_pNext = pDst;
		pVoteOptionLast = pDst;
		if (!pVoteOptionFirst)
			pVoteOptionFirst = pDst;

		str_copy(pDst->m_aDescription, pSrc->m_aDescription, sizeof(pDst->m_aDescription));
		mem_copy(pDst->m_aCommand, pSrc->m_aCommand, Len + 1);
	}

	// clean up
	delete pSelf->m_pVoteOptionHeap;
	pSelf->m_pVoteOptionHeap = pVoteOptionHeap;
	pSelf->m_pVoteOptionFirst = pVoteOptionFirst;
	pSelf->m_pVoteOptionLast = pVoteOptionLast;
	pSelf->m_NumVoteOptions = NumVoteOptions;
}

void CGameContext::ConForceVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pType = pResult->GetString(0);
	const char *pValue = pResult->GetString(1);
	const char *pReason = pResult->NumArguments() > 2 && pResult->GetString(2)[0] ? pResult->GetString(2) : "No reason given";
	char aBuf[128] = {0};

	if (str_comp_nocase(pType, "option") == 0)
	{
		CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
		while (pOption)
		{
			if (str_comp_nocase(pValue, pOption->m_aDescription) == 0)
			{
				str_format(aBuf, sizeof(aBuf), "admin forced server option '%s' (%s)", pValue, pReason);
				pSelf->SendChatTarget(-1, aBuf);
				pSelf->Console()->ExecuteLine(pOption->m_aCommand, -1);
				break;
			}

			pOption = pOption->m_pNext;
		}

		if (!pOption)
		{
			str_format(aBuf, sizeof(aBuf), "'%s' isn't an option on this server", pValue);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
	}
	else if (str_comp_nocase(pType, "kick") == 0)
	{
		int KickID = str_toint(pValue);
		if (KickID < 0 || KickID >= MAX_CLIENTS || !pSelf->m_apPlayers[KickID])
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to kick");
			return;
		}

		if (!g_Config.m_SvVoteKickBantime)
		{
			str_format(aBuf, sizeof(aBuf), "kick %d %s", KickID, pReason);
			pSelf->Console()->ExecuteLine(aBuf, -1);
		}
		else
		{
			char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
			pSelf->Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
			str_format(aBuf, sizeof(aBuf), "ban %s %d %s", aAddrStr, g_Config.m_SvVoteKickBantime, pReason);
			pSelf->Console()->ExecuteLine(aBuf, -1);
		}
	}
	else if (str_comp_nocase(pType, "spectate") == 0)
	{
		int SpectateID = str_toint(pValue);
		if (SpectateID < 0 || SpectateID >= MAX_CLIENTS || !pSelf->m_apPlayers[SpectateID] || pSelf->m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to move");
			return;
		}

		str_format(aBuf, sizeof(aBuf), "admin moved '%s' to spectator (%s)", pSelf->Server()->ClientName(SpectateID), pReason);
		pSelf->SendChatTarget(-1, aBuf);
		str_format(aBuf, sizeof(aBuf), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
		pSelf->Console()->ExecuteLine(aBuf, -1);
	}
}

void CGameContext::ConClearVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "cleared votes");
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	pSelf->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);
	pSelf->m_pVoteOptionHeap->Reset();
	pSelf->m_pVoteOptionFirst = 0;
	pSelf->m_pVoteOptionLast = 0;
	pSelf->m_NumVoteOptions = 0;
}

void CGameContext::ConVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	// check if there is a vote running
	if (!pSelf->m_VoteCloseTime)
		return;

	if (str_comp_nocase(pResult->GetString(0), "yes") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_YES;
	else if (str_comp_nocase(pResult->GetString(0), "no") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_NO;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "admin forced vote %s", pResult->GetString(0));
	pSelf->SendChatTarget(-1, aBuf);
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pResult->GetString(0));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CGameContext::ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if (pResult->NumArguments())
	{
		CNetMsg_Sv_Motd Msg;
		Msg.m_pMessage = g_Config.m_SvMotd;
		CGameContext *pSelf = (CGameContext *)pUserData;
		for (int i = 0; i < MAX_CLIENTS; ++i)
			if (pSelf->m_apPlayers[i] && !pSelf->IsBot(i))
				pSelf->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
	}
}

void CGameContext::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	Console()->Register("tune", "si", CFGFLAG_SERVER, ConTuneParam, this, "Tune variable to value");
	Console()->Register("tune_reset", "", CFGFLAG_SERVER, ConTuneReset, this, "Reset tuning");
	Console()->Register("tune_dump", "", CFGFLAG_SERVER, ConTuneDump, this, "Dump tuning");

	Console()->Register("pause", "", CFGFLAG_SERVER, ConPause, this, "Pause/unpause game");
	Console()->Register("change_map", "?r", CFGFLAG_SERVER | CFGFLAG_STORE, ConChangeMap, this, "Change map");
	Console()->Register("restart", "?i", CFGFLAG_SERVER | CFGFLAG_STORE, ConRestart, this, "Restart in x seconds (0 = abort)");
	Console()->Register("broadcast", "r", CFGFLAG_SERVER, ConBroadcast, this, "Broadcast message");
	Console()->Register("say", "r", CFGFLAG_SERVER, ConSay, this, "Say in chat");
	Console()->Register("set_team", "ii?i", CFGFLAG_SERVER, ConSetTeam, this, "Set team of player to team");
	Console()->Register("set_team_all", "i", CFGFLAG_SERVER, ConSetTeamAll, this, "Set team of all players to team");
	Console()->Register("swap_teams", "", CFGFLAG_SERVER, ConSwapTeams, this, "Swap the current teams");
	Console()->Register("shuffle_teams", "", CFGFLAG_SERVER, ConShuffleTeams, this, "Shuffle the current teams");
	Console()->Register("lock_teams", "", CFGFLAG_SERVER, ConLockTeams, this, "Lock/unlock teams");

	Console()->Register("add_vote", "sr", CFGFLAG_SERVER, ConAddVote, this, "Add a voting option");
	Console()->Register("remove_vote", "s", CFGFLAG_SERVER, ConRemoveVote, this, "remove a voting option");
	Console()->Register("force_vote", "ss?r", CFGFLAG_SERVER, ConForceVote, this, "Force a voting option");
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, ConClearVotes, this, "Clears the voting options");
	Console()->Register("vote", "r", CFGFLAG_SERVER, ConVote, this, "Force a vote to yes/no");

	Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);
}

void CGameContext::OnInit(/*class IKernel *pKernel*/)
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>(); // MapGen
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);

	for (int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel());
	m_Collision.Init(&m_Layers);
	m_MapGen.Init(&m_Layers, &m_Collision, m_pStorage); // MapGen

	// select gametype
	if (str_comp(g_Config.m_SvGametype, "coop") == 0)
		m_pController = new CGameControllerCoop(this);
	else if (str_comp(g_Config.m_SvGametype, "ctf") == 0)
		m_pController = new CGameControllerCTF(this);
	else if (str_comp(g_Config.m_SvGametype, "dom") == 0)
		m_pController = new CGameControllerDOM(this);
	else if (str_comp(g_Config.m_SvGametype, "tdm") == 0)
		m_pController = new CGameControllerTDM(this);
	else if (str_comp(g_Config.m_SvGametype, "cstt") == 0)
		m_pController = new CGameControllerCSTT(this);
	else if (str_comp(g_Config.m_SvGametype, "csbb") == 0)
		m_pController = new CGameControllerCSBB(this);
	else
		m_pController = new CGameControllerDM(this);

	if (g_Config.m_SvMapGen && !m_pServer->m_MapGenerated)
	{
		m_MapGen.FillMap();
		SaveMap("");

		str_copy(g_Config.m_SvMap, "generated", sizeof(g_Config.m_SvMap));
		m_pServer->m_MapGenerated = true;
	}

	// create all entities from the game layer
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data);

	for (int y = 0; y < pTileMap->m_Height; y++)
	{
		for (int x = 0; x < pTileMap->m_Width; x++)
		{
			int Index = pTiles[y * pTileMap->m_Width + x].m_Index;

			if (Index >= ENTITY_OFFSET)
			{
				vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
				m_pController->OnEntity(Index - ENTITY_OFFSET, Pos);
			}
		}
	}
}

/*
void CGameContext::SetupVotes(int ClientID)
{
	ResetVotes();
}
*/

enum VoteTypes
{
	VOTE_ALL,
	VOTE_WEAPON,
	VOTE_WEAPONDESC,
	VOTE_MONEY,
	VOTE_CLASS,
	VOTE_CLASSDESC,
	VOTE_ABILITY,
	VOTE_ABILITYPOINTS,
	VOTE_ABILITYDESC,
};

void CGameContext::ResetVotes()
{
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);

	m_pVoteOptionHeap->Reset();
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;

	if (g_Config.m_SvAbilities)
	{
		AddCustomVote("PICK A CLASS", "null", VOTE_CLASSDESC);
		for (int i = 0; i < NUM_CLASSES; i++)
			AddCustomVote(aClassName[i], aClassName[i], VOTE_CLASS, i);
		AddCustomVote("", "null", VOTE_CLASSDESC);

		AddCustomVote("", "null", VOTE_ABILITYPOINTS); // Pick abilities - x point(s) left
		for (int i = 0; i < NUM_ABILITIES; i++)
			AddCustomVote(aAbilities[i].m_aName, aAbilities[i].m_aName, VOTE_ABILITY, i);
		AddCustomVote("", "null", VOTE_ABILITYDESC);
	}

	if (!g_Config.m_SvRandomWeapons)
	{
		AddCustomVote("", "null", VOTE_MONEY); // Money: x
		// AddCustomVote("", "null", VOTE_WEAPONDESC);

		// AddCustomVote("Buy & Upgrade:", "null", VOTE_WEAPONDESC);

		for (int i = 0; i < NUM_CUSTOMWEAPONS; i++)
		{
			if (aCustomWeapon[i].m_Cost > 0)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "%s - ♪%d", aCustomWeapon[i].m_Name, aCustomWeapon[i].m_Cost);

				AddCustomVote(aBuf, aCustomWeapon[i].m_BuyCmd, VOTE_WEAPON, i);
			}
		}
	}
}

void CGameContext::ClearShopVotes(int ClientID)
{
	if (str_comp(g_Config.m_SvGametype, "csbb") == 0)
		return;

	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, ClientID);

	/*
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "Can't shop right now");

	CNetMsg_Sv_VoteOptionAdd OptionMsg;
	OptionMsg.m_pDescription = aBuf;
	Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
	*/
}

void CGameContext::AddCustomVote(const char *Desc, const char *Cmd, int Type, int WeaponIndex)
{
	if (m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pseudovote", "ERROR - MAX_VOTE_OPTIONS REACHED! (did you really reach 128?)");
		return;
	}

	// check for valid option"
	if (str_length(Cmd) >= VOTE_CMD_LENGTH)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", Cmd);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pseudovote", aBuf);
		return;
	}
	while (*Desc && *Desc == ' ')
		Desc++;

	CVoteOptionServer *pOption = m_pVoteOptionFirst;

	// add the option
	++this->m_NumVoteOptions;
	int Len = str_length(Cmd);

	pOption = (CVoteOptionServer *)this->m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
	pOption->m_pNext = 0;
	pOption->m_pPrev = m_pVoteOptionLast;
	if (pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	m_pVoteOptionLast = pOption;
	if (!m_pVoteOptionFirst)
		m_pVoteOptionFirst = pOption;

	str_copy(pOption->m_aDescription, Desc, sizeof(pOption->m_aDescription));
	mem_copy(pOption->m_aCommand, Cmd, Len + 1);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "added option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	// Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pseudovote", aBuf);

	if (Type == VOTE_MONEY)
	{
		CNetMsg_Sv_VoteOptionAdd OptionMsg;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i] && !IsBot(i))
			{
				char aBuf[256];

				if (m_apPlayers[i]->m_CanShop || str_comp(g_Config.m_SvGametype, "csbb") == 0)
					str_format(aBuf, sizeof(aBuf), "WEAPON SHOP  -  money: ♪%d ", m_apPlayers[i]->m_Money);
				else
					str_format(aBuf, sizeof(aBuf), "Can't shop right now");

				str_copy(pOption->m_aDescription, aBuf, sizeof(pOption->m_aDescription));

				OptionMsg.m_pDescription = pOption->m_aDescription;
				Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, i);
			}
		}
	}

	if (Type == VOTE_WEAPON)
	{
		CNetMsg_Sv_VoteOptionAdd OptionMsg;
		OptionMsg.m_pDescription = pOption->m_aDescription;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i] && !IsBot(i))
			{
				if (!m_apPlayers[i]->m_CanShop && str_comp(g_Config.m_SvGametype, "cstt") == 0)
					continue;

				if (m_apPlayers[i]->BuyableWeapon(WeaponIndex))
				{
					Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, i);
				}
			}
		}
	}

	// buy & upgrade text
	if (Type == VOTE_WEAPONDESC)
	{
		CNetMsg_Sv_VoteOptionAdd OptionMsg;
		OptionMsg.m_pDescription = pOption->m_aDescription;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i] && !IsBot(i))
			{
				if (!m_apPlayers[i]->m_CanShop && str_comp(g_Config.m_SvGametype, "cstt") == 0)
					continue;

				Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, i);
			}
		}
	}

	if (Type == VOTE_CLASSDESC || Type == VOTE_CLASS)
	{
		CNetMsg_Sv_VoteOptionAdd OptionMsg;
		OptionMsg.m_pDescription = pOption->m_aDescription;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i] && !IsBot(i))
			{
				if (m_apPlayers[i]->GetClass() != -1)
					continue;

				Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, i);
			}
		}
	}

	if (Type == VOTE_ABILITYDESC)
	{
		CNetMsg_Sv_VoteOptionAdd OptionMsg;
		OptionMsg.m_pDescription = pOption->m_aDescription;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i] && !IsBot(i))
			{
				if (m_apPlayers[i]->GetClass() == -1 || !m_apPlayers[i]->GetAbilityPoints())
					continue;

				Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, i);
			}
		}
	}

	if (Type == VOTE_ABILITYPOINTS)
	{
		CNetMsg_Sv_VoteOptionAdd OptionMsg;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i] && !IsBot(i))
			{
				int c = m_apPlayers[i]->GetClass();

				if (c == -1)
					continue;

				int Points = m_apPlayers[i]->GetAbilityPoints();

				if (!Points)
					continue;

				char aBuf[256];

				if (Points > 1)
					str_format(aBuf, sizeof(aBuf), "PICK ABILITIES - %d skill points left", Points);
				else
					str_format(aBuf, sizeof(aBuf), "PICK ABILITIES - 1 skill point left");

				str_copy(pOption->m_aDescription, aBuf, sizeof(pOption->m_aDescription));

				OptionMsg.m_pDescription = pOption->m_aDescription;
				Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, i);
			}
		}
	}

	if (Type == VOTE_ABILITY)
	{
		CNetMsg_Sv_VoteOptionAdd OptionMsg;
		OptionMsg.m_pDescription = pOption->m_aDescription;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i] && !IsBot(i))
			{
				int c = m_apPlayers[i]->GetClass();
				if (c == -1)
					continue;

				if (!m_apPlayers[i]->GetAbilityPoints() || !m_apPlayers[i]->AbilityAvailable(WeaponIndex))
					continue;

				Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, i);
			}
		}
	}

	if (Type == VOTE_ALL)
	{
		CNetMsg_Sv_VoteOptionAdd OptionMsg;
		OptionMsg.m_pDescription = pOption->m_aDescription;
		Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);
	}
}

void CGameContext::AddVote(const char *Desc, const char *Cmd, int ClientID)
{
	if (m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pseudovote", "ERROR - MAX_VOTE_OPTIONS REACHED! (did you really reach 128?)");
		return;
	}

	// check for valid option"
	if (/*!pSelf->Console()->LineIsValid(pCommand) ||  */ str_length(Cmd) >= VOTE_CMD_LENGTH)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", Cmd);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pseudovote", aBuf);
		return;
	}
	while (*Desc && *Desc == ' ')
		Desc++;
	/*if(str_length(Desc) >= VOTE_DESC_LENGTH || *Desc == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid option '%s'", Desc);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pseudovote", aBuf);
		return;
	}*/

	// Shall we keep this? -kiChris // no need - Kompl.exe
	//  check for duplicate entry
	CVoteOptionServer *pOption = m_pVoteOptionFirst;
	/*while(pOption)
	{
		if(str_comp_nocase(Desc, pOption->m_aDescription) == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "option '%s' already exists", Desc);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pseudovote", aBuf);
			return;
		}
		pOption = pOption->m_pNext;
	}*/

	// add the option
	++this->m_NumVoteOptions;
	int Len = str_length(Cmd);

	pOption = (CVoteOptionServer *)this->m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
	pOption->m_pNext = 0;
	pOption->m_pPrev = m_pVoteOptionLast;
	if (pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	m_pVoteOptionLast = pOption;
	if (!m_pVoteOptionFirst)
		m_pVoteOptionFirst = pOption;

	str_copy(pOption->m_aDescription, Desc, sizeof(pOption->m_aDescription));
	mem_copy(pOption->m_aCommand, Cmd, Len + 1);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "added option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	// Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pseudovote", aBuf);

	if (ClientID == -2)
		return;
	// inform clients about added option
	CNetMsg_Sv_VoteOptionAdd OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::OnShutdown()
{
	KickBots();
	if (str_comp(m_pController->m_pGameType, "coop") == 0)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
			if (m_apPlayers[i])
				m_apPlayers[i]->SaveData();
	}
	delete m_pController;
	m_pController = 0;
	Clear();
}

void CGameContext::OnSnap(int ClientID)
{
	// add tuning to demo
	CTuningParams StandardTuning;
	if (ClientID == -1 && Server()->DemoRecorder_IsRecording() && mem_comp(&StandardTuning, &m_Tuning, sizeof(CTuningParams)) != 0)
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		int *pParams = (int *)&m_Tuning;
		for (unsigned i = 0; i < sizeof(m_Tuning) / sizeof(int); i++)
			Msg.AddInt(pParams[i]);
		Server()->SendMsg(&Msg, MSGFLAG_RECORD | MSGFLAG_NOSEND, ClientID);
	}

	m_World.Snap(ClientID);
	m_pController->Snap(ClientID);
	m_Events.Snap(ClientID);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_apPlayers[i])
			m_apPlayers[i]->Snap(ClientID);
	}
}
void CGameContext::OnPreSnap() {}
void CGameContext::OnPostSnap()
{
	m_Events.Clear();
}

bool CGameContext::IsClientReady(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsReady ? true : false;
}

bool CGameContext::IsClientPlayer(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS ? false : true;
}

const char *CGameContext::GameType() { return m_pController && m_pController->m_pGameType ? m_pController->m_pGameType : ""; }
const char *CGameContext::Version() { return GAME_VERSION; }
const char *CGameContext::NetVersion() { return GAME_NETVERSION; }

IGameServer *CreateGameServer() { return new CGameContext; }

void CGameContext::KickBots()
{
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "engine", "Kicking bots...");

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (IsBot(i))
			Server()->Kick(i, "");
	}
}

void CGameContext::KickBot(int ClientID)
{
	if (IsBot(ClientID))
		Server()->Kick(ClientID, "");
}

void CGameContext::AddBot()
{
	Server()->AddZombie();
}

// MapGen
void CGameContext::SaveMap(const char *path)
{
	IMap *pMap = Layers()->Map();
	if (!pMap)
		return;

	CDataFileWriter fileWrite;
	char aMapFile[512];
	// str_format(aMapFile, sizeof(aMapFile), "maps/%s_%d.map", Server()->GetMapName(), g_Config.m_SvMapGenSeed);
	str_format(aMapFile, sizeof(aMapFile), "maps/generated.map");

	// Map will be saved to current dir, not to ~/.ninslash/maps or to data/maps, so we need to create a dir for it
	Storage()->CreateFolder("maps", IStorage::TYPE_SAVE);

	fileWrite.SaveMap(Storage(), pMap->GetFileReader(), aMapFile);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Map saved in '%s'!", aMapFile);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CGameContext::GenerateMap()
{
	m_MapGen.FillMap();
	SaveMap("");

	str_copy(g_Config.m_SvMap, "generated", sizeof(g_Config.m_SvMap));
	m_pServer->m_MapGenerated = true;
}

void CGameContext::ReloadMap()
{
	Console()->ExecuteLine("reload", -1);
}

void CGameContext::OnSetAuthed(int ClientID, int Level)
{
	if (m_apPlayers[ClientID])
		m_apPlayers[ClientID]->m_Authed = Level;
}

const char *CGameContext::Localize(const char *pLanguageCode, const char *pText)
{
	return Server()->Localization()->Localize(pLanguageCode, pText);
}