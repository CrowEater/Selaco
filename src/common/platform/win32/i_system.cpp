/*
** i_system.cpp
** Timers, pre-console output, IWAD selection, and misc system routines.
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** Copyright (C) 2007-2012 Skulltag Development Team
** Copyright (C) 2007-2016 Zandronum Development Team
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. Redistributions in any form must be accompanied by information on how to
**    obtain complete source code for the software and any accompanying software
**    that uses the software. The source code must either be included in the
**    distribution or be available for no more than the cost of distribution plus
**    a nominal fee, and must be freely redistributable under reasonable
**    conditions. For an executable file, complete source code means the source
**    code for all modules it contains. It does not include source code for
**    modules or files that typically accompany the major components of the
**    operating system on which the executable file runs.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

// HEADER FILES ------------------------------------------------------------

#pragma warning(disable:4996)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <process.h>
#include <time.h>
#include <map>

#include <stdarg.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <richedit.h>
#include <wincrypt.h>
#include <shlwapi.h>

#include "hardware.h"
#include "printf.h"

#include "version.h"
#include "i_sound.h"
#include "resource.h"
#include "stats.h"
#include "v_text.h"
#include "utf8.h"

#include "i_input.h"
#include "c_dispatch.h"

#include "gameconfigfile.h"
#include "v_font.h"
#include "i_system.h"
#include "bitmap.h"
#include "cmdlib.h"
#include "i_interface.h"
#include "i_mainwindow.h"

// MACROS ------------------------------------------------------------------

#ifdef _MSC_VER
// Turn off "conversion from 'LONG_PTR' to 'LONG', possible loss of data"
// generated by SetClassLongPtr().
#pragma warning(disable:4244)
#endif

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void DestroyCustomCursor();

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static HCURSOR CreateCompatibleCursor(FBitmap &cursorpic, int leftofs, int topofs);
static HCURSOR CreateAlphaCursor(FBitmap &cursorpic, int leftofs, int topofs);
static HCURSOR CreateBitmapCursor(int xhot, int yhot, HBITMAP and_mask, HBITMAP color_mask);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR (Bool, queryiwad);
// Used on welcome/IWAD screen.
EXTERN_CVAR (Bool, disableautoload)
EXTERN_CVAR (Bool, autoloadlights)
EXTERN_CVAR (Bool, autoloadbrightmaps)
EXTERN_CVAR (Bool, autoloadwidescreen)
EXTERN_CVAR (Int, vid_preferbackend)

extern HANDLE StdOut;
extern bool FancyStdOut;
extern HINSTANCE g_hInst;
extern FILE *Logfile;
extern bool NativeMouse;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (String, queryiwad_key, "none", CVAR_GLOBALCONFIG|CVAR_ARCHIVE);
CVAR (Bool, con_debugoutput, false, 0);

double PerfToSec, PerfToMillisec;

UINT TimerPeriod;

int sys_ostype = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static WadStuff *WadList;
static int NumWads;
static int DefaultWad;

static HCURSOR CustomCursor;

//==========================================================================
//
// I_DetectOS
//
// Determine which version of Windows the game is running on.
//
//==========================================================================

void I_DetectOS(void)
{
	OSVERSIONINFOEX info;
	const char *osname;

	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if (!GetVersionEx((OSVERSIONINFO *)&info))
	{
		// Retry with the older OSVERSIONINFO structure.
		info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx((OSVERSIONINFO *)&info);
	}

	switch (info.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_NT:
		osname = "NT";
		if (info.dwMajorVersion == 6)
		{
			if (info.dwMinorVersion == 0)
			{
				osname = (info.wProductType == VER_NT_WORKSTATION) ? "Vista" : "Server 2008";
				sys_ostype = 2; // legacy OS
			}
			else if (info.dwMinorVersion == 1)
			{
				osname = (info.wProductType == VER_NT_WORKSTATION) ? "7" : "Server 2008 R2";
				sys_ostype = 2; // supported OS
			}
			else if (info.dwMinorVersion == 2)	
			{
				// Starting with Windows 8.1, you need to specify in your manifest
				// the highest version of Windows you support, which will also be the
				// highest version of Windows this function returns.
				osname = (info.wProductType == VER_NT_WORKSTATION) ? "8" : "Server 2012";
				sys_ostype = 2; // supported OS
			}
			else if (info.dwMinorVersion == 3)
			{
				osname = (info.wProductType == VER_NT_WORKSTATION) ? "8.1" : "Server 2012 R2";
				sys_ostype = 2; // supported OS
			}
			else if (info.dwMinorVersion == 4)
			{
				osname = (info.wProductType == VER_NT_WORKSTATION) ? "10 (beta)" : "Server 2016 (beta)";
			}
		}
		else if (info.dwMajorVersion == 10)
		{
			osname = (info.wProductType == VER_NT_WORKSTATION) ? (info.dwBuildNumber >= 22000 ? "11 (or higher)" : "10") : "Server 2016 (or higher)";
			sys_ostype = 3; // modern OS
		}
		break;

	default:
		osname = "Unknown OS";
		break;
	}

	if (!batchrun) Printf ("OS: Windows %s (NT %lu.%lu) Build %lu\n    %s\n",
			osname,
			info.dwMajorVersion, info.dwMinorVersion,
			info.dwBuildNumber, info.szCSDVersion);
}

//==========================================================================
//
// CalculateCPUSpeed
//
// Make a decent guess at how much time elapses between TSC steps. This can
// vary over runtime depending on power management settings, so should not
// be used anywhere that truely accurate timing actually matters.
//
//==========================================================================

void CalculateCPUSpeed()
{
	LARGE_INTEGER freq;

	QueryPerformanceFrequency (&freq);

	if (freq.QuadPart != 0)
	{
		LARGE_INTEGER count1, count2;
		cycle_t ClockCalibration;
		DWORD min_diff;

		ClockCalibration.Reset();

		// Count cycles for at least 55 milliseconds.
        // The performance counter may be very low resolution compared to CPU
        // speeds today, so the longer we count, the more accurate our estimate.
        // On the other hand, we don't want to count too long, because we don't
        // want the user to notice us spend time here, since most users will
        // probably never use the performance statistics.
        min_diff = freq.LowPart * 11 / 200;

		// Minimize the chance of task switching during the testing by going very
		// high priority. This is another reason to avoid timing for too long.
		SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

		// Make sure we start timing on a counter boundary.
		QueryPerformanceCounter(&count1);
		do { QueryPerformanceCounter(&count2); } while (count1.QuadPart == count2.QuadPart);

		// Do the timing loop.
		ClockCalibration.Clock();
		do { QueryPerformanceCounter(&count1); } while ((count1.QuadPart - count2.QuadPart) < min_diff);
		ClockCalibration.Unclock();

		SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

		PerfToSec = double(count1.QuadPart - count2.QuadPart) / (double(ClockCalibration.GetRawCounter()) * freq.QuadPart);
		PerfToMillisec = PerfToSec * 1000.0;
	}

	if (!batchrun) Printf ("CPU speed: %.0f MHz\n", 0.001 / PerfToMillisec);
}

//==========================================================================
//
// I_PrintStr
//
// Send output to the list box shown during startup (and hidden during
// gameplay).
//
//==========================================================================

static void PrintToStdOut(const char *cpt, HANDLE StdOut)
{
	if (StdOut == nullptr && !con_debugoutput)
		return;

	wchar_t wbuf[256];
	int bpos = 0;

	const uint8_t *cptr = (const uint8_t*)cpt;

	auto outputIt = [&]()
	{
		wbuf[bpos] = 0;
		if (con_debugoutput)
		{
			OutputDebugStringW(wbuf);
		}
		if (StdOut != nullptr)
		{
			// Convert back to UTF-8.
			DWORD bytes_written;
			if (!FancyStdOut)
			{
				FString conout(wbuf);
				WriteFile(StdOut, conout.GetChars(), (DWORD)conout.Len(), &bytes_written, NULL);
			}
			else
			{
				WriteConsoleW(StdOut, wbuf, bpos, &bytes_written, nullptr);
			}
		}
		bpos = 0;
	};

	while (int chr = GetCharFromString(cptr))
	{
		if ((chr == TEXTCOLOR_ESCAPE && bpos != 0) || bpos == 255)
		{
			outputIt();
		}
		if (chr != TEXTCOLOR_ESCAPE)
		{
			if (chr >= 0x1D && chr <= 0x1F)
			{ // The bar characters, most commonly used to indicate map changes
				chr = 0x2550;	// Box Drawings Double Horizontal
			}
			wbuf[bpos++] = chr;
		}
		else
		{
			EColorRange range = V_ParseFontColor(cptr, CR_UNTRANSLATED, CR_YELLOW);

			if (range != CR_UNDEFINED)
			{
				// Change the color of future text added to the control.
				PalEntry color = V_LogColorFromColorRange(range);
				if (StdOut != NULL && FancyStdOut)
				{
					// Unfortunately, we are pretty limited here: There are only
					// eight basic colors, and each comes in a dark and a bright
					// variety.
					float h, s, v, r, g, b;
					int attrib = 0;

					RGBtoHSV(color.r / 255.f, color.g / 255.f, color.b / 255.f, &h, &s, &v);
					if (s != 0)
					{ // color
						HSVtoRGB(&r, &g, &b, h, 1, 1);
						if (r == 1)  attrib  = FOREGROUND_RED;
						if (g == 1)  attrib |= FOREGROUND_GREEN;
						if (b == 1)  attrib |= FOREGROUND_BLUE;
						if (v > 0.6) attrib |= FOREGROUND_INTENSITY;
					}
					else
					{ // gray
						     if (v < 0.33) attrib = FOREGROUND_INTENSITY;
						else if (v < 0.90) attrib = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
						else			   attrib = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
					}
					SetConsoleTextAttribute(StdOut, (WORD)attrib);
				}
			}
		}
	}
	if (bpos != 0)
	{
		outputIt();
	}

	if (StdOut != NULL && FancyStdOut)
	{ // Set text back to gray, in case it was changed.
		SetConsoleTextAttribute(StdOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	}
}

void I_PrintStr(const char *cp)
{
	mainwindow.PrintStr(cp);
	PrintToStdOut(cp, StdOut);
}

//==========================================================================
//
// SetQueryIWAD
//
// The user had the "Don't ask again" box checked when they closed the
// IWAD selection dialog.
//
//==========================================================================

static void SetQueryIWad(HWND dialog)
{
	HWND checkbox = GetDlgItem(dialog, IDC_DONTASKIWAD);
	int state = (int)SendMessage(checkbox, BM_GETCHECK, 0, 0);
	bool query = (state != BST_CHECKED);

	if (!query && queryiwad)
	{
		MessageBoxA(dialog,
			"You have chosen not to show this dialog box in the future.\n"
			"If you wish to see it again, hold down SHIFT while starting " GAMENAME ".",
			"Don't ask me this again",
			MB_OK | MB_ICONINFORMATION);
	}

	queryiwad = query;
}

//==========================================================================
//
// IWADBoxCallback
//
// Dialog proc for the IWAD selector.
//
//==========================================================================

BOOL CALLBACK IWADBoxCallback(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND ctrl;
	int i;

	switch (message)
	{
	case WM_INITDIALOG:
		// Add our program name to the window title
		{
			WCHAR label[256];
			FString newlabel;

			GetWindowTextW(hDlg, label, countof(label));
			FString alabel(label);
			newlabel.Format(GAMENAME " %s: %s", GetVersionString(), alabel.GetChars());
			auto wlabel = newlabel.WideString();
			SetWindowTextW(hDlg, wlabel.c_str());
		}

		// [SP] Upstreamed from Zandronum
		char	szString[256];

		// Check the current video settings.
		SendDlgItemMessage( hDlg, IDC_WELCOME_FULLSCREEN, BM_SETCHECK, vid_fullscreen ? BST_CHECKED : BST_UNCHECKED, 0 );
		switch (vid_preferbackend)
		{
		case 1:
			SendDlgItemMessage( hDlg, IDC_WELCOME_VULKAN2, BM_SETCHECK, BST_CHECKED, 0 );
			break;
		case 2:
			SendDlgItemMessage( hDlg, IDC_WELCOME_VULKAN3, BM_SETCHECK, BST_CHECKED, 0 );
			break;
#ifdef HAVE_GLES2
		case 3:
			SendDlgItemMessage( hDlg, IDC_WELCOME_VULKAN4, BM_SETCHECK, BST_CHECKED, 0 );
			break;
#endif			
		default:
			SendDlgItemMessage( hDlg, IDC_WELCOME_VULKAN1, BM_SETCHECK, BST_CHECKED, 0 );
			break;
		}


		// [SP] This is our's
		SendDlgItemMessage( hDlg, IDC_WELCOME_NOAUTOLOAD, BM_SETCHECK, disableautoload ? BST_CHECKED : BST_UNCHECKED, 0 );
		SendDlgItemMessage( hDlg, IDC_WELCOME_LIGHTS, BM_SETCHECK, autoloadlights ? BST_CHECKED : BST_UNCHECKED, 0 );
		SendDlgItemMessage( hDlg, IDC_WELCOME_BRIGHTMAPS, BM_SETCHECK, autoloadbrightmaps ? BST_CHECKED : BST_UNCHECKED, 0 );
		SendDlgItemMessage( hDlg, IDC_WELCOME_WIDESCREEN, BM_SETCHECK, autoloadwidescreen ? BST_CHECKED : BST_UNCHECKED, 0 );

		// Set up our version string.
		sprintf(szString, "Version %s.", GetVersionString());
		SetDlgItemTextA (hDlg, IDC_WELCOME_VERSION, szString);

		// Populate the list with all the IWADs found
		ctrl = GetDlgItem(hDlg, IDC_IWADLIST);
		for (i = 0; i < NumWads; i++)
		{
			const char *filepart = strrchr(WadList[i].Path, '/');
			if (filepart == NULL)
				filepart = WadList[i].Path;
			else
				filepart++;

			FString work;
			if (*filepart) work.Format("%s (%s)", WadList[i].Name.GetChars(), filepart);
			else work = WadList[i].Name.GetChars();
			std::wstring wide = work.WideString();
			SendMessage(ctrl, LB_ADDSTRING, 0, (LPARAM)wide.c_str());
			SendMessage(ctrl, LB_SETITEMDATA, i, (LPARAM)i);
		}
		SendMessage(ctrl, LB_SETCURSEL, DefaultWad, 0);
		SetFocus(ctrl);
		// Set the state of the "Don't ask me again" checkbox
		ctrl = GetDlgItem(hDlg, IDC_DONTASKIWAD);
		SendMessage(ctrl, BM_SETCHECK, queryiwad ? BST_UNCHECKED : BST_CHECKED, 0);
		// Make sure the dialog is in front. If SHIFT was pressed to force it visible,
		// then the other window will normally be on top.
		SetForegroundWindow(hDlg);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog (hDlg, -1);
		}
		else if (LOWORD(wParam) == IDOK ||
			(LOWORD(wParam) == IDC_IWADLIST && HIWORD(wParam) == LBN_DBLCLK))
		{
			SetQueryIWad(hDlg);
			// [SP] Upstreamed from Zandronum
			vid_fullscreen = SendDlgItemMessage( hDlg, IDC_WELCOME_FULLSCREEN, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
#ifdef HAVE_GLES2
			if (SendDlgItemMessage(hDlg, IDC_WELCOME_VULKAN4, BM_GETCHECK, 0, 0) == BST_CHECKED)
				vid_preferbackend = 3;
			else 
#endif
			if (SendDlgItemMessage(hDlg, IDC_WELCOME_VULKAN3, BM_GETCHECK, 0, 0) == BST_CHECKED)
				vid_preferbackend = 2;
			else if (SendDlgItemMessage(hDlg, IDC_WELCOME_VULKAN2, BM_GETCHECK, 0, 0) == BST_CHECKED)
				vid_preferbackend = 1;
			else if (SendDlgItemMessage(hDlg, IDC_WELCOME_VULKAN1, BM_GETCHECK, 0, 0) == BST_CHECKED)
				vid_preferbackend = 0;

			// [SP] This is our's.
			disableautoload = SendDlgItemMessage( hDlg, IDC_WELCOME_NOAUTOLOAD, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
			autoloadlights = SendDlgItemMessage( hDlg, IDC_WELCOME_LIGHTS, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
			autoloadbrightmaps = SendDlgItemMessage( hDlg, IDC_WELCOME_BRIGHTMAPS, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
			autoloadwidescreen = SendDlgItemMessage( hDlg, IDC_WELCOME_WIDESCREEN, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
			ctrl = GetDlgItem (hDlg, IDC_IWADLIST);
			EndDialog(hDlg, SendMessage (ctrl, LB_GETCURSEL, 0, 0));
		}
		break;
	}
	return FALSE;
}

//==========================================================================
//
// I_PickIWad
//
// Open a dialog to pick the IWAD, if there is more than one found.
//
//==========================================================================

int I_PickIWad(WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
	int vkey;

	if (stricmp(queryiwad_key, "shift") == 0)
	{
		vkey = VK_SHIFT;
	}
	else if (stricmp(queryiwad_key, "control") == 0 || stricmp (queryiwad_key, "ctrl") == 0)
	{
		vkey = VK_CONTROL;
	}
	else
	{
		vkey = 0;
	}
	if (showwin || (vkey != 0 && GetAsyncKeyState(vkey)))
	{
		WadList = wads;
		NumWads = numwads;
		DefaultWad = defaultiwad;

		return (int)DialogBox(g_hInst, MAKEINTRESOURCE(IDD_IWADDIALOG),
			(HWND)mainwindow.GetHandle(), (DLGPROC)IWADBoxCallback);
	}
	return defaultiwad;
}

//==========================================================================
//
// I_SetCursor
//
// Returns true if the cursor was successfully changed.
//
//==========================================================================

bool I_SetCursor(FGameTexture *cursorpic)
{
	HCURSOR cursor;

	if (cursorpic != NULL && cursorpic->isValid())
	{
		auto image = cursorpic->GetTexture()->GetBgraBitmap(nullptr);
		// Must be no larger than 32x32. (is this still necessary?
		if (image.GetWidth() > 32 || image.GetHeight() > 32)
		{
			return false;
		}
		// Fixme: This should get a raw image, not a texture. (Once raw images get implemented.)
		int lo = cursorpic->GetTexelLeftOffset();
		int to = cursorpic->GetTexelTopOffset();

		cursor = CreateAlphaCursor(image, lo, to);
		if (cursor == NULL)
		{
			cursor = CreateCompatibleCursor(image, lo, to);
		}
		if (cursor == NULL)
		{
			return false;
		}
		// Replace the existing cursor with the new one.
		DestroyCustomCursor();
		CustomCursor = cursor;
	}
	else
	{
		DestroyCustomCursor();
		cursor = LoadCursor(NULL, IDC_ARROW);
	}
	SetClassLongPtr(mainwindow.GetHandle(), GCLP_HCURSOR, (LONG_PTR)cursor);
	if (NativeMouse)
	{
		POINT pt;
		RECT client;

		// If the mouse pointer is within the window's client rect, set it now.
		if (GetCursorPos(&pt) && GetClientRect(mainwindow.GetHandle(), &client) &&
			ClientToScreen(mainwindow.GetHandle(), (LPPOINT)&client.left) &&
			ClientToScreen(mainwindow.GetHandle(), (LPPOINT)&client.right))
		{
			if (pt.x >= client.left && pt.x < client.right &&
				pt.y >= client.top && pt.y < client.bottom)
			{
				SetCursor(cursor);
			}
		}
	}
	return true;
}

//==========================================================================
//
// CreateCompatibleCursor
//
// Creates a cursor with a 1-bit alpha channel.
//
//==========================================================================

static HCURSOR CreateCompatibleCursor(FBitmap &bmp, int leftofs, int topofs)
{
	int picwidth = bmp.GetWidth();
	int picheight = bmp.GetHeight();

	// Create bitmap masks for the cursor from the texture.
	HDC dc = GetDC(NULL);
	if (dc == NULL)
	{
		return nullptr;
	}
	HDC and_mask_dc = CreateCompatibleDC(dc);
	HDC xor_mask_dc = CreateCompatibleDC(dc);
	HBITMAP and_mask = CreateCompatibleBitmap(dc, 32, 32);
	HBITMAP xor_mask = CreateCompatibleBitmap(dc, 32, 32);
	ReleaseDC(NULL, dc);

	SelectObject(and_mask_dc, and_mask);
	SelectObject(xor_mask_dc, xor_mask);

	// Initialize with an invisible cursor.
	SelectObject(and_mask_dc, GetStockObject(WHITE_PEN));
	SelectObject(and_mask_dc, GetStockObject(WHITE_BRUSH));
	Rectangle(and_mask_dc, 0, 0, 32, 32);
	SelectObject(xor_mask_dc, GetStockObject(BLACK_PEN));
	SelectObject(xor_mask_dc, GetStockObject(BLACK_BRUSH));
	Rectangle(xor_mask_dc, 0, 0, 32, 32);

	const uint8_t *pixels = bmp.GetPixels();

	// Copy color data from the source texture to the cursor bitmaps.
	for (int y = 0; y < picheight; ++y)
	{
		for (int x = 0; x < picwidth; ++x)
		{
			const uint8_t *bgra = &pixels[x*4 + y*bmp.GetPitch()];
			if (bgra[3] != 0)
			{
				SetPixelV(and_mask_dc, x, y, RGB(0,0,0));
				SetPixelV(xor_mask_dc, x, y, RGB(bgra[2], bgra[1], bgra[0]));
			}
		}
	}
	DeleteDC(and_mask_dc);
	DeleteDC(xor_mask_dc);

	// Create the cursor from the bitmaps.
	return CreateBitmapCursor(leftofs, topofs, and_mask, xor_mask);
}

//==========================================================================
//
// CreateAlphaCursor
//
// Creates a cursor with a full alpha channel.
//
//==========================================================================

static HCURSOR CreateAlphaCursor(FBitmap &source, int leftofs, int topofs)
{
	HDC dc;
	BITMAPV5HEADER bi;
	HBITMAP color, mono;
	void *bits;

	// Find closest integer scale factor for the monitor DPI
	HDC screenDC = GetDC(0);
	int dpi = GetDeviceCaps(screenDC, LOGPIXELSX);
	int scale = max((dpi + 96 / 2 - 1) / 96, 1);
	ReleaseDC(0, screenDC);

	memset(&bi, 0, sizeof(bi));
	bi.bV5Size = sizeof(bi);
	bi.bV5Width = 32 * scale;
	bi.bV5Height = 32 * scale;
	bi.bV5Planes = 1;
	bi.bV5BitCount = 32;
	bi.bV5Compression = BI_BITFIELDS;
	bi.bV5RedMask   = 0x00FF0000;
	bi.bV5GreenMask = 0x0000FF00;
	bi.bV5BlueMask  = 0x000000FF;
	bi.bV5AlphaMask = 0xFF000000;

	dc = GetDC(NULL);
	if (dc == NULL)
	{
		return NULL;
	}

	// Create the DIB section with an alpha channel.
	color = CreateDIBSection(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
	ReleaseDC(NULL, dc);

	if (color == NULL)
	{
		return NULL;
	}

	// Create an empty mask bitmap, since CreateIconIndirect requires this.
	mono = CreateBitmap(32 * scale, 32 * scale, 1, 1, NULL);
	if (mono == NULL)
	{
		DeleteObject(color);
		return NULL;
	}

	// Copy cursor to the color bitmap. Note that GDI bitmaps are upside down compared
	// to normal conventions, so we create the FBitmap pointing at the last row and use
	// a negative pitch so that Blit will use GDI's orientation.
	if (scale == 1)
	{
		FBitmap bmp((uint8_t *)bits + 31 * 32 * 4, -32 * 4, 32, 32);
		bmp.Blit(0, 0, source);
	}
	else
	{
		TArray<uint32_t> unscaled;
		unscaled.Resize(32 * 32);
		for (int i = 0; i < 32 * 32; i++) unscaled[i] = 0;
		FBitmap bmp((uint8_t *)&unscaled[0] + 31 * 32 * 4, -32 * 4, 32, 32);
		bmp.Blit(0, 0, source);
		uint32_t *scaled = (uint32_t*)bits;
		for (int y = 0; y < 32 * scale; y++)
		{
			for (int x = 0; x < 32 * scale; x++)
			{
				scaled[x + y * 32 * scale] = unscaled[x / scale + y / scale * 32];
			}
		}
	}

	return CreateBitmapCursor(leftofs * scale, topofs * scale, mono, color);
}

//==========================================================================
//
// CreateBitmapCursor
//
// Create the cursor from the bitmaps. Deletes the bitmaps before returning.
//
//==========================================================================

static HCURSOR CreateBitmapCursor(int xhot, int yhot, HBITMAP and_mask, HBITMAP color_mask)
{
	ICONINFO iconinfo =
	{
		FALSE,		// fIcon
		(DWORD)xhot,	// xHotspot
		(DWORD)yhot,	// yHotspot
		and_mask,	// hbmMask
		color_mask	// hbmColor
	};
	HCURSOR cursor = CreateIconIndirect(&iconinfo);

	// Delete the bitmaps.
	DeleteObject(and_mask);
	DeleteObject(color_mask);

	return cursor;
}

//==========================================================================
//
// DestroyCustomCursor
//
//==========================================================================

void DestroyCustomCursor()
{
	if (CustomCursor != NULL)
	{
		DestroyCursor(CustomCursor);
		CustomCursor = NULL;
	}
}

//==========================================================================
//
// I_WriteIniFailed
//
// Display a message when the config failed to save.
//
//==========================================================================

bool I_WriteIniFailed()
{
	char *lpMsgBuf;
	FString errortext;

	FormatMessageA (FORMAT_MESSAGE_ALLOCATE_BUFFER | 
					FORMAT_MESSAGE_FROM_SYSTEM | 
					FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPSTR)&lpMsgBuf,
		0,
		NULL 
	);
	errortext.Format ("The config file %s could not be written:\n%s", GameConfig->GetPathName(), lpMsgBuf);
	LocalFree (lpMsgBuf);
	return MessageBoxA(mainwindow.GetHandle(), errortext.GetChars(), GAMENAME " configuration not saved", MB_ICONEXCLAMATION | MB_RETRYCANCEL) == IDRETRY;
}


//==========================================================================
//
// I_MakeRNGSeed
//
// Returns a 32-bit random seed, preferably one with lots of entropy.
//
//==========================================================================

unsigned int I_MakeRNGSeed()
{
	unsigned int seed;

	// If RtlGenRandom is available, use that to avoid increasing the
	// working set by pulling in all of the crytographic API.
	HMODULE advapi = GetModuleHandleA("advapi32.dll");
	if (advapi != NULL)
	{
		BOOLEAN (APIENTRY *RtlGenRandom)(void *, ULONG) =
			(BOOLEAN (APIENTRY *)(void *, ULONG))GetProcAddress(advapi, "SystemFunction036");
		if (RtlGenRandom != NULL)
		{
			if (RtlGenRandom(&seed, sizeof(seed)))
			{
				return seed;
			}
		}
	}

	// Use the full crytographic API to produce a seed. If that fails,
	// time() is used as a fallback.
	HCRYPTPROV prov;

	if (!CryptAcquireContext(&prov, NULL, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		return (unsigned int)time(NULL);
	}
	if (!CryptGenRandom(prov, sizeof(seed), (uint8_t *)&seed))
	{
		seed = (unsigned int)time(NULL);
	}
	CryptReleaseContext(prov, 0);
	return seed;
}

//==========================================================================
//
// I_GetLongPathName
//
// Returns the long version of the path, or the original if there isn't
// anything worth changing.
//
//==========================================================================

FString I_GetLongPathName(const FString &shortpath)
{
	std::wstring wshortpath = shortpath.WideString();
	DWORD buffsize = GetLongPathNameW(wshortpath.c_str(), nullptr, 0);
	if (buffsize == 0)
	{ // nothing to change (it doesn't exist, maybe?)
		return shortpath;
	}
	TArray<WCHAR> buff(buffsize, true);
	DWORD buffsize2 = GetLongPathNameW(wshortpath.c_str(), buff.Data(), buffsize);
	if (buffsize2 >= buffsize)
	{ // Failure! Just return the short path
		return shortpath;
	}
	FString longpath(buff.Data());
	return longpath;
}

#ifdef _USING_V110_SDK71_
//==========================================================================
//
// _stat64i32
//
// Work around an issue where stat() function doesn't work 
// with Windows XP compatible toolset.
// It uses GetFileInformationByHandleEx() which requires Windows Vista.
//
//==========================================================================

int _wstat64i32(const wchar_t *path, struct _stat64i32 *buffer)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if(!GetFileAttributesExW(path, GetFileExInfoStandard, &data))
		return -1;

	buffer->st_ino = 0;
	buffer->st_mode = ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? S_IFDIR : S_IFREG)|
	                  ((data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? S_IREAD : S_IREAD|S_IWRITE);
	buffer->st_dev = buffer->st_rdev = 0;
	buffer->st_nlink = 1;
	buffer->st_uid = 0;
	buffer->st_gid = 0;
	buffer->st_size = data.nFileSizeLow;
	buffer->st_atime = (*(uint64_t*)&data.ftLastAccessTime) / 10000000 - 11644473600LL;
	buffer->st_mtime = (*(uint64_t*)&data.ftLastWriteTime) / 10000000 - 11644473600LL;
	buffer->st_ctime = (*(uint64_t*)&data.ftCreationTime) / 10000000 - 11644473600LL;
	return 0;
}
#endif

struct NumaNode
{
	uint64_t affinityMask = 0;
	int threadCount = 0;
};
static TArray<NumaNode> numaNodes;

static void SetupNumaNodes()
{
	if (numaNodes.Size() == 0)
	{
		// Query processors in the system
		DWORD_PTR processMask = 0, systemMask = 0;
		BOOL result = GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask);
		if (result)
		{
			// Find the numa node each processor belongs to
			std::map<int, NumaNode> nodes;
			for (int i = 0; i < sizeof(DWORD_PTR) * 8; i++)
			{
				DWORD_PTR processorMask = (((DWORD_PTR)1) << i);
				if (processMask & processorMask)
				{
					UCHAR nodeNumber = 0;
					result = GetNumaProcessorNode(i, &nodeNumber);
					if (nodeNumber != 0xff)
					{
						nodes[nodeNumber].affinityMask |= (uint64_t)processorMask;
						nodes[nodeNumber].threadCount++;
					}
				}
			}

			// Convert map to a list
			for (const auto &it : nodes)
			{
				numaNodes.Push(it.second);
			}
		}

		// Fall back to a single node if something went wrong
		if (numaNodes.Size() == 0)
		{
			NumaNode node;
			node.threadCount = std::thread::hardware_concurrency();
			if (node.threadCount == 0)
				node.threadCount = 1;
			numaNodes.Push(node);
		}
	}
}

int I_GetNumaNodeCount()
{
	SetupNumaNodes();
	return numaNodes.Size();
}

int I_GetNumaNodeThreadCount(int numaNode)
{
	SetupNumaNodes();
	return numaNodes[numaNode].threadCount;
}

void I_SetThreadNumaNode(std::thread &thread, int numaNode)
{
	if (numaNodes.Size() > 1)
	{
		HANDLE handle = (HANDLE)thread.native_handle();
		SetThreadAffinityMask(handle, (DWORD_PTR)numaNodes[numaNode].affinityMask);
	}
}
