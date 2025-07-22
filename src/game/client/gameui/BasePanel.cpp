//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//


#include "cbase.h"
#include "BasePanel.h"
#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "filesystem.h"

using namespace vgui;

#include "ModInfo.h"

#include "igameuifuncs.h"
#include "vgui_controls/AnimationController.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/PHandle.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/QueryBox.h"
#include "vgui_controls/ControllerMap.h"
#include "vgui_controls/KeyRepeat.h"
#include "tier0/icommandline.h"
#include "tier1/convar.h"
#include "OptionsDialog.h"
#include "BackgroundMenuButton.h"
#include "engine/IEngineSound.h"
#include "bitbuf.h"
#include "tier1/fmtstr.h"
#include "inputsystem/iinputsystem.h"
#include "ixboxsystem.h"
#include "OptionsSubAudio.h"
#include "hl2orange.spa.h"
#include "iachievementmgr.h"
#if defined( _X360 )
#include "xbox/xbox_launch.h"
#else
#include "xbox/xboxstubs.h"
#endif

#include "../engine/imatchmaking.h"
#include "tier1/UtlString.h"
#include "steam/steam_api.h"
#undef MessageBox	// Windows helpfully #define's this to MessageBoxA, we're using vgui::MessageBox

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CDllDemandLoader g_BPGameUIDLL("GameUI");

float m_flWaitBeforePaint = FLT_MAX;

ConVar vgui_message_dialog_modal( "vgui_message_dialog_modal", "1", FCVAR_ARCHIVE );

CBaseModPanel	*g_pBaseModPanel = NULL;
static float		g_flAnimationPadding = 0.01f;


extern const char *COM_GetModDirectory( void );

extern ConVar x360_audio_english;

CGameMenuItem::CGameMenuItem(vgui::Menu *parent, const char *name)  : BaseClass(parent, name, "GameMenuItem") 
{
	m_bRightAligned = false;
}

void CGameMenuItem::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// make fully transparent
	SetFgColor(GetSchemeColor("MainMenu.TextColor", pScheme));
	SetBgColor(Color(0, 0, 0, 0));
	SetDefaultColor(GetSchemeColor("MainMenu.TextColor", pScheme), Color(0, 0, 0, 0));
	SetArmedColor(GetSchemeColor("MainMenu.ArmedTextColor", pScheme), Color(0, 0, 0, 0));
	SetDepressedColor(GetSchemeColor("MainMenu.DepressedTextColor", pScheme), Color(0, 0, 0, 0));
	SetContentAlignment(Label::a_west);
	SetBorder(NULL);
	SetDefaultBorder(NULL);
	SetDepressedBorder(NULL);
	SetKeyFocusBorder(NULL);

	vgui::HFont hMainMenuFont = pScheme->GetFont( "MainMenuFont", IsProportional() );

	if ( hMainMenuFont )
	{
		SetFont( hMainMenuFont );
	}
	else
	{
		SetFont( pScheme->GetFont( "MenuLarge", IsProportional() ) );
	}
	SetTextInset(0, 0);
	SetArmedSound("UI/buttonrollover.wav");
	SetDepressedSound("UI/buttonclick.wav");
	SetReleasedSound("UI/buttonclickrelease.wav");
	SetButtonActivationType(Button::ACTIVATE_ONPRESSED);

	if (m_bRightAligned)
	{
		SetContentAlignment(Label::a_east);
	}
}

void CGameMenuItem::PaintBackground()
{
	BaseClass::PaintBackground();
}
void CGameMenuItem::SetRightAlignedText(bool state)
{
	m_bRightAligned = state;
}

//-----------------------------------------------------------------------------
// Purpose: General purpose 1 of N menu
//-----------------------------------------------------------------------------
class CGameMenu : public vgui::Menu
{
	DECLARE_CLASS_SIMPLE( CGameMenu, vgui::Menu );

public:
	CGameMenu(vgui::Panel *parent, const char *name) : BaseClass(parent, name) 
	{
#ifdef PROPORTIONAL_BASEPANEL
		SetProportional(true);
#endif
	}

	virtual void ApplySchemeSettings(IScheme *pScheme)
	{
		BaseClass::ApplySchemeSettings(pScheme);

		// make fully transparent
#ifdef PROPORTIONAL_BASEPANEL
		SetMenuItemHeight(scheme()->GetProportionalScaledValue(atoi(pScheme->GetResourceString("MainMenu.MenuItemHeight"))));
#else
		SetMenuItemHeight(atoi(pScheme->GetResourceString("MainMenu.MenuItemHeight")));
#endif
		SetBgColor(Color(0, 0, 0, 0));
		SetBorder(NULL);
	}

