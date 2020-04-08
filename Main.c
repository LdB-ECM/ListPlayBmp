#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>

#pragma comment( lib, "comctl32.lib")
#pragma comment( lib,"Msimg32.lib")  
 
// HINSTANCE of our window
static HINSTANCE hInst = 0;

// Loaded Jpeg details
static HDC ImageDc = 0;
static uint16_t ImageWth = 0;
static uint16_t ImageHt = 0;



// ** LdB the list box had an ID of 7000 it coccurs a few places I dragged it up here to a constant which is how it should be done
//  I have replaced the number 7000 where it occurred
#define ID_LISTBOX 7000
// this is the second listbox ID
#define ID_LISTBOX1 7001
 

/*-[ CreateFullFilename ]---------------------------------------------------}
.  Creates a full filename from a shortname.
.--------------------------------------------------------------------------*/
bool CreateFullFilename (const char* ShortName, char* NameBuffer, uint16_t BufSize)
{
	if (NameBuffer && ShortName && BufSize > 0)							// Check pointers and buffer size
	{
		uint16_t len = (uint16_t)GetModuleFileName(GetModuleHandle(NULL),
			NameBuffer, BufSize);										// Transfer full path to EXE to buffer
		if (len && GetLastError() == 0)									// It copied name to buffer and it fitted
		{
			char* pStr = strrchr(NameBuffer, '\\');						// Find directory slash between EXE name and path		
			if (pStr) *(++pStr) = '\0';									// Terminate the string there
			if (strcat_s(NameBuffer, BufSize, ShortName) == 0)			// Add shortname to path and check it worked  
				return true;											// Return success
		}
	}
	return false;														// Any failure will return false
}

/*-[ MemDCFromJPGFile ]-----------------------------------------------------}
.  Creates a Memory Device context from the the JPEG filename. Due to the
.  way OLE works the filename must be fully qualified (ie c:\folder\a.jpg)
.  Width and height of the JPG can be return by providing valid pointers or
.  NULL can be used if the values are not required. The function will return
.  NULL for any error, otherwise returns a valid Memory Device Contest.
.  Caller must take responsibility for disposal of the Device Context.
.--------------------------------------------------------------------------*/
#include "olectl.h"													// OLE is used for JPG image loading
#define HIMETRIC_PER_INCH ( 2540 )
HDC MemDCFromJPGFile (const char* Filename, uint16_t* Wth, uint16_t* Ht)
{
	HDC MemDC = 0;													// Preset return value to fail
	if (Filename)													// Check filename pointer valid 
	{
		IPicture* Ipic = NULL;
		WCHAR OleFQN[_MAX_PATH];
		size_t len = strlen(Filename);								// Hold the filename string length
		MultiByteToWideChar(850, 0, Filename, len + 1,
			&OleFQN[0], _countof(OleFQN));							// Convert filename to unicode
		HRESULT hr = OleLoadPicturePath(OleFQN, NULL, 0, 0,
			&IID_IPicture, (LPVOID*)&Ipic);							// Load the picture
		if ((hr == S_OK) && (Ipic != 0)) 							// Check picture loaded okay	
		{
			SIZE sizeInHiMetric;
			HDC Dc = GetDC(NULL);									// Get screen DC
			int nPixelsPerInchX = GetDeviceCaps(Dc, LOGPIXELSX);	// Screen X pixels per inch 
			int nPixelsPerInchY = GetDeviceCaps(Dc, LOGPIXELSY);	// Screen Y pixels per inch
			ReleaseDC(NULL, Dc);									// Release screen DC
			Ipic->lpVtbl->get_Width(Ipic, &sizeInHiMetric.cx);		// Get picture witdh in HiMetric
			Ipic->lpVtbl->get_Height(Ipic, &sizeInHiMetric.cy);		// Get picture height in HiMetric
			int Tw = (nPixelsPerInchX * sizeInHiMetric.cx +
				HIMETRIC_PER_INCH / 2) / HIMETRIC_PER_INCH;			// Calculate picture width in pixels
			int Th = (nPixelsPerInchY * sizeInHiMetric.cy +
				HIMETRIC_PER_INCH / 2) / HIMETRIC_PER_INCH;			// Calculate picture height in pixels
			if (Wth) (*Wth) = Tw;									// User requested width result
			if (Ht) (*Ht) = Th;										// User request height result
			MemDC = CreateCompatibleDC(0);							// Create a memory device context
			if (MemDC)												// Check memory context created				
			{
				// Create A Temporary Bitmap
				BITMAPINFO	bi = { 0 };								// The Type Of Bitmap We Request
				DWORD* pBits = 0;									// Pointer To The Bitmap Bits
				bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);		// Set Structure Size
				bi.bmiHeader.biBitCount = 32;						// 32 Bit
				bi.bmiHeader.biWidth = Tw;							// Image width
				bi.bmiHeader.biHeight = Th;							// Make Image Top Up (Positive Y-Axis)
				bi.bmiHeader.biCompression = BI_RGB;				// RGB Encoding
				bi.bmiHeader.biPlanes = 1;							// 1 Bitplane
				HBITMAP bmp = CreateDIBSection(MemDC, &bi,
					DIB_RGB_COLORS, (void**)&pBits, 0, 0);			// Create a DIB section
				if (bmp)
				{
					SelectObject(MemDC, bmp);						// Select the bitmap to the memory context															
					hr = Ipic->lpVtbl->Render(Ipic, MemDC, 0, 0, Tw, Th,
						0, sizeInHiMetric.cy, sizeInHiMetric.cx,
						-sizeInHiMetric.cy, NULL);					// Render the image to the DC
					DeleteObject(bmp);								// Delete the bitmap section we are finished with
				}
				else {												// DIB section create failed	
					DeleteObject(MemDC);							// Delete the memory DC
					MemDC = 0;										// Reset the memory DC back to zero to fail out
				}
			}
			Ipic->lpVtbl->Release(Ipic);							// Finised with Ipic
		}
	}
	return MemDC;													// Return the memory context
}

