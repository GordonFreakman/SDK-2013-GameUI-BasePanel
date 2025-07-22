//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASEPANEL_H
#define BASEPANEL_H
#ifdef _WIN32
#pragma once
#endif

// #define PROPORTIONAL_BASEPANEL

#include "vgui_controls/Panel.h"
#include "vgui_controls/PHandle.h"
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/messagedialog.h"
#include "KeyValues.h"
#include "UtlVector.h"
#include "tier1/CommandBuffer.h"
#include "GameUI/IGameUI.h"
extern float m_flWaitBeforePaint;
class CMatchmakingBasePanel;
class CBackgroundMenuButton;
class CGameMenu;
//-----------------------------------------------------------------------------
// Purpose: EditablePanel that can replace the GameMenuButtons in CBasePanel
//-----------------------------------------------------------------------------
class CMainMenuGameLogo : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CMainMenuGameLogo, vgui::EditablePanel );
public:
	CMainMenuGameLogo( vgui::Panel *parent, const char *name );

	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	int GetOffsetX(){ return m_nOffsetX; }
	int GetOffsetY(){ return m_nOffsetY; }

	bool IsLoading()
	{
		return (m_flWaitBeforePaint > Plat_FloatTime() || engine->IsDrawingLoadingImage());
	};

	bool IsVisible()
	{
		if (IsLoading())
			return false;
		return BaseClass::IsVisible();
	};

private:
	int m_nOffsetX;
	int m_nOffsetY;
};
//-----------------------------------------------------------------------------
// Purpose: Transparent menu item designed to sit on the background ingame
//-----------------------------------------------------------------------------
class CGameMenuItem : public vgui::MenuItem
{
	DECLARE_CLASS_SIMPLE( CGameMenuItem, vgui::MenuItem );
public: 
	CGameMenuItem(vgui::Menu *parent, const char *name);

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground( void );
	void SetRightAlignedText( bool state );

	bool IsLoading()
	{
		return (m_flWaitBeforePaint > Plat_FloatTime() || engine->IsDrawingLoadingImage());
	};

	bool IsVisible()
	{
		if (IsLoading())
			return false;
		return BaseClass::IsVisible();
	};

private:
	bool		m_bRightAligned;
};

//-----------------------------------------------------------------------------
// Purpose: This is the panel at the top of the panel hierarchy for GameUI
//			It handles all the menus, background images, and loading dialogs
//-----------------------------------------------------------------------------
class CBaseModPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE(CBaseModPanel, vgui::Panel);

public:
	CBaseModPanel(vgui::VPANEL parent);
	virtual ~CBaseModPanel();

	// update the taskbar a frame
	void RunFrame();

	// fades to black then runs an engine command (usually to start a level)
	void FadeToBlackAndRunEngineCommand( const char *engineCommand );

	// sets the blinking state of a menu item
	void SetMenuItemBlinkingState( const char *itemName, bool state );

	// handles gameUI being shown
	IGameUI*		GetGameUI();

	// forces any changed options dialog settings to be applied immediately, if it's open
	void ApplyOptionsDialogSettings();

	void PositionDialog( vgui::PHandle dlg );

	virtual void OnSizeChanged( int newWide, int newTall );

	void ArmFirstMenuItem( void );

	void OnGameUIHidden();

protected:
	virtual void PaintBackground();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:

	// sets the menu alpha [0..255]
	void SetMenuAlpha(int alpha);

	bool			LoadGameUI();

	IGameUI*		gameui;

	// menu manipulation
	void CreateGameMenu();
	void CreateGameLogo();
	void UpdateGameMenus();
	CGameMenu *RecursiveLoadGameMenu(KeyValues *datafile);

	virtual void OnCommand(const char *command);
	virtual void PerformLayout();

	// menu logo
	CMainMenuGameLogo *m_pGameLogo;
	
	// menu buttons
	CUtlVector< CBackgroundMenuButton * >m_pGameMenuButtons;
	CGameMenu *m_pGameMenu;
	bool m_bPlatformMenuInitialized;
	int m_iGameMenuInset;

	struct coord {
		int x;
		int y;
	};
	CUtlVector< coord > m_iGameTitlePos;
	coord m_iGameMenuPos;

	// background transition
	bool m_bFadingInMenus;
	float m_flFadeMenuStartTime;
	float m_flFadeMenuEndTime;

	bool m_bRenderingBackgroundTransition;
	float m_flTransitionStartTime;
	float m_flTransitionEndTime;

	// background fill transition
	bool m_bHaveDarkenedBackground;
	bool m_bHaveDarkenedTitleText;
	bool m_bForceTitleTextUpdate;
	float m_flFrameFadeInTime;
	Color m_BackdropColor;
	CPanelAnimationVar( float, m_flBackgroundFillAlpha, "m_flBackgroundFillAlpha", "0" );
};

namespace vgui
{
	class Panel;
}

class IBaseModPanelInterface
{
public:
	virtual void		Create(vgui::VPANEL parent) = 0;
	virtual vgui::VPANEL	GetPanel(void) = 0;
	virtual void		Destroy(void) = 0;
};

extern IBaseModPanelInterface *BaseModPanel;

extern CBaseModPanel	*g_pBaseModPanel;

#endif // BASEPANEL_H