	virtual void LayoutMenuBorder()
	{
	}

	virtual void SetVisible(bool state)
	{
		// force to be always visible
		BaseClass::SetVisible(true);
		// move us to the back instead of going invisible
		if (!state)
		{
			ipanel()->MoveToBack(GetVPanel());
		}
	}

	virtual int AddMenuItem(const char *itemName, const char *itemText, const char *command, Panel *target, KeyValues *userData = NULL)
	{
		MenuItem *item = new CGameMenuItem(this, itemName);
#ifdef PROPORTIONAL_BASEPANEL
		item->SetProportional(true);
#endif
		item->AddActionSignalTarget(target);
		item->SetCommand(command);
		item->SetText(itemText);
		item->SetUserData(userData);
		return BaseClass::AddMenuItem(item);
	}

	virtual int AddMenuItem(const char *itemName, const char *itemText, KeyValues *command, Panel *target, KeyValues *userData = NULL)
	{
		CGameMenuItem *item = new CGameMenuItem(this, itemName);
#ifdef PROPORTIONAL_BASEPANEL
		item->SetProportional(true);
#endif
		item->AddActionSignalTarget(target);
		item->SetCommand(command);
		item->SetText(itemText);
		item->SetRightAlignedText(true);
		item->SetUserData(userData);
		return BaseClass::AddMenuItem(item);
	}

	virtual void SetMenuItemBlinkingState( const char *itemName, bool state )
	{
		for (int i = 0; i < GetChildCount(); i++)
		{
			Panel *child = GetChild(i);
			MenuItem *menuItem = dynamic_cast<MenuItem *>(child);
			if (menuItem)
			{
				if ( Q_strcmp( menuItem->GetCommand()->GetString("command", ""), itemName ) == 0 )
				{
					menuItem->SetBlink( state );
				}
			}
		}
		InvalidateLayout();
	}

	virtual void OnCommand(const char *command)
	{
		m_KeyRepeat.Reset();

		if (!stricmp(command, "Open"))
		{
			MoveToFront();
			RequestFocus();
		}
		else
		{
			BaseClass::OnCommand(command);
		}
	}

	virtual void OnKeyCodePressed( KeyCode code )
	{

		m_KeyRepeat.KeyDown( code );

		BaseClass::OnKeyCodePressed( code );

		// HACK: Allow F key bindings to operate even here
		if ( IsPC() && code >= KEY_F1 && code <= KEY_F12 )
		{
			// See if there is a binding for the FKey
			const char *binding = gameuifuncs->GetBindingForButtonCode( code );
			if ( binding && binding[0] )
			{
				// submit the entry as a console commmand
				char szCommand[256];
				Q_strncpy( szCommand, binding, sizeof( szCommand ) );
				engine->ClientCmd_Unrestricted( szCommand );
			}
		}
	}

	void OnKeyCodeReleased( vgui::KeyCode code )
	{
		m_KeyRepeat.KeyUp( code );

		BaseClass::OnKeyCodeReleased( code );
	}

	void OnThink()
	{
		vgui::KeyCode code = m_KeyRepeat.KeyRepeated();
		if ( code )
		{
			OnKeyCodeTyped( code );
		}

		BaseClass::OnThink();
	}

	virtual void OnKillFocus()
	{
		BaseClass::OnKillFocus();

		// force us to the rear when we lose focus (so it looks like the menu is always on the background)
		surface()->MovePopupToBack(GetVPanel());

		m_KeyRepeat.Reset();
	}

