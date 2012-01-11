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

#define UNICODE 1

#include "../_sdk/foobar2000/SDK/foobar2000.h"
#include "../_sdk/foobar2000/helpers/helpers.h"
#include "winamp.h"

#define VERSION ("0.96")

DECLARE_COMPONENT_VERSION ("Winamp API Emulator",
	VERSION,
	"Emulates the Winamp API, making Foobar compatible with sofware written for Winamp\n"
	"Original (till v0.90) by R1CH (www.r1ch.net)\n"
	"Sice v0.91 maintained by Chronial (Christian Fersch)");


// {1D3BB858-26FA-4a70-ADB0-6EB567B32576}
static const GUID cfg_format_guid = 
{ 0x1d3bb858, 0x26fa, 0x4a70, { 0xad, 0xb0, 0x6e, 0xb5, 0x67, 0xb3, 0x25, 0x76 } };

cfg_string cfg_format(cfg_format_guid, "[%artist% - ]%title%");

// {C9B308C6-6124-4444-B2A5-D85D114FC410}
static const GUID cfg_playlist_guid = 
{ 0xc9b308c6, 0x6124, 0x4444, { 0xb2, 0xa5, 0xd8, 0x5d, 0x11, 0x4f, 0xc4, 0x10 } };

cfg_bool cfg_playlist(cfg_playlist_guid,true);


// {E58441D2-6652-4d9d-8F42-B96A81322D26}
static const GUID cfg_nodebug_guid = 
{ 0xe58441d2, 0x6652, 0x4d9d, { 0x8f, 0x42, 0xb9, 0x6a, 0x81, 0x32, 0x2d, 0x26 } };

cfg_bool cfg_nodebug(cfg_nodebug_guid,false);

HWND hwndMain;

int fb2kKbps;
int fb2kTotalLength;
int fb2kKhz;
int fb2kCurPLEntry;
int fb2kNumChans;
int fb2kRating;

int playingStatus = 0;

char fb2kFakeSongPath[MAX_PATH];

pfc::string8 fb2kSongTitle;
pfc::string8 fb2kFakeWindowTitle;
pfc::string8 trackNumber;
pfc::string8 winampPath;

//metadb_handle *fb2kCurrentSongHandle = NULL;

void UpdateFakeWindowTitle (pfc::string8 trackTitle)
{
//	wchar_t	out[1024];

	fb2kFakeWindowTitle.set_string ("");
	fb2kFakeWindowTitle.add_string (trackNumber.get_ptr());
	fb2kFakeWindowTitle.add_string (". ");
	fb2kFakeWindowTitle.add_string (trackTitle.get_ptr());
	fb2kFakeWindowTitle.add_string (" - Winamp");

	//pfc::stringcvt::convert_utf8_to_wide (out, sizeof(out), fb2kFakeWindowTitle, fb2kFakeWindowTitle.get_length());

	uSetWindowText (hwndMain, fb2kFakeWindowTitle);
}

