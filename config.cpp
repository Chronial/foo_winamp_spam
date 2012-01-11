/*
Copyright (c) 2005-2006 r1ch.net

This software is provided 'as-is',  without any express or implied  warranty. In
no event will the authors be held liable for any damages arising from the use of
this software.

Permission is granted to anyone to use this software for any purpose,  including
commercial applications, and to alter it and redistribute it freely, subject  to
the following restrictions:

    1. The  origin of  this software  must not  be misrepresented;  you must not
    claim that you wrote  the original software. If  you use this software  in a
    product, an acknowledgment in the product documentation would be appreciated
    but is not required.

    2. Altered source versions must be  plainly marked as such, and must  not be
    misrepresented as being the original software.

    3. This notice may not be removed or altered from any source distribution.
*/

#include "../_sdk/foobar2000/SDK/foobar2000.h"
#include "../_sdk/foobar2000/helpers/helpers.h"
#include "resource.h"

extern	cfg_string cfg_format;
extern  cfg_bool cfg_playlist;
extern  cfg_bool cfg_nodebug;
extern	pfc::string8 fb2kSongTitle;
void UpdateFakeWindowTitle (pfc::string8 trackTitle);
void checkWinampPath ();
void resetWinampPath ();

static BOOL CALLBACK ConfigProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		{
			uSetDlgItemText (wnd,IDC_FORMAT,cfg_format);
			CheckDlgButton(wnd,IDC_PLAYLIST,(cfg_playlist ? BST_CHECKED : BST_UNCHECKED));
			CheckDlgButton(wnd,IDC_NODEBUG,(cfg_nodebug ? BST_CHECKED : BST_UNCHECKED));
			return TRUE;
		}
		break;
	case WM_COMMAND:
		switch(wp)
		{
		case (EN_CHANGE<<16)|IDC_FORMAT:
			cfg_format.set_string (string_utf8_from_window((HWND)lp).get_ptr ());
			UpdateFakeWindowTitle (fb2kSongTitle);
			break;
		case (BN_CLICKED<<16)|IDC_PLAYLIST:
			{
				bool val = ( BST_CHECKED == IsDlgButtonChecked( wnd, IDC_PLAYLIST ) );
				cfg_playlist = val;
				if (val)
					checkWinampPath();
				else
					resetWinampPath();
			}
			break;
		case (BN_CLICKED<<16)|IDC_NODEBUG:
			{
				bool val = ( BST_CHECKED == IsDlgButtonChecked( wnd, IDC_PLAYLIST ) );
				cfg_nodebug = val;
			}
		}
		break;
	}
	return 0;
}

// {D6E376C3-3C15-4e70-9ACB-1E4B32493722}
static const GUID config_guid = 
{ 0xd6e376c3, 0x3c15, 0x4e70, { 0x9a, 0xcb, 0x1e, 0x4b, 0x32, 0x49, 0x37, 0x22 } };


class config_input : public preferences_page
{
public:
	HWND create(HWND parent)
	{
		return uCreateDialog(IDD_CONFIG,parent,ConfigProc);
	}

	GUID get_guid()
	{
		return config_guid;
	}

	bool preferences_page::reset_query(void)
	{
		return false;
	}
	void preferences_page::reset(void) {}


	const char * get_name() {return "Winamp API Emulator";}
	GUID get_parent_guid() {return preferences_page::guid_tools; }
};

//static service_factory_t<config,config_input> foo;
static preferences_page_factory_t <config_input> foo;