	void UpdateMenuItemState( bool isInGame, bool isMultiplayer )
	{
		bool isSteam = IsPC() && ( CommandLine()->FindParm("-steam") != 0 );
		bool bIsConsoleUI = false;

		// disabled save button if we're not in a game
		for (int i = 0; i < GetChildCount(); i++)
		{
			Panel *child = GetChild(i);
			MenuItem *menuItem = dynamic_cast<MenuItem *>(child);
			if (menuItem)
			{
				bool shouldBeVisible = true;
				// filter the visibility
				KeyValues *kv = menuItem->GetUserData();
				if (!kv)
					continue;

				if (!isInGame && kv->GetInt("OnlyInGame") )
				{
					shouldBeVisible = false;
				}
				else if (isMultiplayer && kv->GetInt("notmulti"))
				{
					shouldBeVisible = false;
				}
				else if (isInGame && !isMultiplayer && kv->GetInt("notsingle"))
				{
					shouldBeVisible = false;
				}
				else if (isSteam && kv->GetInt("notsteam"))
				{
					shouldBeVisible = false;
				}
				else if ( !bIsConsoleUI && kv->GetInt( "ConsoleOnly" ) )
				{
					shouldBeVisible = false;
				}

				menuItem->SetVisible( shouldBeVisible );
			}
		}

		if ( !isInGame )
		{
			// Sort them into their original order
			for ( int j = 0; j < GetChildCount() - 2; j++ )
			{
				MoveMenuItem( j, j + 1 );
			}
		}
		else
		{
			// Sort them into their in game order
			for ( int i = 0; i < GetChildCount(); i++ )
			{
				for ( int j = i; j < GetChildCount() - 2; j++ )
				{
					int iID1 = GetMenuID( j );
					int iID2 = GetMenuID( j + 1 );

					MenuItem *menuItem1 = GetMenuItem( iID1 );
					MenuItem *menuItem2 = GetMenuItem( iID2 );

					KeyValues *kv1 = menuItem1->GetUserData();
					KeyValues *kv2 = menuItem2->GetUserData();

					if ( kv1->GetInt("InGameOrder") > kv2->GetInt("InGameOrder") )
						MoveMenuItem( iID2, iID1 );
				}
			}
		}

		InvalidateLayout();
	}

private:
	vgui::CKeyRepeatHandler	m_KeyRepeat;
};

static CBackgroundMenuButton* CreateMenuButton( CBaseModPanel *parent, const char *panelName, const wchar_t *panelText )
{
	CBackgroundMenuButton *pButton = new CBackgroundMenuButton( parent, panelName );
	pButton->SetProportional(true);
	pButton->SetCommand("OpenGameMenu");
	pButton->SetText(panelText);

	return pButton;
}

bool g_bIsCreatingNewGameMenuForPreFetching = false;
#if 1
bool m_bWorkshopWarning = false;
#endif
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBaseModPanel::CBaseModPanel(VPANEL parent) : Panel(NULL, "BaseGameUIPanel")
{	
	m_flWaitBeforePaint = Plat_FloatTime() + 3.0f;

	SetParent(parent);
	g_pBaseModPanel = this;
	SetScheme(vgui::scheme()->LoadSchemeFromFile("resource/SourceScheme.res", "SourceScheme"));
	m_flTransitionStartTime = 0.0f;
	m_flTransitionEndTime = 0.0f;
	m_flFrameFadeInTime = 0.5f;
	m_bRenderingBackgroundTransition = false;
	m_bFadingInMenus = false;
	m_iGameMenuInset = 24;
	m_bPlatformMenuInitialized = false;
	m_bHaveDarkenedBackground = false;
	m_bHaveDarkenedTitleText = true;
	m_bForceTitleTextUpdate = true;
	m_BackdropColor = Color(0, 0, 0, 128);

	m_pGameMenuButtons.AddToTail( CreateMenuButton( this, "GameMenuButton", ModInfo().GetGameTitle() ) );
	m_pGameMenuButtons.AddToTail( CreateMenuButton( this, "GameMenuButton2", ModInfo().GetGameTitle2() ) );

	m_pGameMenu = NULL;
	m_pGameLogo = NULL;
	gameui = NULL;

	CreateGameMenu();
	InvalidateLayout(true, true);

	CreateGameLogo();

	// start the menus fully transparent
	SetMenuAlpha( 255 );
}

//-----------------------------------------------------------------------------
// Purpose: Causes the first menu item to be armed
//-----------------------------------------------------------------------------
void CBaseModPanel::ArmFirstMenuItem( void )
{
	UpdateGameMenus();

	// Arm the first item in the menu
	for ( int i = 0; i < m_pGameMenu->GetItemCount(); ++i )
	{
		if ( m_pGameMenu->GetMenuItem( i )->IsVisible() )
		{
			m_pGameMenu->SetCurrentlyHighlightedItem( i );
			break;
		}
	}
}

CBaseModPanel::~CBaseModPanel()
{
	gameui = NULL;
	g_BPGameUIDLL.Unload();
}