void checkWinampPath(){
	HKEY keySoftware;
	HKEY keyWinamp;
	winampPath.reset();
	if (RegOpenKeyEx(HKEY_CURRENT_USER,L"Software",0,KEY_READ,&keySoftware) == ERROR_SUCCESS){
		if (RegOpenKeyEx(keySoftware,L"Winamp",0,KEY_READ,&keyWinamp) == ERROR_SUCCESS){
			DWORD type;
			BYTE data[1024];
			DWORD length;
			if (RegQueryValueExA(keyWinamp,NULL,0,&type,data,&length) == ERROR_SUCCESS){
				if (type == REG_SZ){
					winampPath.set_string((char*)data,length);
					//console::info("loaded from registry:");
					//console::info(winampPath);
				}
			}
			RegCloseKey(keyWinamp);
		}
		RegCloseKey(keySoftware);
	}
	if (winampPath.is_empty()){
		if (RegOpenKeyEx(HKEY_CURRENT_USER,L"Software",NULL,KEY_ALL_ACCESS,&keySoftware) == ERROR_SUCCESS){
			if (RegCreateKeyEx(keySoftware,L"Winamp",NULL,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&keyWinamp,NULL) == ERROR_SUCCESS){
				const char * foobarPath = (core_api::get_profile_path()+7);
				if (RegSetValueExA(keyWinamp,NULL,0,REG_SZ,(BYTE*)foobarPath,strlen(foobarPath)+1) == ERROR_SUCCESS){
					winampPath = foobarPath;
					//console::info("written to registry:");
					//console::info(winampPath);
				}
				RegCloseKey(keyWinamp);
			}
			RegCloseKey(keySoftware);
		}
	}
	if (winampPath.is_empty()){
		console::error("foo_winamp_spam: Couldn't write winamp path to registry, ensure you have access");
	}
}
void resetWinampPath(){
	HKEY keySoftware;
	HKEY keyWinamp;
	winampPath.reset();
	if (RegOpenKeyEx(HKEY_CURRENT_USER,L"Software",0,KEY_ALL_ACCESS,&keySoftware) == ERROR_SUCCESS){
		if (RegOpenKeyEx(keySoftware,L"Winamp",0,KEY_ALL_ACCESS,&keyWinamp) == ERROR_SUCCESS){
			DWORD type;
			BYTE data[1024];
			DWORD length;
			if (RegQueryValueExA(keyWinamp,NULL,0,&type,data,&length) == ERROR_SUCCESS){
				if (type == REG_SZ){
					const char * foobarPath = (core_api::get_profile_path()+7);
					if (_strcmpi((char*)data,foobarPath) == 0 ){
						RegDeleteValue(keyWinamp,NULL);
						winampPath.reset();
						//console::info("key deleted");
					} else {
						//console::info("was not our setting");
					}
				}
			}
			RegCloseKey(keyWinamp);
		}
		RegCloseKey(keySoftware);
	}
}


class foo_winamp_spam_callback : private play_callback
{
public:
	virtual void on_playback_starting(play_control::t_track_command p_command, bool p_paused)
	{
		playingStatus = 1;
	}

	virtual void on_playback_time(double p_time) {}
	virtual void on_playback_edited(metadb_handle_ptr p_track) {}
	virtual void on_playback_seek(double p_time) {}
	virtual void on_volume_change(float p_new_val) {}
	virtual void on_playback_pause(bool p_state)
	{
		if (p_state) 
			playingStatus = 3;
		else
			playingStatus = 1;
	}

	virtual void on_playback_stop(play_control::t_stop_reason reason)
	{
		playingStatus = 0;

		//if (fb2kCurrentSongHandle) {
			//fb2kCurrentSongHandle-> ();
			//fb2kCurrentSongHandle = NULL;
		//}
	}

	virtual void on_playback_dynamic_info_track(const file_info & p_info)
	{
		//fb2kSongTitle.set_string (p_info.meta_get ("Title", 0));
		//p_info.copy_meta
		static_api_ptr_t<play_control> pc;
		service_ptr_t<titleformat_object> script;
		if (static_api_ptr_t<titleformat_compiler>()->compile (script, cfg_format.get_ptr()))
		{
			metadb_handle_ptr meta;
			if (pc->get_now_playing (meta))
			{
				pc->playback_format_title_ex(meta, NULL, fb2kSongTitle, script, NULL, play_control::display_level_titles);
			}
			else
				fb2kSongTitle.set_string (p_info.meta_get ("Title", 0));
		}
		UpdateFakeWindowTitle (fb2kSongTitle);
	}

	virtual void on_playback_dynamic_info(const file_info & p_info)
	{
		fb2kKbps = (int) p_info.info_get_bitrate_vbr();
	}

