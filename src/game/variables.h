/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VARIABLES_H
#define GAME_VARIABLES_H
#undef GAME_VARIABLES_H // this file will be included several times


// client
MACRO_CONFIG_INT(ClPredict, cl_predict, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Predict client movements")
MACRO_CONFIG_INT(ClNameplates, cl_nameplates, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show name plates")
MACRO_CONFIG_INT(ClNameplatesAlways, cl_nameplates_always, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Always show name plates disregarding of distance")
MACRO_CONFIG_INT(ClNameplatesTeamcolors, cl_nameplates_teamcolors, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use team colors for name plates")
MACRO_CONFIG_INT(ClNameplatesSize, cl_nameplates_size, 50, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Size of the name plates from 0 to 100%")
MACRO_CONFIG_INT(ClAutoswitchWeapons, cl_autoswitch_weapons, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Auto switch weapon on pickup")

MACRO_CONFIG_INT(ClShowhud, cl_showhud, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame HUD")
MACRO_CONFIG_INT(ClShowChatFriends, cl_show_chat_friends, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show only chat messages from friends")
MACRO_CONFIG_INT(ClShowfps, cl_showfps, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame FPS counter")

MACRO_CONFIG_INT(ClAirjumpindicator, cl_airjumpindicator, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClThreadsoundloading, cl_threadsoundloading, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Load sound files threaded")

MACRO_CONFIG_INT(ClWarningTeambalance, cl_warning_teambalance, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Warn about team balance")

MACRO_CONFIG_INT(ClMouseDeadzone, cl_mouse_deadzone, 300, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMouseFollowfactor, cl_mouse_followfactor, 60, 0, 200, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMouseMaxDistance, cl_mouse_max_distance, 800, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(EdShowkeys, ed_showkeys, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

//MACRO_CONFIG_INT(ClFlow, cl_flow, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(ClShowWelcome, cl_show_welcome, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMotdTime, cl_motd_time, 10, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "How long to show the server message of the day")

MACRO_CONFIG_STR(ClVersionServer, cl_version_server, 100, "version.teeworlds.com", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Server to use to check for new versions")

MACRO_CONFIG_STR(ClLanguagefile, cl_languagefile, 255, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "What language file to use")

MACRO_CONFIG_INT(PlayerUseCustomColor, player_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors")
MACRO_CONFIG_INT(PlayerColorBody, player_color_body, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player body color")
MACRO_CONFIG_INT(PlayerColorFeet, player_color_feet, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player feet color")
MACRO_CONFIG_STR(PlayerSkin, player_skin, 24, "default", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin")

MACRO_CONFIG_INT(UiPage, ui_page, 6, 0, 10, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface page")
MACRO_CONFIG_INT(UiToolboxPage, ui_toolbox_page, 0, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toolbox page")
MACRO_CONFIG_STR(UiServerAddress, ui_server_address, 64, "localhost:8303", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface server address")
MACRO_CONFIG_INT(UiScale, ui_scale, 100, 50, 150, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface scale")
MACRO_CONFIG_INT(UiMousesens, ui_mousesens, 100, 5, 100000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Mouse sensitivity for menus/editor")

MACRO_CONFIG_INT(UiColorHue, ui_color_hue, 160, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface color hue")
MACRO_CONFIG_INT(UiColorSat, ui_color_sat, 70, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface color saturation")
MACRO_CONFIG_INT(UiColorLht, ui_color_lht, 175, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface color lightness")
MACRO_CONFIG_INT(UiColorAlpha, ui_color_alpha, 228, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface alpha")

MACRO_CONFIG_INT(GfxNoclip, gfx_noclip, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Disable clipping")


// custom engine settings

MACRO_CONFIG_INT(SvRandomWeapons, sv_randomweapons, 0, 0, 1, CFGFLAG_SERVER, "Enable random weapons")
MACRO_CONFIG_INT(SvSurvivalMode, sv_survivalmode, 0, 0, 1, CFGFLAG_SERVER, "Survival mode")
MACRO_CONFIG_INT(SvAbilities, sv_abilities, 0, 0, 1, CFGFLAG_SERVER, "Enable classes & abilities")
MACRO_CONFIG_INT(SvPickupDrops, sv_pickupdrops, 1, 0, 1, CFGFLAG_SERVER, "Pickup drops")
MACRO_CONFIG_INT(SvVanillaPickups, sv_vanillapickups, 1, 0, 1, CFGFLAG_SERVER, "Enable vanilla's pickups")
MACRO_CONFIG_INT(SvInstaDeathTiles, sv_instadeathtiles, 0, 0, 1, CFGFLAG_SERVER, "Instakill death tiles")
MACRO_CONFIG_INT(SvWeaponDrops, sv_weapondrops, 1, 0, 1, CFGFLAG_SERVER, "Enable weapon drops")
MACRO_CONFIG_INT(SvPreferredTeamSize, sv_preferredteamsize, 5, 1, 8, CFGFLAG_SERVER, "Preferred team size")

MACRO_CONFIG_INT(SvHammerFight, sv_hammerfight, 0, 0, 1, CFGFLAG_SERVER, "Thunder hammer as a starting weapon")



// CS:TT specific
MACRO_CONFIG_INT(SvNumRounds, sv_numrounds, 7, 1, 100, CFGFLAG_SERVER, "Number of rounds")

MACRO_CONFIG_INT(SvStartMoney, sv_startmoney, 200, 0, 1000, CFGFLAG_SERVER, "Start money")
MACRO_CONFIG_INT(SvMaxMoney, sv_maxmoney, 500, 0, 10000, CFGFLAG_SERVER, "Max money")
MACRO_CONFIG_INT(SvKillMoney, sv_killmoney, 20, 0, 1000, CFGFLAG_SERVER, "Kill money")
MACRO_CONFIG_INT(SvWinMoney, sv_winmoney, 100, 0, 1000, CFGFLAG_SERVER, "Win money")
MACRO_CONFIG_INT(SvLoseMoney, sv_losemoney, 50, 0, 1000, CFGFLAG_SERVER, "Lose money")

MACRO_CONFIG_INT(SvRoundTime, sv_roundtime, 60, 0, 1000, CFGFLAG_SERVER, "Round time limit (seconds)")
MACRO_CONFIG_INT(SvPreroundTime, sv_preroundtime, 5, 2, 20, CFGFLAG_SERVER, "Pre-round time (seconds)")

MACRO_CONFIG_INT(SvBombTime, sv_bombtime, 20, 10, 120, CFGFLAG_SERVER, "Bomb time (seconds)")
MACRO_CONFIG_INT(SvBombPlantTime, sv_bombplanttime, 3, 2, 20, CFGFLAG_SERVER, "Time bomb planting takes (seconds)")
MACRO_CONFIG_INT(SvBombDefuseTime, sv_bombdefusetime, 4, 2, 20, CFGFLAG_SERVER, "Time bomb defusing takes (seconds)")

MACRO_CONFIG_INT(SvSpectatorUpdateTime, sv_spectatorupdatetime, 2, 1, 20, CFGFLAG_SERVER, "Time between spectator view changes to spectators")

MACRO_CONFIG_INT(SvRespawnDelayCSTT, sv_respawn_delay_cstt, 5, 0, 20, CFGFLAG_SERVER, "Time needed to respawn after death in cstt gametype")


MACRO_CONFIG_INT(SvBroadcastLock, sv_broadcastlock, 3, 0, 5, CFGFLAG_SERVER, "Broadcast lock time (seconds)")

// CS:BB
MACRO_CONFIG_INT(SvBaseCaptureDistance, sv_basecapturedistance, 200, 50, 2000, CFGFLAG_SERVER, "Base capture distance")
MACRO_CONFIG_INT(SvBaseCaptureTime, sv_basecapturetime, 3, 1, 60, CFGFLAG_SERVER, "Base capture time (seconds)")

MACRO_CONFIG_INT(SvRespawnDelayCSBB, sv_respawn_delay_csbb, 3, 0, 10, CFGFLAG_SERVER, "Time needed to respawn after death in csbb gametype")

// Domination++
MACRO_CONFIG_INT(SvBaseCaptureTreshold, sv_basecapturetreshold, 100, 50, 1500, CFGFLAG_SERVER, "Base capture time(ish)")



// AI
MACRO_CONFIG_INT(SvBotReactTime, sv_bot_react_time, 6, 0, 20, CFGFLAG_SERVER, "Time bot takes to start shooting")



MACRO_CONFIG_INT(SvWarmup, sv_warmup, 0, 0, 0, CFGFLAG_SERVER, "Number of seconds to do warmup before round starts")
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SERVER, "Message of the day to display for the clients")
MACRO_CONFIG_INT(SvTeamdamage, sv_teamdamage, 0, 0, 1, CFGFLAG_SERVER, "Team damage")
MACRO_CONFIG_STR(SvMaprotation, sv_maprotation, 768, "", CFGFLAG_SERVER, "Maps to rotate between")
MACRO_CONFIG_INT(SvRoundsPerMap, sv_rounds_per_map, 1, 1, 100, CFGFLAG_SERVER, "Number of rounds on each map before rotating")
MACRO_CONFIG_INT(SvRoundSwap, sv_round_swap, 1, 0, 1, CFGFLAG_SERVER, "Swap teams between rounds")
MACRO_CONFIG_INT(SvPowerups, sv_powerups, 1, 0, 1, CFGFLAG_SERVER, "Allow powerups like ninja")
MACRO_CONFIG_INT(SvScorelimit, sv_scorelimit, 0, 0, 1000, CFGFLAG_SERVER, "Score limit (0 disables)")
MACRO_CONFIG_INT(SvTimelimit, sv_timelimit, 0, 0, 1000, CFGFLAG_SERVER, "Time limit in minutes (0 disables)")
MACRO_CONFIG_STR(SvGametype, sv_gametype, 32, "cstt", CFGFLAG_SERVER, "Game type (dm, tdm, ctf)")
MACRO_CONFIG_INT(SvTournamentMode, sv_tournament_mode, 0, 0, 1, CFGFLAG_SERVER, "Tournament mode. When enabled, players joins the server as spectator")
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SERVER, "Spam protection")

MACRO_CONFIG_INT(SvRespawnDelayTDM, sv_respawn_delay_tdm, 3, 0, 10, CFGFLAG_SERVER, "Time needed to respawn after death in tdm gametype")

MACRO_CONFIG_INT(SvSpectatorSlots, sv_spectator_slots, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Number of slots to reserve for spectators")
MACRO_CONFIG_INT(SvTeambalanceTime, sv_teambalance_time, 1, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before autobalancing teams")
MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 5, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before taking care of inactive players")
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 1, 0, 2, CFGFLAG_SERVER, "How to deal with inactive players (0=move to spectator, 1=move to free spectator slot/kick, 2=kick)")

MACRO_CONFIG_INT(SvStrictSpectateMode, sv_strict_spectate_mode, 0, 0, 1, CFGFLAG_SERVER, "Restricts information in spectator mode")
MACRO_CONFIG_INT(SvVoteSpectate, sv_vote_spectate, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to move players to spectators")
MACRO_CONFIG_INT(SvVoteSpectateRejoindelay, sv_vote_spectate_rejoindelay, 1, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before a player can rejoin after being moved to spectators by vote")
MACRO_CONFIG_INT(SvVoteKick, sv_vote_kick, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to kick players")
MACRO_CONFIG_INT(SvVoteKickMin, sv_vote_kick_min, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Minimum number of players required to start a kick vote")
MACRO_CONFIG_INT(SvVoteKickBantime, sv_vote_kick_bantime, 5, 0, 1440, CFGFLAG_SERVER, "The time to ban a player if kicked by vote. 0 makes it just use kick")

// MapGen
MACRO_CONFIG_INT(SvMapGen, sv_mapgen, 1, 0, 1, CFGFLAG_SERVER, "Map Generation Status")
MACRO_CONFIG_INT(SvMapGenLevel, sv_mapgen_level, 1, 1, 9999, CFGFLAG_SERVER, "Map Difficulty")
MACRO_CONFIG_INT(SvMapGenSeed, sv_mapgen_seed, 0, 0, 32767, CFGFLAG_SERVER, "Map generation seed")
MACRO_CONFIG_INT(SvMapGenRandSeed, sv_mapgen_random_seed, 1, 0, 1, CFGFLAG_SERVER, "Random map generation seed")

// Invasion
MACRO_CONFIG_INT(SvInvFails, sv_inv_fails,  0, 0, 9, CFGFLAG_SERVER, "Invasion level fails")
MACRO_CONFIG_INT(SvInvBosses, sv_inv_bosses,  0, 0, 99, CFGFLAG_SERVER, "Invasion level bosses")
MACRO_CONFIG_STR(SvInvMap, sv_dont_use_j92zjhgsabkjznbdatka9j, 128, "", CFGFLAG_SERVER, "Latest invasion map")
MACRO_CONFIG_INT(SvInvGroupLeft, sv_dont_use_j92213gyahjbd837tka9j, 0, 0, 9999999, CFGFLAG_SERVER, "Latest invasion map")
MACRO_CONFIG_INT(SvInvELeft, sv_dont_use_j92tka9zi8ywhsadhi87yihvxmcnzj, 0, 0, 9999999, CFGFLAG_SERVER, "Latest invasion map")

MACRO_CONFIG_INT(SvBotLevel, sv_bot_level, 40, 1, 9999, CFGFLAG_SERVER, "Bot level")

// debug
#ifdef CONF_DEBUG // this one can crash the server if not used correctly
	MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, 15, CFGFLAG_SERVER, "")
#endif

MACRO_CONFIG_INT(DbgFocus, dbg_focus, 0, 0, 1, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(DbgTuning, dbg_tuning, 0, 0, 1, CFGFLAG_CLIENT, "")

#endif