//-----------------------------------------------------------------------------
// Purpose: paints the main background image
//-----------------------------------------------------------------------------
void CBaseModPanel::PaintBackground()
{
#if 1
	if (m_bWorkshopWarning)
	{
		MessageBox *box = new MessageBox("Half-Life 2 mount error", "Half-Life 2 was not found. Either\n- You don't own Half-Life 2. \n- You don't have Half-Life 2 installed. \n- Steam decided to not work. \n\nYou will be missing most of the game's maps.", this);
		box->DoModal();
		m_bWorkshopWarning = false;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Size should only change on first vgui frame after startup
//-----------------------------------------------------------------------------
void CBaseModPanel::OnSizeChanged( int newWide, int newTall )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::CreateGameMenu()
{
	// load settings from config file
	KeyValues *datafile = new KeyValues("GameMenu");
	datafile->UsesEscapeSequences( true );	// VGUI uses escape sequences
	if (datafile->LoadFromFile( g_pFullFileSystem, "Resource/GameMenu.res" ) )
	{
		m_pGameMenu = RecursiveLoadGameMenu(datafile);
	}

	if ( !m_pGameMenu )
	{
		Error( "Could not load file Resource/GameMenu.res" );
	}
	else
	{
		// start invisible
		SETUP_PANEL( m_pGameMenu );
		m_pGameMenu->SetAlpha( 0 );
	}

	datafile->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if menu items need to be enabled/disabled
//-----------------------------------------------------------------------------
void CBaseModPanel::UpdateGameMenus()
{
	// check our current state
	bool isInGame = engine->IsInGame() && !engine->IsLevelMainMenuBackground();
	bool isMulti = isInGame && (engine->GetMaxClients() > 1);

	// iterate all the menu items
	m_pGameMenu->UpdateMenuItemState( isInGame, isMulti );

	// position the menu
	InvalidateLayout();
	m_pGameMenu->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: sets up the game menu from the keyvalues
//			the game menu is hierarchial, so this is recursive
//-----------------------------------------------------------------------------
CGameMenu *CBaseModPanel::RecursiveLoadGameMenu(KeyValues *datafile)
{
	CGameMenu *menu = new CGameMenu(this, datafile->GetName());

	// loop through all the data adding items to the menu
	for (KeyValues *dat = datafile->GetFirstSubKey(); dat != NULL; dat = dat->GetNextKey())
	{
		const char *label = dat->GetString("label", "<unknown>");
		const char *cmd = dat->GetString("command", NULL);
		const char *name = dat->GetString("name", label);

//		if ( !Q_stricmp( cmd, "OpenFriendsDialog" ) && bSteamCommunityFriendsVersion )
//			continue;

		menu->AddMenuItem(name, label, cmd, this, dat);
	}

	return menu;
}

//-----------------------------------------------------------------------------
// Purpose: update the taskbar a frame
//-----------------------------------------------------------------------------
void CBaseModPanel::RunFrame()
{
	InvalidateLayout();
	vgui::GetAnimationController()->UpdateAnimations( engine->Time() );
}

//-----------------------------------------------------------------------------
// Purpose: Lays out the position of the taskbar
//-----------------------------------------------------------------------------
void CBaseModPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	// Get the screen size
	int wide, tall;
	vgui::surface()->GetScreenSize(wide, tall);

	SetSize(wide, tall);
	// Get the size of the menu
	int menuWide, menuTall;
	m_pGameMenu->GetSize( menuWide, menuTall );

	int idealMenuY = m_iGameMenuPos.y;
	if ( idealMenuY + menuTall + m_iGameMenuInset > tall )
	{
		idealMenuY = tall - menuTall - m_iGameMenuInset;
	}

	int yDiff = idealMenuY - m_iGameMenuPos.y;

	if (!m_pGameMenuButtons.IsEmpty())
	{ 
		for ( int i=0; i<m_pGameMenuButtons.Count(); ++i )
		{
			// Get the size of the logo text
			//int textWide, textTall;
			m_pGameMenuButtons[i]->SizeToContents();
			//vgui::surface()->GetTextSize( m_pGameMenuButtons[i]->GetFont(), ModInfo().GetGameTitle(), textWide, textTall );

			// place menu buttons above middle of screen
			m_pGameMenuButtons[i]->SetPos(m_iGameTitlePos[i].x, m_iGameTitlePos[i].y + yDiff);
			//m_pGameMenuButtons[i]->SetSize(textWide + 4, textTall + 4);
		}
	}
		if ( m_pGameLogo )
		{
			// move the logo to sit right on top of the menu
			m_pGameLogo->SetPos( m_iGameMenuPos.x + m_pGameLogo->GetOffsetX(), idealMenuY - m_pGameLogo->GetTall() + m_pGameLogo->GetOffsetY() );
		}

	m_pGameMenu->SetPos(m_iGameMenuPos.x, idealMenuY);

	UpdateGameMenus();
}	

//-----------------------------------------------------------------------------
// Purpose: Loads scheme information
//-----------------------------------------------------------------------------
void CBaseModPanel::ApplySchemeSettings(IScheme *pScheme)
{
	int i;
	BaseClass::ApplySchemeSettings(pScheme);
#ifdef PROPORTIONAL_BASEPANEL
	m_iGameMenuInset = scheme()->GetProportionalScaledValue(atoi(pScheme->GetResourceString("MainMenu.Inset")));
#else
	m_iGameMenuInset = atoi(pScheme->GetResourceString("MainMenu.Inset"));
#endif

	m_iGameMenuInset *= 2;

	IScheme *pClientScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "ClientScheme" ) );

	Assert(pClientScheme);

	CUtlVector< Color > buttonColor;
	if ( pClientScheme )
	{
		m_iGameTitlePos.RemoveAll();
		for ( i=0; i<m_pGameMenuButtons.Count(); ++i )
		{
			m_pGameMenuButtons[i]->SetFont(pClientScheme->GetFont("ClientTitleFont", true));
			m_iGameTitlePos.AddToTail( coord() );
			m_iGameTitlePos[i].x = atoi(pClientScheme->GetResourceString( CFmtStr( "Main.Title%d.X", i+1 ) ) );
			m_iGameTitlePos[i].x = scheme()->GetProportionalScaledValue( m_iGameTitlePos[i].x );
			m_iGameTitlePos[i].y = atoi(pClientScheme->GetResourceString( CFmtStr( "Main.Title%d.Y", i+1 ) ) );
			m_iGameTitlePos[i].y = scheme()->GetProportionalScaledValue( m_iGameTitlePos[i].y );

			buttonColor.AddToTail( pClientScheme->GetColor( CFmtStr( "Main.Title%d.Color", i+1 ), Color(255, 255, 255, 255)) );
		}
#ifdef CS_BETA
		if ( !ModInfo().NoCrosshair() ) // hack to not show the BETA for HL2 or HL1Port
		{
			m_pGameMenuButtons[m_pGameMenuButtons.Count()-1]->SetFont(pClientScheme->GetFont("BetaFont", true));
		}
#endif // CS_BETA

		m_iGameMenuPos.x = atoi(pClientScheme->GetResourceString("Main.Menu.X"));
		m_iGameMenuPos.x = scheme()->GetProportionalScaledValue( m_iGameMenuPos.x );
		m_iGameMenuPos.y = atoi(pClientScheme->GetResourceString("Main.Menu.Y"));
		m_iGameMenuPos.y = scheme()->GetProportionalScaledValue( m_iGameMenuPos.y );

		m_iGameMenuInset = atoi(pClientScheme->GetResourceString("Main.BottomBorder"));
		m_iGameMenuInset = scheme()->GetProportionalScaledValue( m_iGameMenuInset );
	}
	else
	{
		for ( i=0; i<m_pGameMenuButtons.Count(); ++i )
		{
			m_pGameMenuButtons[i]->SetFont(pScheme->GetFont("TitleFont"));
			buttonColor.AddToTail( Color( 255, 255, 255, 255 ) );
		}
	}

	for ( i=0; i<m_pGameMenuButtons.Count(); ++i )
	{
		m_pGameMenuButtons[i]->SetDefaultColor(buttonColor[i], Color(0, 0, 0, 0));
		m_pGameMenuButtons[i]->SetArmedColor(buttonColor[i], Color(0, 0, 0, 0));
		m_pGameMenuButtons[i]->SetDepressedColor(buttonColor[i], Color(0, 0, 0, 0));
	}

	m_flFrameFadeInTime = atof(pScheme->GetResourceString("Frame.TransitionEffectTime"));

	// work out current focus - find the topmost panel
	SetBgColor(Color(0, 0, 0, 0));

	m_BackdropColor = pScheme->GetColor("mainmenu.backdrop", Color(0, 0, 0, 128));
}

//-----------------------------------------------------------------------------
// Purpose: message handler for menu selections
//-----------------------------------------------------------------------------
void CBaseModPanel::OnCommand( const char *command )
{
	//HACK
	char CMD_GameMenuCommand[MAX_PATH];
	V_sprintf_safe(CMD_GameMenuCommand, "gamemenucommand \"%s\"", command);
	engine->ClientCmd_Unrestricted(CMD_GameMenuCommand);
}

//-----------------------------------------------------------------------------
// Purpose: moves the game menu button to the right place on the taskbar
//-----------------------------------------------------------------------------
void CBaseModPanel::PositionDialog(vgui::PHandle dlg)
{
	if (!dlg.Get())
		return;

	int x, y, ww, wt, wide, tall;
	vgui::surface()->GetWorkspaceBounds( x, y, ww, wt );
	dlg->GetSize(wide, tall);

	// Center it, keeping requested size
	dlg->SetPos(x + ((ww - wide) / 2), y + ((wt - tall) / 2));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::CreateGameLogo()
{
	if (ModInfo().UseGameLogo())
	{
		m_pGameLogo = new CMainMenuGameLogo(this, "GameLogo");

		if (m_pGameLogo)
		{
#ifdef PROPORTIONAL_BASEPANEL
			m_pGameLogo->SetProportional(true);
#endif
			SETUP_PANEL(m_pGameLogo);
			
			m_pGameLogo->InvalidateLayout(true, true);

			// start invisible
			m_pGameLogo->SetAlpha(0);
		}
	}
	else
	{
		m_pGameLogo = NULL;
	}

}

//-----------------------------------------------------------------------------
// Purpose: Sets the alpha of the menu panels
//-----------------------------------------------------------------------------
void CBaseModPanel::SetMenuAlpha(int alpha)
{
	m_pGameMenu->SetAlpha(alpha);

	if ( m_pGameLogo )
	{
		m_pGameLogo->SetAlpha( alpha );
	}

	for ( int i=0; i<m_pGameMenuButtons.Count(); ++i )
	{
		m_pGameMenuButtons[i]->SetAlpha(alpha);
	}
	m_bForceTitleTextUpdate = true;
}

//-----------------------------------------------------------------------------
// Purpose: starts the game
//-----------------------------------------------------------------------------
void CBaseModPanel::FadeToBlackAndRunEngineCommand( const char *engineCommand )
{
	KeyValues *pKV = new KeyValues( "RunEngineCommand", "command", engineCommand );

	// execute immediately, with no delay
	PostMessage( this, pKV, 0 );
}

void CBaseModPanel::SetMenuItemBlinkingState( const char *itemName, bool state )
{
	for (int i = 0; i < GetChildCount(); i++)
	{
		Panel *child = GetChild(i);
		CGameMenu *pGameMenu = dynamic_cast<CGameMenu *>(child);
		if ( pGameMenu )
		{
			pGameMenu->SetMenuItemBlinkingState( itemName, state );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Editable panel that can replace the GameMenuButtons in CBaseModPanel
//-----------------------------------------------------------------------------
CMainMenuGameLogo::CMainMenuGameLogo( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	m_nOffsetX = 0;
	m_nOffsetY = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMainMenuGameLogo::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_nOffsetX = scheme()->GetProportionalScaledValue(inResourceData->GetInt("offsetX", 0));
	m_nOffsetY = scheme()->GetProportionalScaledValue(inResourceData->GetInt( "offsetY", 0 ));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMainMenuGameLogo::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/GameLogo.res" );
}

// Derived class of override interface
class CBaseModPanelInterface : public IBaseModPanelInterface
{
private:
	CBaseModPanel *MainMenu;

public:
	int UI_Interface(void)
	{
		MainMenu = NULL;
	}

	void Create(vgui::VPANEL parent)
	{
		// Create immediately
		MainMenu = new CBaseModPanel(parent);
	}

	vgui::VPANEL GetPanel(void)
	{
		if (!MainMenu)
			return NULL;
		return MainMenu->GetVPanel();
	}

	void Destroy(void)
	{
		if (MainMenu)
		{
			MainMenu->SetParent((vgui::Panel *)NULL);
			delete MainMenu;
		}
	}

};


static CBaseModPanelInterface g_SMenu;
IBaseModPanelInterface *BaseModPanel = (IBaseModPanelInterface *)&g_SMenu;


//-----------------------------------------------------------------------------
// Purpose: Animates menus on gameUI being shown
//-----------------------------------------------------------------------------
IGameUI *CBaseModPanel::GetGameUI()
{
	if (!gameui)
	{
		if (!LoadGameUI())
			return NULL;
	}

	return gameui;
}

bool CBaseModPanel::LoadGameUI()
{
	if (!gameui)
	{
		CreateInterfaceFn gameUIFactory = g_BPGameUIDLL.GetFactory();
		if (gameUIFactory)
		{
			gameui = (IGameUI *)gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL);
			if (!gameui)
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	return true;
}