	virtual void on_playback_new_track (metadb_handle_ptr metadb)
	{

		static_api_ptr_t<play_control> pc;
		service_ptr_t<titleformat_object> script;
		if (static_api_ptr_t<titleformat_compiler>()->compile (script, cfg_format.get_ptr()))
		{
			pc->playback_format_title_ex(metadb, NULL, fb2kSongTitle, script, NULL, play_control::display_level_titles);
		}

		t_size plIndex, trackIndex;
		if (static_api_ptr_t<playlist_manager>()->get_playing_item_location (&plIndex, &trackIndex))
		{
			char	buff[64];
			_snprintf (buff, sizeof(buff)-1, "%d", (int)trackIndex);
			trackNumber.set_string (buff);
		}
		else
			trackNumber.set_string ("0");

		metadb->metadb_lock();
		const file_info * info;
		if (metadb->get_info_locked (info)){

			fb2kTotalLength = (int)metadb->get_length ();
			fb2kKbps = (int)info->info_get_int ("bitrate");
			fb2kKhz = (int)info->info_get_int ("samplerate");
			fb2kNumChans = (int)info->info_get_int ("channels");
			loadRating(info);


			strncpy (fb2kFakeSongPath, metadb->get_path(), sizeof(fb2kFakeSongPath)-1);
		}
		metadb->metadb_unlock();
		UpdateFakeWindowTitle(fb2kSongTitle);
	}

	void doregister (void)
	{
		static_api_ptr_t<play_callback_manager> pcm;
		pcm->register_callback(this, flag_on_playback_new_track | flag_on_playback_dynamic_info | flag_on_playback_dynamic_info_track | flag_on_playback_stop | flag_on_playback_starting, false);
	}

	void dounregister (void)
	{
		static_api_ptr_t<play_callback_manager> pcm;
		pcm->unregister_callback(this);
	}
private:
	void loadRating(const file_info * p_info){
		if (p_info->meta_exists("rating")){
			int winampRating = _atoi64(p_info->meta_get("rating",0));
			if (winampRating < 1)
				winampRating = 1;
			else if (winampRating > 5)
				winampRating = 5;
			fb2kRating = winampRating;
		} else {
			fb2kRating = 0;
		}
	}
};

static foo_winamp_spam_callback winampspam;