/*--------------------------------------------------------------------------
  Pass in any window handle and a tooltip string and this function will set
  the create a tooltip to display on the window if you hover over it.
  -------------------------------------------------------------------------*/
HWND AddToolTip (HWND hWnd,											// Handle for window to put tooltip over 							 
				 char* tooltip) {									// Text the tool tip should say
	TOOLINFO ti;
	HWND TTWnd;

	if (tooltip == 0) return (0);									// Check we have a tooltip
	InitCommonControls(); 	     									// Check common controls are initialized
	TTWnd = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS,
		NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, hWnd, 0, 0, 0);								// Create tooltip window
	memset(&ti, 0, sizeof(TOOLINFO));								// Clear structure
    ti.cbSize = sizeof(TOOLINFO);									// Size of structure
    ti.uFlags = TTF_SUBCLASS;										// Class is subclass
    ti.hwnd = hWnd;													// Parent window
    ti.hinst = 0;													// This instance
    ti.uId = 0;														// No uid
    ti.lpszText = tooltip;											// Transfer the text pointer
    GetClientRect (hWnd, &ti.rect);									// Tooltip to cover whole window
    SendMessage(TTWnd, TTM_ADDTOOL, 0, (LPARAM) &ti);				// Add tooltip
	return(TTWnd);                                                  // Return the tooltip window
}



// LdB this is the property title of the FONT to attach to our window
#define TEXTPROP "FONT"