LRESULT CALLBACK WndProcMain(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CLOSE:
			ShowWindow (hwnd, SW_HIDE);
			return TRUE;
		case WM_SHOWWINDOW:
			if (wParam == TRUE){
				CloseWindow (hwnd);
				standard_commands::main_activate();
				return 0;
			}
		case WM_USER:
			switch (lParam)
			{
				case IPC_GETINFO:
					switch (wParam)
					{
						case 0:
							return fb2kKhz;
							break;
						case 1:
							return fb2kKbps;
							break;
						case 2:
							return fb2kNumChans;
							break;
					}
					break;

				case IPC_GETOUTPUTTIME:
					{
						static_api_ptr_t<playback_control> pc;
						if (!pc->is_playing()){
							return -1;
						} else {
							switch (wParam)
							{
								case 0:
									return (long)(pc->playback_get_position() * 1000);
									break;
								case 1:
									return fb2kTotalLength;
									break;
							}
						}
					}
					break;

				case IPC_GETPLAYLISTFILE:
					if (!strncmp (fb2kFakeSongPath, "file://", 7))
						return (long)(void *)(fb2kFakeSongPath + 7);
					else
						return (long)(void *)fb2kFakeSongPath;
					break;

				case IPC_ISPLAYING:
					return playingStatus;

				case IPC_GETPLAYLISTTITLE:
					return (long)(void *)fb2kSongTitle.get_ptr();
					break;

				case IPC_UPDTITLE:
					{
						//metadb_handle *current = play_control::get()->get_now_playing();
						//current->handle_update_info (NULL, true);
					}
					break;

				case IPC_GET_SHUFFLE:
					//return mainmenu_commands::
					//return menu_manager::is_command_checked ("Playback/Order/Random");
					return (static_api_ptr_t<playlist_manager>()->playback_order_get_active () >= 3 && static_api_ptr_t<playlist_manager>()->playback_order_get_active () <= 5) ? 1 : 0;
					break;

				case IPC_GET_REPEAT:
					return (static_api_ptr_t<playlist_manager>()->playback_order_get_active () >= 1 && static_api_ptr_t<playlist_manager>()->playback_order_get_active () <= 2) ? 1 : 0;
					//return menu_manager::is_command_checked ("Playback/Order/Repeat");
					break;

				case IPC_SET_SHUFFLE:
					if ((static_api_ptr_t<playlist_manager>()->playback_order_get_active () >= 3 && static_api_ptr_t<playlist_manager>()->playback_order_get_active () <= 5) ? 1 : 0 == (bool)lParam)
						break;
					else
						static_api_ptr_t<playlist_manager>()->playback_order_set_active (lParam ? 3 : 0);
					break;

				case IPC_SET_REPEAT:
					if ((static_api_ptr_t<playlist_manager>()->playback_order_get_active () >= 1 && static_api_ptr_t<playlist_manager>()->playback_order_get_active () <= 2) ? 1 : 0 == (bool)lParam)
						break;
					else
						static_api_ptr_t<playlist_manager>()->playback_order_set_active (lParam ? 2 : 0);
					break;

				case IPC_GETLISTPOS:
					return fb2kCurPLEntry;
					break;

				case IPC_IS_FOOBAR:
					return 1;

				case IPC_GETVERSION:
					return 0x2080;
					break;
				
				// Implemented by Chronial
				case IPC_GETLISTLENGTH:
					{
						static_api_ptr_t<playlist_manager> pm;
						t_size plIndex, trackIndex;
						if (pm->get_playing_item_location (&plIndex, &trackIndex)){
							return pm->playlist_get_item_count(plIndex);
						} else {
							return pm->activeplaylist_get_item_count();
						}
					}
					break;
				
				case IPC_JUMPTOTIME:
					{
						static_api_ptr_t<playback_control> pc;
						if (!pc->is_playing())
							return -1;
						else if (pc->playback_can_seek() && (pc->playback_get_length() <= wParam)){
							pc->playback_seek(((double)wParam)/1000);
							return 0;
						} else {
							return 1;
						}
					}
					break;

				case IPC_GETTIMEDISPLAYMODE:
					return 0;
					break;

				case IPC_WRITEPLAYLIST:
					if (cfg_playlist){
						static_api_ptr_t<playlist_manager> pm;
						pfc::list_t<metadb_handle_ptr> playlistItems;
						t_size plIndex, trackIndex;
						if (pm->get_playing_item_location (&plIndex, &trackIndex)){
							pm->playlist_get_all_items(plIndex,playlistItems);
						} else {
							pm->activeplaylist_get_all_items(playlistItems);
							trackIndex = 0;
						}
						foobar2000_io::abort_callback_impl abortCallback;
						char * playlistPath = new char[winampPath.length() + 12];
						strcpy(playlistPath,winampPath);
						strcpy(playlistPath+winampPath.length(),"\\winamp.m3u");
						//console::info("saving requested, saving to:");
						//console::info(playlistPath);
						playlist_loader::g_save_playlist(playlistPath,playlistItems,abortCallback);
						delete[] playlistPath;

						return trackIndex;
					} else {
						console::info("foo_winamp_spam: Playlist writing requested, but disabled in Options (IPC_WRITEPLAYLIST)");
						return 0;
					}
					break;


				case IPC_SETVOLUME:
					{
						static_api_ptr_t<playback_control> pc;
						if (wParam == -666){
							float fooVolume = pc->get_volume();
							int winampVolume = floor( (-71.9274 * log( -0.00962056 * (-3. + fooVolume))) + 0.5);
							if (winampVolume > 255)
								winampVolume = 255;
							else if (winampVolume < 0)
								winampVolume = 0;
							return winampVolume;
						} else {
							float fooVolume = - (-1 + pow((float)1.014,(float)(255-wParam)))*3;
							pc->set_volume(fooVolume);
							return 0;
						}
					}
					break;

				case IPC_GETRATING:
					return fb2kRating;
					break;

				case IPC_SETRATING:
					{
						static_api_ptr_t<playback_control> pc;
						metadb_handle_ptr p;
						if (pc->get_now_playing(p)) {
							int rating = wParam;
							if (rating > 0 && rating < 6){
								static_api_ptr_t<metadb_io_v2> dbio;
								file_info_impl info;
								pfc::list_t<metadb_handle_ptr> trackList;
								pfc::list_t<const file_info*> data;
								p->get_info(info);

								if (rating == 0){
									info.meta_remove_field("rating");
								} else { // rating [1-5]
									char ratingStr;
									sprintf_s(&ratingStr, 1, "%d", rating);
									info.meta_set_ex("rating",~0,&ratingStr,1);
								}
								data.add_item(&info);
								trackList.add_item(p);
								dbio->update_info_async_simple(trackList,data,core_api::get_main_window(),dbio->op_flag_no_errors|dbio->op_flag_delay_ui,0);
								fb2kRating = rating;
							}
						}
						return 0;
					}
					break;

				// do not have any meaning
				/*case 1241704:
				case 1242024:
				case 1243292:
				case 1237188:
				case 1235828:
					break;*/

				default:
					{
						if ((lParam < 1200000) && (!cfg_nodebug)){
							char warning[128];
							sprintf (warning, "foo_winamp_spam: Unsupported WM_USER, lParam %d.\n", lParam);
							console::warning (warning);
						}
					}
					break;
			}
	
			break;

		case WM_CREATE:
			return TRUE;
			break;

		case WM_COMMAND:
			switch(wParam)
			{
			case WINAMP_BUTTON2:
				//play_control::get()->play_start();
				standard_commands::main_play ();
				break;

			case WINAMP_BUTTON2_SHIFT:
				standard_commands::main_add_files();
				break;

			case WINAMP_BUTTON2_CTRL:
				standard_commands::main_add_location();
				break;

			case WINAMP_BUTTON4:
			case WINAMP_BUTTON4_SHIFT: //Stop with fade-out, default in foobar
				standard_commands::main_stop ();
				break;

			case WINAMP_BUTTON4_CTRL:
				//play_control::get()->toggle_stop_after_current ();
				{
					static_api_ptr_t<play_control> pc;
					pc->toggle_stop_after_current ();
				}
				break;

			case WINAMP_BUTTON3:
			case WINAMP_BUTTON3_SHIFT:
			case WINAMP_BUTTON3_CTRL:
				//standard_commands::main_play_or_pause ();
				{
					static_api_ptr_t<playback_control> pc;
					pc->pause(true);
					//pc->play_or_pause();
					//console::printf("time: %d",(int)(pc->playback_get_position()*1000));
				}
				break;

			case WINAMP_BUTTON5:
				//play_control::get()->play_start (play_control::TRACK_COMMAND_NEXT);
				standard_commands::main_next ();
				break;

			case WINAMP_BUTTON5_CTRL:
				{
					static_api_ptr_t<playback_control> pc;
					pc->playback_seek_delta(5);
				}
				break;

			case WINAMP_BUTTON5_SHIFT:
				{
					static_api_ptr_t<playlist_manager> pm;
					t_size pl = pm->get_playing_playlist();
					t_size len = pm->playlist_get_item_count(pl);
					pm->playlist_execute_default_action(pl, len - 1);
				}
				break;

			case WINAMP_BUTTON1:
				//play_control::get()->play_start (play_control::TRACK_COMMAND_PREV);
				standard_commands::main_previous ();
				break;

			case WINAMP_BUTTON1_CTRL:
				{
					static_api_ptr_t<playback_control> pc;
					pc->playback_seek_delta(-5);
				}
				break;

			case WINAMP_BUTTON1_SHIFT:
				{
					static_api_ptr_t<playlist_manager> pm;
					t_size pl = pm->get_playing_playlist();
					pm->playlist_execute_default_action(pl,0);
				}
				break;

			case WINAMP_VOLUMEUP:
				//play_control::get()->volume_up ();
				standard_commands::main_volume_up ();
				break;

			case WINAMP_VOLUMEDOWN:
				//play_control::get()->volume_down ();
				standard_commands::main_volume_down ();
				break;

			case WINAMP_FILE_PLAY:
				//menu_manager::run_command("Playlist/Add files...");
				standard_commands::main_add_files ();
				break;

			case WINAMP_OPTIONS_PREFS:
				//menu_manager::run_command("Foobar2000/Preferences");
				standard_commands::main_preferences ();
				break;

			case WINAMP_HELP_ABOUT:
				//menu_manager::run_command("Foobar2000/About");
				standard_commands::main_about ();
				break;

			case 40194: // Jump to file
				//menu_manager::run_command("Playlist/Search...");
				standard_commands::main_playlist_search ();
				break;

			case 40001:
				//menu_manager::run_command("Foobar2000/Exit");
				standard_commands::main_exit ();
				break;

			default:
				{
					char warning[128];
					sprintf (warning, "foo_winamp_spam: Unsupported WM_COMMAND, wParam %d.\n", wParam);
					console::warning (warning);
				}
				break;

			/*case WM_COPYDATA:
				COPYDATASTRUCT cds;
				cds = *(COPYDATASTRUCT *)lParam;
				switch (cds.dwData) {
					case IPC_PLAYFILE:
						playable_location_i loc;
						char *buff = new char[*(int *)cds.cbData];
						strncpy (buff, (const char *)cds.lpData, *(int *)cds.cbData);
						loc.set_path (buff);
						delete buff;

						
						metadb * p_metadb = metadb::get();

						p_metadb->

						metadb_handle *temp = p_metadb->handle_create (&loc);
						play_control::get()->play_item (temp);
						temp->handle_release ();
				}*/
			}
			break;
	}

	return DefWindowProcA (hwnd, message, wParam, lParam);;
}

class foo_winamp_spam_initquit : public initquit
{
public:
	virtual void on_init()
	{
		if (cfg_playlist)
			checkWinampPath();

		WNDCLASSA wcex;

		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= (WNDPROC)WndProcMain;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= core_api::get_my_instance();
		wcex.hIcon			= NULL; //LoadIcon(hInstance, (LPCTSTR)IDI_RCONOMATIC);
		wcex.hCursor		= LoadCursor (core_api::get_my_instance(), MAKEINTRESOURCE(32512));
		wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
		wcex.lpszMenuName	= NULL;
		wcex.lpszClassName	= "Winamp v1.x";

		RegisterClassA (&wcex);

		hwndMain = uCreateWindowEx (0, "Winamp v1.x", "Winamp v1.x", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 110, 110, 510, 110, NULL, NULL, core_api::get_my_instance(), NULL);

		winampspam.doregister ();
	}
	virtual void on_quit() {
		resetWinampPath();

		if (hwndMain)
			DestroyWindow (hwndMain);

		hwndMain = NULL;

		UnregisterClass (L"Winamp v1.x", core_api::get_my_instance());

		winampspam.dounregister ();
	}
};


//static service_factory_single_t<foo_winamp_spam_callback> foo;
//static service_factory_single_t<foo_winamp_spam_initquit> bar;

static initquit_factory_t< foo_winamp_spam_initquit > foo_initquit;
//static play_callback_manager <foo_winamp_spam_callback> foo_callback;