// subclass procedure for transparent tree
static LRESULT CALLBACK TreeProc( HWND hwnd, UINT message, 
    WPARAM wParam, LPARAM lParam, 
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData )
{
	switch (message)
	{
		// handle messages that paint tree without WM_PAINT
	case WM_TIMER:  // handles autoscrolling when item is partially visible
	case TVM_DELETEITEM:
	case TVM_INSERTITEM:
	case WM_MOUSEWHEEL:
	case WM_HSCROLL:
	case WM_VSCROLL:
	{
		SendMessage(hwnd, WM_SETREDRAW, (WPARAM)FALSE, 0);
		LRESULT lres = DefSubclassProc(hwnd, message, wParam, lParam);
		SendMessage(hwnd, WM_SETREDRAW, (WPARAM)TRUE, 0);
		RedrawWindow(hwnd, NULL, NULL,
			RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
		return lres;
	}
	case WM_PAINT:
	{
		// usual stuff
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		// Simply get printclient to paint  
		SendMessage(hwnd, WM_PRINTCLIENT, (WPARAM)hdc, (LPARAM)(PRF_CLIENT));
		EndPaint(hwnd, &ps);
	}
	return 0L;
	case WM_PRINTCLIENT:
	{
		BITMAPINFO bmi;
		// Get parent client co-ordinates
		RECT rc;
		GetClientRect(GetParent(hwnd), &rc);
		// Create a memory DC
		HDC memDC = CreateCompatibleDC(0);
		// Create a DIB header for parent
		memset(&bmi, 0, sizeof(bmi));
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = rc.right - rc.left;
		bmi.bmiHeader.biHeight = rc.bottom - rc.top;
		bmi.bmiHeader.biBitCount = 24;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biCompression = BI_RGB;
		// Create a DIB bitmap
		HBITMAP tempBmp = CreateDIBSection(0, &bmi, DIB_RGB_COLORS, 0, 0, 0);
		// Select tempBmp onto DC to force size and DIB change on the DC
		SelectObject(memDC, tempBmp);
		// Put parent background on our memory DC
		SendMessage(GetParent(hwnd), WM_PRINTCLIENT, (WPARAM)memDC, (LPARAM)(PRF_CLIENT));
		// Now we can dispose of the DIB bitmap no longer needed
		DeleteObject(tempBmp);
		// map tree's coordinates to parent window
		POINT ptTreeUL;
		ptTreeUL.x = 0;
		ptTreeUL.y = 0;
		ClientToScreen(hwnd, &ptTreeUL);
		ScreenToClient(GetParent(hwnd), &ptTreeUL);
		GetClientRect(hwnd, &rc);
		// Transfer parent background to given DC
		BitBlt((HDC)wParam, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
			memDC, ptTreeUL.x, ptTreeUL.y, SRCCOPY);
		// Okay get treeview to draw on our memory DC
		DefSubclassProc(hwnd, WM_PRINTCLIENT, (WPARAM)memDC, (LPARAM)(PRF_CLIENT));
		// Transfer the treeview DC onto finalDC excluding pink
		TransparentBlt((HDC)wParam, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
			memDC, 0, 0, rc.right - rc.left, rc.bottom - rc.top, RGB(0xFF, 0x00, 0xFF));
		// Finished with MemDC 
		DeleteObject(memDC);
		return (0);
	}
	case WM_ERASEBKGND:
		return TRUE;
	case WM_SETFONT:
	{
		LOGFONT lf;
		HFONT OldFont, Newfont;
		GetObject((HFONT)wParam, sizeof(lf), &lf);              // Get proposed font
		if (lf.lfQuality != NONANTIALIASED_QUALITY) {            // If its antialiased we can't use
			lf.lfQuality = NONANTIALIASED_QUALITY;              // So change it to non antialiased
			Newfont = CreateFontIndirect(&lf);                  // Create a new font 
			OldFont = (HFONT)GetProp(hwnd, TEXTPROP);			// Get the old font
			if (OldFont != 0) DeleteObject(OldFont);            // If valid delete it
			SetProp(hwnd, TEXTPROP, (HANDLE)Newfont);           // Set new font to property
		}
		else Newfont = (HFONT)wParam;                        // Use the font provided
		return DefSubclassProc(hwnd, WM_SETFONT, (WPARAM)Newfont, lParam); // Treat it as usual
	}
	case WM_NCDESTROY:
	{
		HFONT Font = (HFONT)GetProp(hwnd, TEXTPROP);           // Fetch the font property
		if (Font != 0) DeleteObject(Font);                      // Delete the font
		RemoveProp(hwnd, TEXTPROP);                             // Remove the property   
		RemoveWindowSubclass(hwnd, TreeProc, 0);
		return DefSubclassProc(hwnd, message, wParam, lParam);
	}
	}
    return DefSubclassProc( hwnd, message, wParam, lParam);
}
 

// ** LdB Consolidate function to create a listbox and do all the repeat stuff
HWND CreateListBox (HWND parent,                               // Parent window to insert this control in
					char* title,			                   // List box title text
					int x, int y,							   // x,y co-ordinate of parent for the insert
					int cx, int cy,						       // Width and Height of the control
					int id,									   // Id of the control
 				    HFONT hFont) {							   // Handle to any special font (0 = default)
	DWORD dwStyle;
	HWND TreeView;
  
	TreeView = CreateWindowEx(0, WC_TREEVIEW, title, 
		WS_VISIBLE | WS_CHILD | WS_BORDER 
		| TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT, 
		x, y, cx, cy,	parent, (HMENU)id, 0, NULL);

	dwStyle = GetWindowLong( TreeView , GWL_STYLE);
	dwStyle |= TVS_CHECKBOXES;
	SetWindowLongPtr( TreeView , GWL_STYLE, dwStyle );
	SetWindowSubclass( TreeView, TreeProc, 0, 0 );
	TreeView_SetBkColor(TreeView, RGB(0xFF, 0, 0xFF));
	TreeView_SetTextColor(TreeView, RGB(0xFF, 0xFF, 0xFF));
	if (hFont == 0) hFont = (HFONT) SendMessage(TreeView, WM_GETFONT, 0, 0);
	SendMessage(TreeView, WM_SETFONT, (WPARAM)hFont, FALSE);


	HIMAGELIST hImages = ImageList_Create( 16, 16, ILC_MASK, 1, 0 );

    // load system icon so we can dodge the deletion and rc.file
	HICON hiBG = (HICON)( LoadImage( 0, 
		MAKEINTRESOURCE(IDI_WARNING), 
        IMAGE_ICON, 0, 0, LR_SHARED ) );
 
	ImageList_AddIcon( hImages, hiBG );
 
	TreeView_SetImageList(TreeView, hImages, TVSIL_NORMAL);
	return (TreeView);
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
	case WM_CREATE:
		{

			HWND TreeView = CreateListBox(hwnd, "Tree View", 
				50, 10, 200, 200, ID_LISTBOX, 0);
		
			/************ add items and subitems **********/
 
			// add root item

			TVINSERTSTRUCT tvis = {0};
 
			tvis.item.mask = TVIF_TEXT;
			tvis.item.pszText = "This is root item";
			tvis.hInsertAfter = TVI_LAST;
			tvis.hParent = TVI_ROOT;
 
			HTREEITEM hRootItem = (HTREEITEM)(SendMessage( TreeView , TVM_INSERTITEM, 0, (LPARAM) &tvis));
 
			// add subitems for the hTreeItem

			for( int i = 0; i < 15; i++ )
			{
				memset( &tvis, 0, sizeof(tvis) );
 
				tvis.item.mask = TVIF_TEXT;
				tvis.item.pszText = "This is subitem";
				tvis.hInsertAfter = TVI_LAST;
				tvis.hParent = hRootItem;
 
				SendMessage(TreeView , TVM_INSERTITEM, 0, (LPARAM)&tvis);
			}

			TreeView = CreateListBox(hwnd, "Tree View 1", 
				300, 10, 200, 200, ID_LISTBOX1,  0);
			AddToolTip(TreeView, "This is list 2");
		
			/************ add items and subitems **********/
 
			// add root item
			memset( &tvis, 0, sizeof(tvis) );
			tvis.item.mask = TVIF_TEXT;
			tvis.item.pszText = "2nd list root item";
			tvis.hInsertAfter = TVI_LAST;
			tvis.hParent = TVI_ROOT;
 
			hRootItem = (HTREEITEM)(SendMessage( TreeView , TVM_INSERTITEM, 0, (LPARAM) &tvis));
 
			// add subitems for the hTreeItem

			for( int i = 0; i < 15; i++ )
			{
				memset( &tvis, 0, sizeof(tvis) );
 
				tvis.item.mask = TVIF_TEXT;
				tvis.item.pszText = "List2 subitem";
				tvis.hInsertAfter = TVI_LAST;
				tvis.hParent = hRootItem;
				SendMessage(TreeView , TVM_INSERTITEM, 0, (LPARAM)&tvis);
			}

		}
		return 0L;
	case WM_PRINTCLIENT:
		{
			RECT r;
			GetClientRect( hwnd, &r );
 
			StretchBlt((HDC)wParam, r.left, r.top, r.right - r.left, r.bottom-r.top, ImageDc, 0, 0, ImageWth, ImageHt, SRCCOPY);

 			return 0L;
		}
		break;
		// **LdB We accept this message so we can set a minimum window size the users can drag it down too
		case WM_GETMINMAXINFO:
			{
				LPMINMAXINFO lpInfo = (LPMINMAXINFO)lParam;
				if (lpInfo){
					lpInfo->ptMinTrackSize.x = 550;
				    lpInfo->ptMinTrackSize.y = 300;
				}
			}
			return 0;
		/** LdB  **/
		// These next two messages are better to use rather than WM_MOVE/WM_SIZE.
		// Remember WM_MOVE/WM_SIZE are from 16bit windows. In 32bit windows the window
		// manager only sends these two messages and the DefWindowProc() handler actually
		// accepts them and converts them to WM_MOVE/WM_SIZE.
		// 
		// We accept this so we can scale our controls to the client size.
		case WM_WINDOWPOSCHANGING:
		case WM_WINDOWPOSCHANGED:
			{
				HDWP hDWP;
				RECT rc;
				
				// Create a deferred window handle.
				// **LdB Deferring 2 child control at the moment (you now have 2 controls in window listbox, listbox1)
				if(hDWP = BeginDeferWindowPos(2)){ 
					GetClientRect(hwnd, &rc);
					
					// **LdB This time I use the SWP_NOMOVE & SWP_NOSIZE so the position and size isn't change 
					// from what you set in the WM_CREATE
					hDWP = DeferWindowPos(hDWP, GetDlgItem(hwnd, ID_LISTBOX), NULL,
						0, 0, 0, 0, SWP_NOZORDER | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE);

					// **LdB This time I use the SWP_NOMOVE & SWP_NOSIZE so the position and size isn't change 
					// from what you set in the WM_CREATE
					hDWP = DeferWindowPos(hDWP, GetDlgItem(hwnd, ID_LISTBOX1), NULL,
						0, 0, 0, 0, SWP_NOZORDER | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE);

					// Resize all windows under the deferred window handled at the same time.
					EndDeferWindowPos(hDWP);
					
	                // Now invalidate the window area to force redraw
					InvalidateRect(hwnd, 0, TRUE);
				}
			}
			return 0;
	case WM_ERASEBKGND:
		return (FALSE);
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint( hwnd, &ps );

			// LdB Simply get printclient to paint you already do all the gradient there  
  			SendMessage(hwnd, WM_PRINTCLIENT,(WPARAM) hdc, (LPARAM)(PRF_CLIENT) ); 
			EndPaint( hwnd, &ps );
		}
		return 0L;
	case WM_CLOSE:
		{
			// delete tree's imagelist
			HIMAGELIST hImages = (HIMAGELIST)( 
                            SendMessage( GetDlgItem( hwnd, ID_LISTBOX ), 
                            TVM_GETIMAGELIST, 0, 0 ) );
 			ImageList_Destroy(hImages);
 
			DestroyWindow(hwnd);
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}
 
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX wc;
	HWND hwnd;
	MSG Msg;
 
	// initialize common controls
	INITCOMMONCONTROLSEX iccex;
	iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	iccex.dwICC = ICC_LISTVIEW_CLASSES | ICC_UPDOWN_CLASS | ICC_STANDARD_CLASSES ;
	InitCommonControlsEx(&iccex);

	char Filename[MAX_PATH];
	CreateFullFilename("beauty.jpg", Filename, MAX_PATH);
	ImageDc = MemDCFromJPGFile(Filename, &ImageWth, &ImageHt);
 
	hInst = hInstance;
 
	wc.cbSize		 = sizeof(WNDCLASSEX);
	wc.style		 = 0;
	wc.lpfnWndProc	 = WndProc;
	wc.cbClsExtra	 = 0;
	wc.cbWndExtra	 = 0;
	wc.hInstance	 = hInstance;
	wc.hIcon		 = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor		 = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = "Transparent TreeView";
	wc.hIconSm		 = LoadIcon(NULL, IDI_APPLICATION);
 
	if(!RegisterClassEx(&wc))
	{
		MessageBox(NULL, "Window Registration Failed!", "Error!", 
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
 
	// LdB rather than repeat the text I used the Classname from above less chance for a typo to get it wrong
	hwnd = CreateWindowEx( WS_EX_CLIENTEDGE, wc.lpszClassName,
		"The title of my window", 
                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 
		ImageWth+50, ImageHt+50, NULL, NULL, hInstance, NULL);
 
	if(hwnd == NULL)
	{
		MessageBox(NULL, "Window Creation Failed!", "Error!", 
                    MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
 
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);
 
	while(GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	return Msg.wParam;
}