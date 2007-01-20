// TermView.cpp : implementation of the CTermView class
//
#include "stdafx.h"

#include "WinUtils.h"

#include "TermView.h"
#include "ListDlg.h"
#include "MainFrm.h"
#include "SetBkDlg.h"
#include "CustomizeDlg.h"
#include "StringDlg.h"
#include "MemIniFile.h"
#include "StrUtils.h"

#include "SitePage.h"
#include "AutoReplyPage.h"
#include "GeneralPage.h"
#include "OtherPage.h"
#include "HyperLinkPage.h"
#include "ConnectPage.h"

#include "Clipboard.h"
#include "InputNameDlg.h"

#if defined	_COMBO_
	#include "../Combo/WebPageDlg.h"
	#include "../Combo/WebConn.h"
	#include "../Combo/WebCfgPage.h"
#endif

#include <wininet.h>
#include <afxtempl.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CFont fnt;

// The debugger can't handle symbols more than 255 characters long.
// STL often creates symbols longer than that.
// When symbols are longer than 255 characters, the warning is disabled.
#pragma warning(disable:4786)

/////////////////////////////////////////////////////////////////////////////
// CTermView

const UINT WM_FINDREPLACE = RegisterWindowMessage( FINDMSGSTRING );

inline void swap(long& v1,long& v2)		{	long t;	t=v1;	v1=v2;	v2=t;	}

BEGIN_MESSAGE_MAP(CTermView, CWnd)
	ON_MESSAGE(WM_SOCKET,OnSocket)
	//{{AFX_MSG_MAP(CTermView)
	ON_WM_ERASEBKGND()
	ON_WM_CHAR()
	ON_WM_KEYDOWN()
	ON_WM_LBUTTONDOWN()
	ON_WM_DESTROY()
	ON_WM_VSCROLL()
	ON_WM_TIMER()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_CONTEXTMENU()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_COMMAND(ID_PASTEFILE, OnEditPastefile)
	ON_COMMAND(ID_RECONNECT, OnReconnect)
	ON_WM_CREATE()
	ON_COMMAND(ID_SETBK, OnSetBkgnd)
	ON_COMMAND(ID_RIGHTNODBCS, OnRightNoDBCS)
	ON_COMMAND(ID_LEFTNODBCS, OnLeftNoDBCS)
	ON_COMMAND(ID_BACKNODBCS, OnBackspaceNoDBCS)
	ON_COMMAND(ID_SELALL_BUF, OnSelAllBuf)
	ON_WM_DESTROYCLIPBOARD()
	ON_WM_MOUSEWHEEL()
	ON_COMMAND(ID_SMOOTH_DRAW, OnSmoothDraw)
	ON_COMMAND(ID_CURCON_SETTINGS, OnCurConSettings)
	ON_COMMAND(ID_UPDATE_BBSLIST, OnUpdateBBSList)
	ON_COMMAND(ID_DISCONNECT, OnDisConnect)
	ON_COMMAND(ID_ANSICOPY, OnAnsiCopy)
	ON_COMMAND(ID_ANSIEDITOR, OnAnsiEditor)
	ON_COMMAND(ID_ANSI_OPEN, OnAnsiOpen)
	ON_COMMAND(ID_ANSI_SAVE, OnAnsiSave)
	ON_COMMAND(ID_ANSI_SAVEAS, OnAnsiSaveAs)
	ON_COMMAND(ID_ANSI_CLS, OnAnsiCls)
	ON_COMMAND(ID_ANSI_INS, OnAnsiIns)
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_IME_CHAR,OnImeChar)
	ON_MESSAGE(WM_DNSLOOKUP_END,OnDNSLookupEnd)
	ON_WM_MOUSEWHEEL()
	ON_COMMAND_RANGE(ID_EDIT_OPENURL, ID_EDIT_OPENURL_FTP, OnEditOpenURL)
	ON_COMMAND_RANGE(ID_FIRST_HISTORY,ID_LAST_HISTORY,OnHistory)	//�s�u����
    ON_REGISTERED_MESSAGE( WM_FINDREPLACE, OnFind )
END_MESSAGE_MAP()

CFont fnt;

/////////////////////////////////////////////////////////////////////////////
// CTermView construction/destruction


CPtrArray CTermView::all_telnet_conns;

CTermView::CTermView()
{
	CConn::view=this;

	blight=0;
	parent=NULL;
	bk=NULL;
	doflash=0;
	left_margin=0;
	top_margin=0;

	caret_vis=0;
	lineh=12;
	chw=0;

	bk = NULL;
	memset( &font_info, 0, sizeof(font_info) );
	draw_bmp=NULL;
	key_processed=0;

	telnet = NULL;

	auto_switch=0;
	blight = 0;

	holdobj = NULL;
	memdc = NULL;

	hand_cursor = NULL;
	paste_block = 0;

	pfinddlg=NULL;

#if defined _COMBO_
	con = NULL;
	autosort_favorite = 0;
#endif
}

CTermView::~CTermView()
{
#if defined	_COMBO_
//	CloseHandle(lock);
#endif
	if(memdc)
	{
		DeleteDC(memdc);
		SelectObject(memdc,holdobj);
	}
	if(bk)
		DeleteObject(bk);
	telnet=NULL;
}

LPSTR CTermView::ctviewcls="BBS_View";

BOOL CTermView::PreCreateWindow(CREATESTRUCT& cs)
{
	cs.style|=WS_VSCROLL;
	cs.dwExStyle|=WS_EX_CLIENTEDGE;
	RegWndClass(ctviewcls,AfxGetAfxWndProc(),NULL,NULL,AfxGetApp()->LoadStandardCursor(IDC_ARROW),CS_HREDRAW|CS_VREDRAW);
	cs.lpszClass=ctviewcls;
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CTermView drawing

void CTermView::OnInitialUpdate()
{
	SCROLLINFO info;
	GetScrollInfo(SB_VERT,&info);
	info.nMin=0;
	info.nPos=info.nMax=0;
	info.fMask=SIF_RANGE|SIF_POS|SIF_DISABLENOSCROLL;
	SetScrollInfo(SB_VERT,&info);

	SetTimer(ID_MAINTIMER,1000,NULL);

	if(!parent->LoadUI())
	{
		MessageBox( LoadString( IDS_IMPORTANT_FILE_LOST ),NULL,MB_ICONSTOP|MB_OK);
		return;
	}

	if(!AppConfig.auto_font)	//�p�G���ϥΰʺA�r��վ�
	{
		fnt.DeleteObject();
		fnt.CreateFontIndirect(&AppConfig.font_info);
		CWindowDC dc(this);
		CGdiObject* old=dc.SelectObject(&fnt);
		CSize& sz=dc.GetTextExtent(LoadString(IDS_DOUBLE_SPACE_CHAR),2);
		dc.SelectObject(&old);
		chw=sz.cx/2;
		lineh=sz.cy;
	}

	UpdateBkgnd();

	SetScrollBar();
}

/////////////////////////////////////////////////////////////////////////////
// CTermView message handlers

BOOL CTermView::OnEraseBkgnd(CDC* pDC) 
{
	return FALSE;
}

UINT CTermView::SetDCColors(CDC *dc, BYTE color,BOOL invirt)	//�Ǧ^ draw_opt: ETO_OPAQUE or 0
{
	BYTE fg=GetAttrFgColor(color),bk=GetAttrBkColor(color);

	if(bk>7)
		bk=0;

	COLORREF fgc=AppConfig.colormap[fg];
	COLORREF bkc=AppConfig.colormap[bk];
	if(invirt)
	{
		fgc=0x00ffffff & ~fgc;
		bkc=0x00ffffff & ~bkc;
	}

	dc->SetTextColor(fgc);
	if(bk==0 && AppConfig.bktype)
	{
		dc->SetBkMode(TRANSPARENT);
		return 0;
	}
	else
	{
		dc->SetBkMode(OPAQUE);
		dc->SetBkColor(bkc);
		return ETO_OPAQUE;
	}
}

void CTermView::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if(!telnet)
		return;

	if(key_processed)
		return;

	BYTE ch;
	ch=(BYTE)nChar;
	if(telnet->site_settings.localecho)
		telnet->LocalEcho(&ch,1);
	telnet->Send(&ch,1);
}


void CTermView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if(!telnet)
	{
		if(nChar == VK_RETURN && AppConfig.nocon_enter_reconnect )
			OnHistory(ID_FIRST_HISTORY);
		return;
	}

	const char* keystr=NULL;
	if(telnet->key_map)
		keystr=telnet->key_map->FindKey(nChar,nFlags);

	char dbcmd[32];
	LPSTR curstr=telnet->screen[telnet->cursor_pos.y];

	if( !telnet->is_ansi_editor )
	{
		if(!keystr)
		{
			key_processed=false;
			return;
		}
		key_processed=true;

		UINT l=strlen(keystr);
		switch(nChar)
		{
		case VK_LEFT:
			if(telnet->site_settings.auto_dbcs_arrow && telnet->cursor_pos.x>1 && IsBig5(curstr,telnet->cursor_pos.x-2))
			{
				strcpy(dbcmd,keystr);
				strcpy(dbcmd+l,keystr);
				telnet->Send(dbcmd,l*2);
			}
			else
				telnet->Send(keystr,l);
			break;
		case VK_RIGHT:
			if(telnet->site_settings.auto_dbcs_arrow && IsBig5(curstr,telnet->cursor_pos.x))
			{
				strcpy(dbcmd,keystr);
				strcpy(dbcmd+l,keystr);
				telnet->Send(dbcmd,l*2);
			}
			else
				telnet->Send(keystr,l);
			break;
		case VK_BACK:
			if(telnet->cursor_pos.x>1 && telnet->site_settings.auto_dbcs_backspace	&& IsBig5(curstr,telnet->cursor_pos.x-2))
			{
				strcpy(dbcmd,keystr);
				strcpy(dbcmd+l,keystr);
				telnet->Send(dbcmd,l*2);
				if(telnet->site_settings.localecho)
					telnet->Back(2);
			}
			else
			{
				if(telnet->site_settings.localecho)
					telnet->Back();
				telnet->Send(keystr,l);
			}
			break;
		case VK_DELETE:
			if(telnet->site_settings.auto_dbcs_del && IsBig5(curstr,telnet->cursor_pos.x))
			{
				strcpy(dbcmd,keystr);
				strcpy(dbcmd+l,keystr);
				telnet->Send(dbcmd,l*2);
				if(telnet->site_settings.localecho)
					telnet->Delete(2);
			}
			else
			{
				if(telnet->site_settings.localecho)
					telnet->Delete(1);
				telnet->Send(keystr,l);
			}
			break;
		case VK_RETURN:
			if( telnet->is_disconnected && AppConfig.enter_reconnect )
				ReConnect(telnet);
			else
			{
				if(telnet->site_settings.localecho)
					telnet->LocalEcho("\r\n",2);
				telnet->Send(keystr,l);
			}
			break;
		default:
			telnet->Send(keystr,l);
		}
	}
	else
	{
		key_processed=true;
		BOOL update=0;
		switch(nChar)
		{
		case VK_UP:
				telnet->GoUp(1);
			break;
		case VK_DOWN:
				telnet->GoDown(1);
			break;
		case VK_LEFT:
			if(telnet->site_settings.auto_dbcs_arrow && telnet->cursor_pos.x>1 && IsBig5(curstr,telnet->cursor_pos.x-2))
				telnet->GoLeft(2);
			else
				telnet->GoLeft(1);
			break;
		case VK_RIGHT:
			if(telnet->site_settings.auto_dbcs_arrow && IsBig5(curstr,telnet->cursor_pos.x))
				telnet->GoRight(2);
			else
				telnet->GoRight(1);
			break;
		case VK_TAB:
			telnet->Send("    ",4);
			update=TRUE;
			break;
		case VK_PRIOR:	//page up
			OnVScroll(SB_PAGEUP,0,NULL);
			break;
		case VK_NEXT:	//page down
			OnVScroll(SB_PAGEDOWN,0,NULL);
			break;
		case VK_HOME:
			telnet->Home();
			break;
		case VK_END:
			telnet->End();
			break;
		case VK_INSERT:
			OnAnsiIns();
			break;
		case VK_DELETE:
			if(telnet->site_settings.auto_dbcs_del && IsBig5(curstr,telnet->cursor_pos.x))
				telnet->Delete(2);
			else
				telnet->Delete();
			update=TRUE;
			break;
		case VK_BACK:
			if(telnet->cursor_pos.x>1 && telnet->site_settings.auto_dbcs_backspace	&& IsBig5(curstr,telnet->cursor_pos.x-2))
				telnet->Back(2);
			else
				telnet->Back();
			update=TRUE;
			break;
		default:
			key_processed=0;
		}

		if(update)
			telnet->UpdateLine(telnet->cursor_pos.y);	
	}
}


bool find_link(char* type,char* str,int& start,int& end)
{
	char *ppos=strstr(str,type);
	if(!ppos)
		return 0;
	int pos=ppos-str;
	start=pos;
	end=pos;
	while(str[end]>32 && str[end]<127 && str[end]!='>')
		end++;

	return 1;
}


void CTermView::OnLButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();
	if(!telnet)
		return;
/*
//���ե�---------
	CWindowDC dc(this);
	DWORD tc=GetTickCount();
	for(int i=0;i<100;i++)
	{
		OldDrawScreen(dc);
	}
	tc=GetTickCount()-tc;
	char result[10];
	ltoa(tc,result,10);
	MessageBox(result);
//---------------
*/

	SetCapture();
	int x,y;
	PtToLineCol(point,x,y);
	y+=telnet->scroll_pos;
	mouse_sel_timer = SetTimer( ID_MOUSE_SEL_TIMER, 100, NULL );
	telnet->sel_block=!!(nFlags & MK_SHIFT);

	if(telnet->sel_end.x!=telnet->sel_start.x || telnet->sel_end.y!=telnet->sel_start.y)
	{
		telnet->sel_end.x=telnet->sel_start.x;
		telnet->sel_end.y=telnet->sel_start.y;
		Invalidate(FALSE);
	}

	telnet->sel_start.x=x;
	telnet->sel_start.y=y;
	telnet->sel_end.x=x;
	telnet->sel_end.y=y;

	if( telnet->is_ansi_editor )
	{
		telnet->cursor_pos.x=x;
		telnet->cursor_pos.y=y;
		telnet->UpdateCursorPos();
	}
}

void CTermView::OnDestroy() 
{
	KillTimer(ID_MAINTIMER);
	DestroyCaret();
	parent->ansi_bar.DestroyWindow();
	CWnd::OnDestroy();
}

void CTermView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	if(!telnet)
		return;
	SCROLLINFO info;
	GetScrollInfo(SB_VERT,&info);
	int oldpos=info.nPos;
	info.fMask=SIF_POS;
	switch(nSBCode)
	{
	case SB_LINEUP:
		if(info.nPos==0)
			return;
		info.nPos--;
		break;
	case SB_LINEDOWN:
		if(info.nPos >= (info.nMax - info.nPage) )
			return;
		info.nPos++;
		break;
	case SB_PAGEUP:
		info.nPos-=telnet->site_settings.lines_per_page;
		if(info.nPos<0)
			info.nPos=0;
		break;
	case SB_PAGEDOWN:
		info.nPos += telnet->site_settings.lines_per_page;
		if(info.nPos > (info.nMax - info.nPage))
			info.nPos = info.nMax - info.nPage;
		break;
	case SB_THUMBTRACK:
		info.nPos=nPos;
		if(info.nPos > (info.nMax - info.nPage))
			info.nPos = info.nMax - info.nPage;
		break;
	}
	if(info.nPos==oldpos)
		return;

	SetScrollInfo(SB_VERT,&info);
	telnet->scroll_pos=info.nPos;
	if( telnet->is_ansi_editor )
		telnet->UpdateCursorPos();
	Invalidate(FALSE);
}

void CTermView::OnTimer(UINT nIDEvent) 
{
	if( nIDEvent == ID_MAINTIMER )
	{
		if(AppConfig.flash_window && doflash)
			parent->FlashWindow(TRUE);

		if( 0 == all_telnet_conns.GetSize() )
			return;

#ifdef	_COMBO_
		if( con && con->is_connected )
#else
		if( telnet && telnet->is_connected )
#endif
			parent->UpdateStatus();

		blight=!blight;
		if(telnet && telnet->sel_start==telnet->sel_end)
			DrawBlink();

		CTelnetConn** pitem=(CTelnetConn**)all_telnet_conns.GetData();
		CTelnetConn** plast_item = pitem + all_telnet_conns.GetSize();
		for(; pitem < plast_item ; pitem++ )
		{
			CTelnetConn* item = *pitem;
			if( item->is_connected )
			{
				item->time++;
				item->idle_time++;
				if(!(item->idle_time%item->site_settings.idle_interval) && item->site_settings.prevent_idle)
				{
					CString idlestr=UnescapeControlChars(item->site_settings.idle_str);
					item->Send((LPCTSTR)idlestr,idlestr.GetLength());
				}

				//Delay Send
				POSITION pos=item->delay_send.GetHeadPosition();
				while( pos )
				{
					CTelnetConnDelayedSend& ds=item->delay_send.GetAt(pos);
					if(ds.time > 1)
					{
						ds.time--;
						item->delay_send.GetNext(pos);
					}
					else
					{
						CString dsstr=ds.str;
						POSITION _pos=pos;
						item->delay_send.GetNext(pos);
						item->delay_send.RemoveAt(_pos);
						item->SendMacroString(dsstr);
					}
				}
			}
			else if( item->is_disconnected )
			{
				//�p�G�]�w�۰ʭ��s�A�ӥB�b�ɶ����Q�_�u�A�B���j�ɶ��w��
				if(item->site_settings.auto_reconnect)
				{
					if(item->time <= item->site_settings.connect_interval)
					{
						if(item->site_settings.reconnect_interval>0)
							item->site_settings.reconnect_interval--;
						else
							ReConnect(item);	//���s�s�u
					}
				}
			}
		}
	}
}


void CTermView::OnLButtonUp(UINT nFlags, CPoint point) 
{
	ReleaseCapture();
	::KillTimer(NULL,mouse_sel_timer);
	if(!telnet)
		return;
//	----------�p�G������ϡA�Ҽ{�O�_���۰ʽƻs------------
	if( telnet->sel_start.x!=telnet->sel_end.x || telnet->sel_start.y!=telnet->sel_end.y )
	{
		if(AppConfig.auto_copy)
			CopySelText();
	}
	else
	{
//	----------�p�G���O�b�����r�A�N�B�z�W�s��--------------
		int x,y;
		PtToLineCol(point,x,y,false);	//���⦨�׺ݾ��ù��y��
		int l;	char* url;
		if( (url=HyperLinkHitTest(x,y,l)) )	//�p�G�ƹ��I���W�s��
		{
			char tmp;	tmp=url[l];	url[l]=0;
			//	�I�s�{���}�ҶW�s��
			AppConfig.hyper_links.OpenURL(url);
			url[l]=tmp;
		}
//	-------------------------------------------------
	}
}

void CTermView::OnMouseMove(UINT nFlags, CPoint point) 
{
	if(!telnet)
		return;

	int lx,ly;
	PtToLineCol(point,lx,ly,false);

	BOOL bsel=(nFlags&MK_LBUTTON && ::GetCapture()==m_hWnd);		//�O�_���b���?
	point.x-=left_margin;	point.y-=top_margin;
	if(point.x<0)	point.x=0;

	int cx=0;	int cy=(point.y/lineh);
	if(cy>telnet->site_settings.lines_per_page-1)
	{
		cy=telnet->site_settings.lines_per_page-1;
		if(bsel)
			OnVScroll(SB_LINEDOWN,0,NULL);	//�󭶿���A����
	}
	if(point.y<0)
	{
		cy=0;
		if(bsel)
			OnVScroll(SB_LINEUP,0,NULL);	//�󭶿���A����
	}

	int y2=telnet->scroll_pos+cy;
	LPSTR curstr=telnet->screen[y2];
	LPSTR tmpstr=curstr;

	cx=(point.x/chw);
	if(telnet->site_settings.auto_dbcs_mouse)
	{
	//-----------�s���䴩���媺�y�Эp��-----------
		if(cx>0 && IsBig5(tmpstr,cx-1) )	//�p�G��ܤ����b�q�A�N����U�@�Ӧr
			cx++;
		else if(!IsBig5(tmpstr,cx) && (point.x%chw)*2>chw)	//�p�G�]���O����e�b�A�~�O�^��
			cx++;
	}
	else
	{
	//------------���Ҽ{���줸�ժ��y�Эp��-----------
		if( (point.x%chw)*2>chw )
			cx++;
	}

	if(cx>telnet->site_settings.cols_per_page)
		cx=telnet->site_settings.cols_per_page;

	if(point.x<chw)
		cx=0;

	if(bsel)
	{
		blight=1;
		CRect urc;
		GetClientRect(urc);
		urc.left=left_margin;
		int selendy=telnet->sel_end.y-telnet->scroll_pos;
		int selstarty=telnet->sel_start.y-telnet->scroll_pos;

		if(telnet->sel_block)
		{
			int selstartx,selendx;
			if(telnet->sel_end.x>telnet->sel_start.x)
				selstartx=telnet->sel_start.x,selendx=telnet->sel_end.x;
			else
				selstartx=telnet->sel_end.x,selendx=telnet->sel_start.x;

			if(selendy<selstarty)
			{
				int t=selendy;
				selendy=selstarty;
				selstarty=t;
			}
			urc.top=(selstarty<cy?selstarty:cy)*lineh+top_margin;
			urc.bottom=(selendy>cy?selendy:cy)*lineh+lineh+top_margin;
			urc.right=((selendx>cx?selendx:cx)+1)*chw+left_margin;
			urc.left=(selstartx<cx?selstartx:cx)*chw+left_margin;
		}
		else
		{
			if(cy > selendy)
			{
				urc.top=selendy*lineh+top_margin;
				urc.bottom=cy*lineh+top_margin;
				InvalidateRect(urc,FALSE);
				urc.right=chw*cx+left_margin;
				urc.bottom+=lineh+2;
			}
			else if(cy < selendy)
			{
				urc.top=cy*lineh+top_margin;
				urc.bottom=selendy*lineh+lineh+2+top_margin;
				InvalidateRect(urc,FALSE);
				urc.left+=cx*chw;
			}
			else
			{
				urc.top=cy*lineh+top_margin;
				urc.bottom=urc.top+lineh+2;
				urc.left=telnet->sel_end.x*chw+left_margin;
				urc.right=cx*chw+left_margin;
				if(urc.left>urc.right)
				{
					int tmp=urc.left;
					urc.left=urc.right;
					urc.right=tmp;
				}
			}
		}
		telnet->sel_end.x=cx;
		telnet->sel_end.y=y2;
		InvalidateRect(urc,FALSE);

		if( telnet->is_ansi_editor )
		{
			telnet->cursor_pos.x=cx;
			telnet->cursor_pos.y=y2;
			telnet->UpdateCursorPos();
		}
	}
	else
	{
		int len;
		if( HyperLinkHitTest(lx,ly,len))
			SetCursor(hand_cursor);
	}
}

void CTermView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
//	�W�s�����
/*	char text[32];
	MENUITEMINFO inf;
	inf.cbSize=sizeof(inf);
	inf.fMask=MIIM_TYPE|MIIM_SUBMENU|MIIM_ID;
	inf.dwTypeData=text;
	inf.cch=sizeof(text);
	GetMenuItemInfo(edit,4,TRUE,&inf);
	HMENU sub=inf.hSubMenu;	UINT wID=inf.wID;
*/

	char* link=NULL;
	int len;
	if(telnet)
	{
		int x,y;
		CPoint pt=point;
		ScreenToClient(&pt);
		PtToLineCol(pt,x,y,false);

		if( (link=HyperLinkHitTest(x,y,len)) )	//�p�G������O�W�s��
		{
			InsertMenu(parent->edit_menu,0,MF_SEPARATOR|MF_BYPOSITION,0,0);
			InsertMenu(parent->edit_menu,0,MF_STRING|MF_BYPOSITION,ID_EDIT_COPYURL, LoadString(IDS_COPY_URL));
		}
	}

	int c = GetMenuItemCount( parent->edit_menu );
	AppendMenu( parent->edit_menu, MF_SEPARATOR, 0, NULL );
	HMENU config=GetSubMenu(parent->main_menu, 2);
	char str[40];
	GetMenuString(config,ID_VIEW_FULLSCR,str,sizeof(str),MF_BYCOMMAND);
	AppendMenu(parent->edit_menu,MF_STRING,ID_VIEW_FULLSCR,str);

	UINT cmd=::TrackPopupMenu( parent->edit_menu,TPM_RETURNCMD|TPM_LEFTALIGN|TPM_RIGHTBUTTON,
							   point.x, point.y, 0, parent->m_hWnd, NULL);

	RemoveMenu(parent->edit_menu, c, MF_BYPOSITION);
	RemoveMenu(parent->edit_menu, c, MF_BYPOSITION);
	if(link)
	{
		RemoveMenu( parent->edit_menu, 0, MF_BYPOSITION );
		RemoveMenu( parent->edit_menu, 0, MF_BYPOSITION );
		if(cmd==ID_EDIT_COPYURL)
		{
			CClipboard::SetText(m_hWnd,link,len);
			return;
		}
	}
	if(cmd >0)
		AfxGetMainWnd()->SendMessage(WM_COMMAND,cmd,0);
}


void CTermView::OnPaint() 
{
	CPaintDC dc(this); // device context for painting

	if(AppConfig.smooth_draw)
	{
		drawdc.CreateCompatibleDC(&dc);
		GetClientRect(view_rect);
		draw_bmp=::CreateCompatibleBitmap(dc.m_hDC,view_rect.right,view_rect.bottom);
		HGDIOBJ h_old=SelectObject(drawdc.m_hDC,draw_bmp);
		int x=dc.m_ps.rcPaint.left , y=dc.m_ps.rcPaint.top;
		DrawScreen(drawdc);
		dc.BitBlt(x,y,dc.m_ps.rcPaint.right-x,dc.m_ps.rcPaint.bottom-y,&drawdc,x,y,SRCCOPY);
//		dc.BitBlt(x,y,view_rect.right,view_rect.bottom,&drawdc,x,y,SRCCOPY);
		SelectObject(drawdc.m_hDC,h_old);
		DeleteObject(draw_bmp);
		drawdc.DeleteDC();
		return;
	}
	DrawScreen(dc);
}

void CTermView::OnSize(UINT nType, int cx, int cy) 
{
	if(AppConfig.bktype>2)
		UpdateBkgnd();
	AdjustFont(cx,cy);
}

void CTermView::SetScrollBar()
{
	if(!telnet)
	{
		ShowScrollBar(SB_VERT,FALSE);
		EnableScrollBar(SB_VERT,ESB_DISABLE_BOTH);
		return;
	}

	SCROLLINFO info;
	GetScrollInfo(SB_VERT,&info);
	info.nMin=0;
	info.nMax=telnet->site_settings.line_count;
	info.nPos=telnet->scroll_pos;
	info.nPage = telnet->site_settings.lines_per_page;
	info.fMask=SIF_RANGE|SIF_POS|SIF_PAGE;
	SetScrollInfo(SB_VERT,&info);
	ShowScrollBar(SB_VERT,telnet->site_settings.showscroll);

	if(telnet->site_settings.line_count<=telnet->site_settings.lines_per_page)
		EnableScrollBar(SB_VERT,ESB_DISABLE_BOTH);
	else
		EnableScrollBar(SB_VERT,ESB_ENABLE_BOTH);
}

void CTermView::OnDisConnect()
{
#if defined	_COMBO_
	if(!con)
		return;

	if(!telnet)
	{
		((CWebConn*)con)->web_browser.wb_ctrl.Stop();
		return;
	}
#else
	if(!telnet)
		return;
#endif

	if(telnet->is_connecting || telnet->is_lookup_host )
	{
		parent->OnConnectClose();
		return;
	}

	if( !telnet->is_ansi_editor )
	{
		telnet->Shutdown();
		telnet->Close();
		telnet->ClearAllFlags();
		telnet->is_disconnected = true;
		telnet->site_settings.auto_reconnect=0;

		int idx=0;
		TCITEM tcitem;
		tcitem.mask=TCIF_IMAGE;
		tcitem.iImage=1;
		idx=parent->ConnToIndex(telnet);
		parent->tab.SetItem(idx,&tcitem);
	}
}

void CTermView::OnSetFocus(CWnd* pOldWnd) 
{
	CWnd::OnSetFocus(pOldWnd);
	CreateCaret();
	ShowCaret();
	if(telnet)
		telnet->UpdateCursorPos();
	else
		SetCaretPos(CPoint(left_margin,lineh+top_margin-2));
}

void CTermView::OnKillFocus(CWnd* pNewWnd) 
{
	DestroyCaret();
	caret_vis=FALSE;
	CWnd::OnKillFocus(pNewWnd);
}


BYTE CTermView::GetAttrBkColor(BYTE attr)
{
	BYTE bk=(attr&112)>>4;		//0111,0000b=112d;
	return bk;
}

BYTE CTermView::GetAttrFgColor(BYTE attr)
{
	BYTE fg=attr&7;		//0000,0111b=7;
	if(attr&8)
		fg+=8;		//0000,1000b=8d;
	return fg;
}

bool CTermView::IsAttrBlink( BYTE attr)
{
	return !!(attr&128);	//1000,0000b=128d
}

LRESULT CTermView::OnImeChar(WPARAM wparam, LPARAM lparam)
{
	if(!telnet)
		return 0;

	WORD db=(WORD)wparam;
	BYTE ch[]="  ";
	ch[0]=(BYTE) HIBYTE(db);
	ch[1]=(BYTE) LOBYTE(db);

	if(!ch[0])
	{
		if(telnet->site_settings.localecho)
			telnet->LocalEcho(ch+1,1);
		telnet->Send(ch+1,1);
	}
	else
	{
		if(telnet->site_settings.localecho)
			telnet->LocalEcho(ch,2);
		switch( telnet->site_settings.text_input_conv )	// Encoding Conversion
		{
		case BIG52GB:
			chi_conv.Big52GB( (const char*)ch, (char*)ch, 2 );
			break;
		case GB2BIG5:
			chi_conv.GB2Big5( (const char*)ch, (char*)ch, 2 );
		}
		telnet->Send(ch,2);
	}
	return 0;
}


BOOL CTermView::Connect(CString address, CString name, short port, LPCTSTR cfg_filepath)
{
	if(name.IsEmpty())
		return FALSE;
	#if defined	_COMBO_
		if( port > 0 && address.Find("telnet://")==-1)
			address = "telnet://"+address;
	#endif

	CConn* ncon=NewConn(address, name, port, cfg_filepath);	//���ͤF�s���s�u�e���A�����Ҧ��]�w
	if(!ncon)
		return FALSE;

	parent->SwitchToConn( ncon );
	if( ( ncon->is_ansi_editor ) )
		return TRUE;

//�}�l�s��socket
	ConnectSocket((CTelnetConn*)ncon);
	return TRUE;
}

LRESULT CTermView::OnDNSLookupEnd(WPARAM found, LPARAM lparam)
{
	DNSLookupData* data=(DNSLookupData*)lparam;
	CTelnetConn* new_telnet=data->new_telnet;
	SOCKADDR_IN& sockaddr=data->sockaddr;
//�]�w�n������T
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons((u_short)new_telnet->port);
//	sockaddr.sin_addr.s_addr = ((LPIN_ADDR)lphost->h_addr)->s_addr;

	if(found)
	{
		if( new_telnet->is_cancelled )
			delete new_telnet;
		else
		{
			new_telnet->ClearAllFlags();
			new_telnet->is_connecting = true;
			new_telnet->Connect((SOCKADDR*)&sockaddr, sizeof(SOCKADDR_IN));
		}
	}
	else
	{
		if( new_telnet->is_cancelled )
			delete new_telnet;
		else
			new_telnet->OnConnect(WSAEADDRNOTAVAIL);	//Connection failed
	}
	WaitForSingleObject(data->hTask,INFINITE);
	CloseHandle(data->hTask);

	delete data;
	return 0;
}

void CTermView::OnEditPastefile() 
{
	if(!telnet)
		return;
	if( !telnet->is_connected && !telnet->is_ansi_editor )
		return;

	CFileDialog dlg(TRUE,NULL,NULL,OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_ENABLESIZING, LoadString(IDS_TXT_FILTER));
	if(dlg.DoModal()==IDOK)
	{
		CFile file;
		if(file.Open(dlg.GetPathName(),CFile::modeRead))
		{
			int len=file.GetLength();
			char *data=new char[len+1];
			file.Read(data,len);
			data[len]=0;
			file.Abort();
			telnet->SendString(data);
			delete data;
		}
	}
}


void CTermView::ReConnect(CTelnetConn *retelnet)
{
	if( retelnet->is_disconnected )
	{
		int idx=parent->ConnToIndex(retelnet);
		TCITEM item;
		item.mask=TCIF_IMAGE;
		item.iImage=7;
		parent->tab.SetItem(idx,&item);
		retelnet->time=0;
		retelnet->ClearAllFlags();
		retelnet->is_lookup_host = true;
		retelnet->Close();
		retelnet->ClearScreen(2);
		retelnet->site_settings.Load( retelnet->cfg_filepath );
		//���έ��s���J��L����

		ConnectSocket(retelnet);
	}
	else
	{
		Connect(retelnet->address, retelnet->name, retelnet->port, retelnet->cfg_filepath);
	}
}

void CTermView::OnReconnect() 
{
	if(telnet)
		ReConnect(telnet);
#if defined	_COMBO_
	else if(con)
		((CWebConn*)con)->web_browser.wb_ctrl.Refresh();
#endif
}

void CTermView::OnHistory(UINT id)
{
	if(AppConfig.favorites.history.GetSize()>0)
	{
		CString str = AppConfig.favorites.history[id-ID_FIRST_HISTORY];
		int p = str.Find('\t');
		p = str.Find('\t', p+1);
		if( -1 == p )
			ConnectStr(str, "");
		else
			ConnectStr( str.Left(p), str.Mid(p+1) );
	}
}

void CTermView::OnAnsiCopy()
{
	if(!telnet)
		return;

	CString data=GetSelAnsi();
	if(CClipboard::SetText(m_hWnd,data))
		paste_block=telnet->sel_block;

	if(AppConfig.auto_cancelsel)
	{
		telnet->sel_end=telnet->sel_start;
		Invalidate(FALSE);
	}
}

CString CTermView::AttrToStr(BYTE prevatb,BYTE attr)
{
	CString ret="\x1b[";
	if(attr==7)
	{
		ret+='m';
		return ret;
	}

	BYTE fg=attr&7;
	BYTE bk=GetAttrBkColor(attr);
	BYTE blink=attr&128;
	BYTE hilight=attr&8;

	BYTE hilight_changed=0;
	BYTE blink_changed=0;
	BYTE fg_changed=0;
	BYTE bk_changed=0;

	if(fg!= (prevatb&7))	//�p�G�e�������
		fg_changed=1;
	if(bk!=GetAttrBkColor(prevatb))	//�p�G�I�������
		bk_changed=1;

	if( hilight != (prevatb&8) )	//�p�G���G�ק���
	{
		hilight_changed=1;
		if(!hilight)	//�p�G�ܦ����G,�n���]�Ҧ��ݩ�
		{
			blink_changed=fg_changed=bk_changed=1;
			ret+=';';
		}
	}

	if( blink != (prevatb&128))	//�p�G�{�{����
	{
		blink_changed=1;
		if(!blink)	//�p�G�ܦ����{�{,�n���]�Ҧ��ݩ�
		{
			if( !(hilight_changed && !hilight) )	//�p�G�Ҧ��ݩ��٨S���]�L�~���]
			{
				ret+=';';
				hilight_changed=fg_changed=bk_changed=1;
			}
		}
	}

	if(hilight_changed && hilight)
		ret+="1;";
	if(blink_changed && blink)
		ret+="5;";
	char num[4]={0,0,';',0};
	if(fg_changed)
	{
		*num='3';
		*(num+1)='0'+fg;
		ret+=num;
	}
	if(bk_changed)
	{
		*num='4';
		*(num+1)='0'+bk;
		ret+=num;
	}

	char* pret=LPSTR(LPCTSTR(ret));
	if(pret[ret.GetLength()-1]==';')
		pret[ret.GetLength()-1]='m';
	else
		ret+='m';
	return ret;
}


void CTermView::OnAnsiEditor()
{
	Connect( LoadString(IDS_NOT_SAVED), LoadString(IDS_ANSI_EDIT), 0 );
}


int CTermView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	hand_cursor = AfxGetApp()->LoadCursor(IDC_HAND_CUR);

	chi_conv.SetTablePath(AppPath+"Conv\\");
	return 0;
}


void CTermView::OnAnsiOpen()
{
	CFileDialog dlg(TRUE,NULL,NULL,OFN_HIDEREADONLY|OFN_ALLOWMULTISELECT|OFN_OVERWRITEPROMPT|OFN_ENABLESIZING, LoadString(IDS_TXT_AND_ANS_FILTER),parent);
	if(dlg.DoModal()==IDOK)
	{
		POSITION pos=dlg.GetStartPosition();
		while(pos)
			OpenAnsFile(dlg.GetNextPathName(pos));
	}
}

void CTermView::OnAnsiSaveAs()
{
	if(!telnet)
		return;
	if( !telnet->is_ansi_editor )
		return;

	CFileDialog dlg(FALSE,"ans",NULL,OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_ENABLESIZING, LoadString(IDS_ANS_FILTER));
	if(dlg.DoModal()==IDOK)
	{
		CFile file;
		if(file.Open(dlg.GetPathName(),CFile::modeCreate|CFile::modeWrite))
		{
			OnSelAllBuf();
			CString str=GetSelAnsi();
			telnet->sel_end=telnet->sel_start;
			file.Write(LPCTSTR(str),str.GetLength());
			file.Abort();
			telnet->address=dlg.GetPathName();
			telnet->name = dlg.GetFileName();

			parent->tab.SetItemText(parent->tab.GetCurSel(), telnet->name);
			parent->UpdateStatus();
			parent->UpdateAddressBar();
		}
	}
}

CString CTermView::GetSelAnsi()
{
	CString data;
	if(telnet->sel_end.x!=telnet->sel_start.x || telnet->sel_end.y!=telnet->sel_start.y)
	{
		UINT tmp;
		if(telnet->sel_end.x<telnet->sel_start.x)
		{
			tmp=telnet->sel_end.x;
			telnet->sel_end.x=telnet->sel_start.x;
			telnet->sel_start.x=tmp;
		}

		if(telnet->sel_end.y<telnet->sel_start.y)
		{
			tmp=telnet->sel_end.y;
			telnet->sel_end.y=telnet->sel_start.y;
			telnet->sel_start.y=tmp;
		}

		long selstarty=telnet->sel_start.y;
		long selendy=telnet->sel_end.y;

		data="\x1b[m";
		LPSTR str=telnet->screen[selstarty]+telnet->sel_start.x;
		LPSTR strend;
		LPBYTE attr=telnet->GetLineAttr(selstarty)+telnet->sel_start.x;
		BYTE tmpatb=*attr;
		if(tmpatb!=7)
		{
			data+=AttrToStr(7,tmpatb);
		}
		if(selstarty==selendy)	//if a single line is selected
		{
			strend=telnet->screen[selendy]+telnet->sel_end.x-1;
			BYTE *atbend=telnet->GetLineAttr(selendy)+telnet->sel_end.x-1;
			while(*strend==' ' && GetAttrBkColor(*atbend)==0)
				strend--,atbend--;

			while(str<=strend)
			{
				if(*attr!=tmpatb)
				{
					data+=AttrToStr(tmpatb,*attr);
					tmpatb=*attr;
				}
				if(*str)
					data+=*str;
				str++;
				attr++;
			}
			if(tmpatb!=7)
			data+="\x1b[m";
		}
		else	//select several line
		{
			int x,x2;
			if(telnet->sel_block)	//�ϥΰ϶����
			{
				x=telnet->sel_start.x;
				x2=telnet->sel_end.x;
				if(telnet->sel_end.x >= telnet->site_settings.cols_per_page)
					x2--;
			}
			else
			{
				x=0;
				x2=telnet->site_settings.cols_per_page-1;
			}

			strend=telnet->screen[selstarty]+x2;
			LPBYTE atbend=telnet->GetLineAttr(selstarty)+x2;
			while(*strend==' ' && GetAttrBkColor(*atbend)==0)
				strend--,atbend--;

			while(str<=strend)
			{
				if(*attr!=tmpatb)
				{
					data+=AttrToStr(tmpatb,*attr);
					tmpatb=*attr;
				}
				if(*str)
					data+=*str;
				str++;
				attr++;
			}
			if(tmpatb!=7)
				data+="\x1b[m";
			data+="\x0d\x0a";

			for(int i=selstarty+1;i<selendy;i++)
			{
				str=telnet->screen[i]+x;
				attr=telnet->GetLineAttr(i)+x;
				strend=str+x2;
				atbend=attr+x2;
				while(*strend==' ' && GetAttrBkColor(*atbend)==0)
					strend--,atbend--;

				tmpatb=7;
				while(str<=strend)
				{
					if(*attr!=tmpatb)
					{
						data+=AttrToStr(tmpatb,*attr);
						tmpatb=*attr;
					}
					if(*str)
						data += *str;
					str++;
					attr++;
				}
				if(tmpatb!=7)
					data+="\x1b[m";
				data+="\x0d\x0a";
			}
			str=telnet->screen[telnet->sel_end.y];
			attr=telnet->GetLineAttr(telnet->sel_end.y);
			strend=str+telnet->sel_end.x-1;
			atbend=attr+telnet->sel_end.x-1;
			str+=x;
			attr+=x;
			while(*strend==' ' && GetAttrBkColor(*atbend)==0)
				strend--,atbend--;

			tmpatb=7;
			while(str<=strend)
			{
				if(*attr!=tmpatb)
				{
					data+=AttrToStr(tmpatb,*attr);
					tmpatb=*attr;
				}
				if(*str)
					data+=*str;
				str++;
				attr++;
			}
			if(tmpatb!=7)
				data+="\x1b[m";
		}
	}
	return data;
}

void CTermView::OnAnsiSave()
{
	if(telnet && !telnet->address.Compare( LoadString(IDS_NOT_SAVED) ))
	{
		OnAnsiSaveAs();
	}
	else
	{
		CFile file;
		if(file.Open(telnet->address,CFile::modeCreate|CFile::modeWrite))
		{
			OnSelAllBuf();
			while(telnet->sel_end.y > telnet->sel_start.y && telnet->IsEmptyLine(telnet->screen[telnet->sel_end.y],telnet->site_settings.cols_per_page))
				telnet->sel_end.y--;
			CString str=GetSelAnsi();
			telnet->sel_end=telnet->sel_start;
			file.Write(LPCTSTR(str),str.GetLength());
			file.Abort();
		}
	}
}

void CTermView::OnAnsiCls()
{
	if(telnet && telnet->is_ansi_editor)
	{
		telnet->cursor_pos.x=0;
		telnet->cursor_pos.y=0;
		telnet->UpdateCursorPos();
		telnet->ReSizeBuffer(48,telnet->site_settings.cols_per_page,telnet->site_settings.lines_per_page);
		for(int i=0;i<telnet->last_line;i++)
			telnet->InitNewLine(telnet->screen[i]);
		Invalidate(FALSE);
	}
}

void CTermView::OnAnsiIns()
{
	if(telnet && telnet->is_ansi_editor)
	{
		telnet->insert_mode=!telnet->insert_mode;
		parent->UpdateStatus();
	}
}

void CTermView::OnSetBkgnd() 
{
	CSetBkDlg dlg(this);
	if(dlg.DoModal()==IDOK)
	{
		if(AppConfig.bktype==1)
		{
			parent->CloseWindow();
			Sleep(500);
			UpdateBkgnd();
			parent->ShowWindow(SW_RESTORE);
			parent->ShowWindow(SW_SHOW);
		}
		else if(AppConfig.bktype==0)
		{
			DeleteObject(bk);
			bk=NULL;
			Invalidate(FALSE);
		}
		else
		{
			UpdateBkgnd();
			Invalidate(FALSE);
		}
	}
}

void CTermView::UpdateBkgnd()
{
	if(memdc)
	{
		SelectObject(memdc,holdobj);
		DeleteDC(memdc),memdc=NULL;
	}
	if(bk)
		DeleteObject(bk),bk=NULL;
	if(AppConfig.bktype==0)
		return;

	HDC hdc=NULL;
	BITMAP bmp;

	hdc=::GetDC(m_hWnd);
	memdc=::CreateCompatibleDC(hdc);
	::ReleaseDC(m_hWnd,hdc);
	if(AppConfig.bktype>1)
	{
		bk=(HBITMAP)LoadImage(AfxGetInstanceHandle(),AppConfig.bkpath,IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_DEFAULTCOLOR);
		if(!bk)
		{
			MessageBox( LoadString(IDS_LOAD_PIC_FAILED ), LoadString(IDS_ERR),MB_OK|MB_ICONSTOP);
			bk=NULL;
			AppConfig.bktype=0;
			AppConfig.bkpath.Empty();
			return;
		}
	}
	else
	{
		hdc=::GetDC(::GetDesktopWindow());
		bk=CreateCompatibleBitmap(hdc,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN));
		holdobj=SelectObject(memdc,bk);
		BitBlt(memdc,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),hdc,0,0,SRCCOPY);
		::ReleaseDC(::GetDesktopWindow(),hdc);
		SelectObject(memdc,holdobj);
	}
	GetObject(bk,sizeof(bmp),&bmp);
	DWORD len=bmp.bmWidth*bmp.bmHeight*4;
	BYTE *data = new BYTE[len];
	BYTE* pdata=data;
	BYTE *lastdata= data+len;

	BITMAPINFO info;
	info.bmiColors;
	info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth=bmp.bmWidth;
	info.bmiHeader.biHeight=bmp.bmHeight;
	info.bmiHeader.biPlanes=1;
	info.bmiHeader.biBitCount=32;
	info.bmiHeader.biCompression=BI_RGB;
	info.bmiHeader.biSizeImage=0;
	info.bmiHeader.biClrUsed=0;
	info.bmiHeader.biClrImportant=0;
	info.bmiHeader.biXPelsPerMeter=0;
	info.bmiHeader.biYPelsPerMeter=0;

	HDC dc=::GetDC(m_hWnd);
	if(::GetDIBits(dc,bk,0,bmp.bmHeight,data,&info,DIB_RGB_COLORS))
	{
		while(data<lastdata)
		{
			*data=*data*AppConfig.bkratio/10;
			data++;
		}
	
		int r=::SetDIBits(dc,bk,0,bmp.bmHeight,pdata,&info,DIB_RGB_COLORS);
		delete []pdata;
	}
	::ReleaseDC(m_hWnd,dc);

	if(AppConfig.bktype==2)
	{
		CRect rc;
		rc.left=0;	rc.right=GetSystemMetrics(SM_CXSCREEN);
		rc.top=0;	rc.bottom=GetSystemMetrics(SM_CYSCREEN);
		HDC dc=::GetDC(m_hWnd);
		HDC memdc2=::CreateCompatibleDC(dc);
		HBITMAP tmp=bk;
		bk=CreateCompatibleBitmap(dc,rc.right,rc.bottom);
		::ReleaseDC(m_hWnd,dc);
		holdobj=SelectObject(memdc,bk);
		HGDIOBJ old2=SelectObject(memdc2,tmp);
		while(rc.left < rc.right)
		{
			while(rc.top < rc.bottom)
			{
				::BitBlt(memdc,rc.left,rc.top,bmp.bmWidth,bmp.bmHeight,memdc2,0,0,SRCCOPY);		
				rc.top+=bmp.bmHeight;
			}
			rc.top=0;
			rc.left+=bmp.bmWidth;
		}
		SelectObject(memdc,holdobj);
		SelectObject(memdc2,old2);
		DeleteDC(memdc2);
		DeleteObject(tmp);
	}
	else if(AppConfig.bktype==3)
	{
		CRect rc;
		HDC dc=::GetDC(m_hWnd);
		HBITMAP tmp=bk;
		GetClientRect(rc);
		bk=CreateCompatibleBitmap(dc,rc.right,rc.bottom);
		::ReleaseDC(m_hWnd,dc);
		HDC memdc2=::CreateCompatibleDC(memdc);
		holdobj=SelectObject(memdc,bk);
		HGDIOBJ old2=SelectObject(memdc2,tmp);

		::FillRect(memdc,rc,(HBRUSH)GetStockObject(BLACK_BRUSH));
		int left=(rc.Width()-bmp.bmWidth)/2;
		int top=(rc.Height()-bmp.bmHeight)/2;
		::BitBlt(memdc,left,top,bmp.bmWidth,bmp.bmHeight,memdc2,0,0,SRCCOPY);

		SelectObject(memdc,holdobj);
		DeleteDC(memdc2);
		SelectObject(memdc2,old2);
		DeleteObject(tmp);
	}
	else if(AppConfig.bktype==4)
	{
		CRect rc;
		HDC dc=::GetDC(m_hWnd);
		HBITMAP tmp=bk;
		GetClientRect(rc);
		bk=CreateCompatibleBitmap(dc,rc.right,rc.bottom);
		::ReleaseDC(m_hWnd,dc);
		HDC memdc2=::CreateCompatibleDC(memdc);
		holdobj=SelectObject(memdc,bk);
		HGDIOBJ old2=SelectObject(memdc2,tmp);

		::StretchBlt(memdc,0,0,rc.Width(),rc.Height(),memdc2,0,0,bmp.bmWidth,bmp.bmHeight,SRCCOPY);

		SelectObject(memdc,holdobj);
		SelectObject(memdc2,old2);
		DeleteDC(memdc2);
		DeleteObject(tmp);
	}
	holdobj=SelectObject(memdc,bk);
}


inline void CTermView::FillBk(CDC &dc)
{
	GetClientRect(view_rect);
	if(!AppConfig.bktype)
	{
		dc.FillSolidRect(view_rect,AppConfig.colormap[0]);
		return;
	}
	BITMAP bmp;
	GetObject(bk,sizeof(BITMAP),&bmp);
	if(AppConfig.bktype==1)
	{
		ClientToScreen(view_rect);
		::BitBlt(dc.m_hDC,0,0,view_rect.Width(),view_rect.Height(),memdc,view_rect.left,view_rect.top,SRCCOPY);
	}
	else
		::BitBlt(dc.m_hDC,0,0,bmp.bmWidth,bmp.bmHeight,memdc,0,0,SRCCOPY);		
}

void CTermView::OnRightNoDBCS() 
{
	if(telnet)
		telnet->Send("\x1b[C",3);
}

void CTermView::OnLeftNoDBCS() 
{
	if(telnet)
		telnet->Send("\x1b[D",3);
}

void CTermView::OnBackspaceNoDBCS() 
{
	if(telnet)
	{
		if(telnet->is_ansi_editor)
			telnet->Back();
		else
		{
			const char* back=NULL;
			if(telnet->key_map && (back=telnet->key_map->FindKey(VK_BACK,0)) )
				telnet->Send(back,strlen(back));
		}
	}
}

CConn* CTermView::NewConn(CString address, CString name, short port, LPCTSTR cfg_filepath)
{
	CConn* newcon=NULL;
	newcon=new CTelnetConn;
	newcon->address=address;
	newcon->name=name;

#if defined	_COMBO_
	if( !newcon->is_web )	//�p�G�O�s�uBBS,�s�}BBS�e��
	{
#endif
		CTelnetConn* new_telnet=(CTelnetConn*)newcon;
		new_telnet->port=port;
		new_telnet->cfg_filepath=cfg_filepath;

		//���s��socket���J�]�w��
		if(!new_telnet->site_settings.Load( new_telnet->cfg_filepath ))	//�p�G���J�]�w�o�Ϳ��~
		{
			//�o�ܦ��i��|�b�K�X��J���~���ɭԵo��!
			delete new_telnet;
			return NULL;
		}
		new_telnet->key_map=CKeyMap::Load(new_telnet->site_settings.KeyMapName);

		if( new_telnet->site_settings.text_input_conv || new_telnet->site_settings.text_output_conv )
			chi_conv.AddRef();

		if( 0 == port ) // port = 0 �N�� ansi editor ( dirty hack :( )
		{
			new_telnet->ClearAllFlags();
			new_telnet->is_ansi_editor = true;
			new_telnet->site_settings.line_count=AppConfig.ed_lines_per_page*2;
			new_telnet->site_settings.cols_per_page=AppConfig.ed_cols_per_page;
			new_telnet->site_settings.lines_per_page=AppConfig.ed_lines_per_page;
			new_telnet->site_settings.showscroll=TRUE;
		}
		new_telnet->CreateBuffer();

#if defined	_COMBO_
	}
#endif
	parent->NewTab( newcon );
	all_telnet_conns.Add( newcon );
	return newcon;
}

DWORD CTermView::DNSLookupThread(LPVOID param)
{
	DNSLookupData* host=(DNSLookupData*)param;
	CTelnetConn* new_telnet=host->new_telnet;
	LPHOSTENT lphost = gethostbyname(host->address);
	if(lphost == NULL)
	{
		WSASetLastError(WSAEINVAL);
		new_telnet->view->PostMessage(WM_DNSLOOKUP_END,FALSE,(LPARAM)param);
		ExitThread(1);
	}
	else
	{
		host->sockaddr.sin_addr.s_addr = ((LPIN_ADDR)lphost->h_addr)->s_addr;
	}
	new_telnet->view->PostMessage(WM_DNSLOOKUP_END,TRUE,(LPARAM)param);
	ExitThread(0);
	return 0;
}

LRESULT CTermView::OnSocket(WPARAM wparam, LPARAM lparam)
{
	if( !all_telnet_conns.GetData() )
		return 0;

	CTelnetConn** pptelnet=(CTelnetConn**)all_telnet_conns.GetData();
	CTelnetConn** plasttelnet = pptelnet + all_telnet_conns.GetSize();
	for( ; pptelnet < plasttelnet; pptelnet++ )
	{
		CTelnetConn* ptelnet = *pptelnet;
		if( ptelnet->is_telnet && ptelnet->telnet==(SOCKET)wparam )
		{
			WORD err=HIWORD(lparam);

			switch(LOWORD(lparam))
			{
			case FD_READ:
				ptelnet->OnReceive(err);
				break;
			case FD_CONNECT:
				ptelnet->OnConnect(err);
				break;
			case FD_CLOSE:
				ptelnet->OnClose(err);
			}
		}
	} //end for
	return 0;
}

void CTermView::ConnectSocket(CTelnetConn *new_telnet)
{
	if(InternetAttemptConnect(0)!=ERROR_SUCCESS)
	{
		new_telnet->OnConnect(WSAENOTCONN);
		return;
	}

	new_telnet->Create();

#if defined	_COMBO_
	const LPCTSTR paddress=LPCTSTR(new_telnet->address)+9;
#else
	CString &paddress=new_telnet->address;
#endif
	SOCKADDR_IN sockaddr;
	memset(&sockaddr,0,sizeof(SOCKADDR_IN));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons((u_short) new_telnet->port );
	sockaddr.sin_addr.s_addr = inet_addr(paddress);
	if(sockaddr.sin_addr.s_addr != INADDR_NONE)
	{
		new_telnet->ClearAllFlags();
		new_telnet->is_connecting = true;
		new_telnet->Connect((SOCKADDR*)&sockaddr,sizeof(SOCKADDR_IN));
		return;
	}
//	�p�G���OIP�A�����M��D��
//	�[�J�s�����
	DNSLookupData *newfind = new DNSLookupData;
	newfind->new_telnet = new_telnet;
	newfind->address = paddress;
	DWORD tid;
//	�}�l�s�������
	memset( &sockaddr, 0, sizeof(SOCKADDR_IN) );
	newfind->hTask = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)DNSLookupThread,
								   newfind, 0, &tid);
}


void CTermView::OnSelAllBuf() 
{
	if(!telnet)
		return;
	telnet->sel_start.x=0;
	telnet->sel_start.y=0;
	telnet->sel_end.x=telnet->site_settings.cols_per_page;
	telnet->sel_end.y=telnet->site_settings.line_count-1;
	Invalidate(FALSE);
}

BOOL CTermView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) 
{
	if(!telnet)
		return TRUE;
	SCROLLINFO info;
	GetScrollInfo(SB_VERT,&info);
	if( (zDelta>0 && info.nPos>0) || (zDelta<0 && info.nPos<info.nMax) )
	{
		int oldpos = info.nPos;

		info.fMask=SIF_POS;
		info.nPos-=zDelta/WHEEL_DELTA;
		int pos=info.nPos;
		if(info.nPos<0)
			info.nPos=0;
		else if(info.nPos > (info.nMax - info.nPage))
			info.nPos = info.nMax - info.nPage;

		if( info.nPos != oldpos )
		{
			SetScrollInfo(SB_VERT,&info);
			telnet->scroll_pos = info.nPos;
			if( telnet->is_ansi_editor )
				telnet->UpdateCursorPos();

	//		telnet->CheckHyperLinks();
			Invalidate(FALSE);
		}
	}
	return CWnd::OnMouseWheel(nFlags, zDelta, pt);
}

inline void CTermView::DrawScreen(CDC &dc)
{
	if(!telnet) { FillBk(dc); return; }

	GetClientRect(view_rect);
	if(AppConfig.bktype==1)
		ClientToScreen(view_rect);

	int y=top_margin;
	HANDLE fold=SelectObject(dc.m_hDC,fnt.m_hObject);

	//�p�����ϡA�N�˸m��������٭�
	long selstarty=telnet->sel_start.y;		long selendy=telnet->sel_end.y;
	long selstartx=telnet->sel_start.x;		long selendx=telnet->sel_end.x;
	if(telnet->sel_block)
	{
		if(selstarty > selendy)
			swap(selstarty,selendy);
		if(selstartx > selendx)
			swap(selstartx,selendx);
	}
	else
	{
		if(selstarty>selendy)
		{
			swap(selstarty,selendy);
			swap(selstartx,selendx);
		}
		else if(selstarty==selendy && selstartx > selendx)
			swap(selstartx,selendx);			
	}

	BYTE* pline_selstart;	//�]���̤j
	BYTE* pline_selend;	//�]���̤p
	int last_line=telnet->scroll_pos + telnet->site_settings.lines_per_page;
	for( int i=telnet->scroll_pos ; i < last_line; i++ )
	{
		LPBYTE atbline=telnet->GetLineAttr(i);	//attribs
		if( i >= selstarty && i <= selendy)	//�p�G�b����Ϥ�
		{
			pline_selstart=(BYTE*)0xffffffff;	//�]���̤j
			pline_selend	=(BYTE*)0x00000000;	//�]���̤p
			if(telnet->sel_block)	//�϶����
			{
				pline_selstart=atbline+selstartx;
				pline_selend=atbline+selendx;
			}
			else
			{
				pline_selstart=(i==selstarty)? (atbline+selstartx) : atbline;
				pline_selend=(i==selendy)? (atbline+selendx) : atbline+telnet->site_settings.cols_per_page;
			}
		}
		else
			pline_selstart=pline_selend=NULL;

		if(AppConfig.old_textout)
			DrawLineOld(dc,telnet->screen[i],pline_selstart,pline_selend,y);
		else
			DrawLine(dc,telnet->screen[i],pline_selstart,pline_selend,y);

		if(AppConfig.link_underline)
			DrawLink(dc,telnet->screen[i],telnet->GetLineAttr(i),y);
		y+=lineh;
	}
	SelectObject(dc.m_hDC,fold);

//-----------�񺡵e���P��-------------
	int right_margin=left_margin+telnet->site_settings.cols_per_page*chw;
	CRect rc;	GetClientRect(rc);
	int t=rc.bottom;	rc.bottom=top_margin;	FillBkRect(dc,rc,0);	//�W
	rc.bottom=t;	rc.top=y;	FillBkRect(dc,rc,0);	//�U
	t=rc.right;		rc.right=left_margin;	rc.top=top_margin;	rc.bottom=y;	FillBkRect(dc,rc,0);	//��
	rc.right=t;		rc.left=right_margin;	FillBkRect(dc,rc,0);	//�k
}


BOOL IsDBCS(char* head,char* str)
{
	int w;
	while(head<=str)
	{
		w=get_chw(head);
		head+=w;
	}
	return (head>=str && w==2);
}


void CTermView::DrawLine(CDC &dc, LPCSTR line, BYTE* pline_selstart, BYTE* pline_selend, int y)
{
	LPCSTR eol=line+telnet->site_settings.cols_per_page;
	CRect rc(left_margin,y,0,y+lineh);
	UINT draw_opt=SetDCColors(&dc,7);
	BYTE prevatb=7;		bool prevbsel=false,textout=true;

	while(line< eol)
	{
		int w=get_chw(line);
		LPBYTE patb=telnet->GetLineAttr(line);
		bool bsel=(patb>=pline_selstart && patb< pline_selend);

		int dx=chw*w;
		rc.right=rc.left+dx;
		if(w==2 )	//�p�G�O���줸�զr�A�ˬd�O�_������r
		{
			bool bsel2 = ( (patb+1)>=pline_selstart && (patb+1)< pline_selend);
			//�p�G�O����r�A�άO����e��b������A���P
			if( patb[0] != patb[1] || bsel != bsel2)	//����X��Ӥ�r�A�A��X�e�b
			{
				if(patb[1] != prevatb || bsel2 != prevbsel )
				{
					draw_opt=SetDCColors(&dc,(prevatb=patb[1]), (prevbsel=bsel2) );
					textout=(!IsAttrBlink( patb[1]) || blight || telnet->sel_end!=telnet->sel_start);
				}
				if( !(draw_opt & ETO_OPAQUE) )
					FillBkRect(dc,rc,prevatb,prevbsel);
				ExtTextOut(dc, rc.left,rc.top,draw_opt,rc,line,(textout?2:0));
				rc.right-=chw;
				draw_opt=ETO_CLIPPED;
			}
		}
		if(patb[0] != prevatb || bsel != prevbsel )
		{
			draw_opt&=ETO_CLIPPED;
			draw_opt|=SetDCColors(&dc, (prevatb=patb[0]) ,(prevbsel=bsel) );
			textout=(!IsAttrBlink( patb[0]) || blight || telnet->sel_end!=telnet->sel_start);
		}

		if( !(draw_opt & ETO_OPAQUE) )
			FillBkRect(dc,rc,prevatb,prevbsel);

		ExtTextOut(dc, rc.left,rc.top,draw_opt,rc,line,(textout?w:0));

		rc.left += dx;
		line += w;
	}
}


void CTermView::DrawLineBlink(CDC &dc, LPCSTR line, int y)
{
	LPCSTR eol=line+telnet->site_settings.cols_per_page;
	CRect rc(left_margin,y,0,y+lineh);
	UINT draw_opt=SetDCColors(&dc,7);
	BYTE prevatb=7;
	while(line< eol)
	{
		int w=get_chw(line);
		LPBYTE patb=telnet->GetLineAttr(line);

		int dx=chw*w;
		rc.right=rc.left+dx;
		bool update=IsAttrBlink( patb[0]);

		if( w==2 )	//�p�G�O���줸�զr�A�ˬd�O�_������r
		{
			//�p�G�O����r
			if( patb[0] != patb[1] )	//����X��Ӥ�r�A�A��X�e�b
			{
				if(IsAttrBlink( patb[1]) )
				{
					if( patb[1] != prevatb)
						draw_opt=SetDCColors(&dc,(prevatb=patb[1]) );

					if( !(draw_opt & ETO_OPAQUE) )
						FillBkRect(dc,rc,prevatb);
					ExtTextOut(dc, rc.left,rc.top,draw_opt,rc,line,(blight?2:0));
					update=true;
				}
				rc.right-=chw;
				draw_opt=ETO_CLIPPED;
			}
		}

		if(update)
		{
			if(patb[0] != prevatb )
			{
				draw_opt&=ETO_CLIPPED;
				draw_opt|=SetDCColors(&dc, (prevatb=patb[0]) );
			}
			if( !(draw_opt & ETO_OPAQUE) )
				FillBkRect(dc,rc,prevatb);

			ExtTextOut(dc, rc.left,rc.top,draw_opt,rc,line,
				((!IsAttrBlink( patb[0]) || blight)?w:0));
		}
		rc.left += dx;
		line += w;
	}
}


inline void CTermView::DrawLink(CDC &dc, LPSTR line, LPBYTE atbline, int y)
{
	if( !telnet->GetHyperLink(line) )
		return;

	int start,len;
	HPEN pen=NULL;
	y+=lineh-1;
	const char* plink=line;
	const char* eol=line+telnet->site_settings.cols_per_page;
	while( (plink < eol) && (plink=AppConfig.hyper_links.FindHyperLink(plink,len)) )
	{
		start= (int(plink)-int(line))*chw+left_margin;
		char tmp=plink[len];	((char*)plink)[len]=0;
		int t=AppConfig.hyper_links.GetURLType(plink);
		((char*)plink)[len]=tmp;
		if(t>=0)
		{
			pen=CreatePen(PS_SOLID,1,AppConfig.hyper_links.links[t].color);
			HGDIOBJ h_old=SelectObject(dc.m_hDC,pen);
			dc.MoveTo(start,y);
			dc.LineTo(start+len*chw,y);		
			SelectObject(dc.m_hDC,h_old);
			DeleteObject(pen);
		}
		plink+=len;
	}
}

inline void CTermView::DrawBlink()
{
	if(!telnet)
		return;
	CClientDC dc(this);

	GetClientRect(view_rect);
	if(AppConfig.bktype==1)
		ClientToScreen(view_rect);

	LPSTR* curline=telnet->screen+telnet->scroll_pos;
	LPSTR* lastline=curline+telnet->site_settings.lines_per_page;
	int y=top_margin;
	CFont* fold=dc.SelectObject(&fnt);
	
	if(AppConfig.old_textout)
	{
		for(;curline<lastline; curline++,y+=lineh)
			DrawLineBlinkOld(dc,*curline,y);
	}
	else
	{
		for(;curline<lastline; curline++,y+=lineh)
			DrawLineBlink(dc,*curline,y);
	}
	dc.SelectObject(fold);
}


inline LPBYTE find_blink(LPBYTE atbline,LPBYTE eoatb,int& len)
{
	LPBYTE patb; for(patb=atbline;!CTermView::IsAttrBlink(*patb) && patb<eoatb; patb++);
	if(patb>=eoatb)
		return NULL;
	atbline=patb;
	while(CTermView::IsAttrBlink(*patb) && *patb==*atbline && patb<eoatb)
		patb++;
	len=int(patb-atbline);
	return atbline;
}

inline void CTermView::FillBkRect(CDC &dc, CRect &rc, BYTE attr,BOOL bkinvirt/* =0 */)
{
	int i=GetAttrBkColor(attr);
	if(AppConfig.bktype==0 || i)
	{
		dc.SetBkColor(AppConfig.colormap[i]);
		dc.ExtTextOut(rc.left,rc.top,ETO_OPAQUE,rc,NULL,0,NULL);
	}
	else
		::BitBlt(dc.m_hDC,rc.left,rc.top,rc.Width(),rc.Height(),memdc,view_rect.left+rc.left,rc.top+view_rect.top, bkinvirt ? NOTSRCCOPY:SRCCOPY);
}

BOOL CTermView::OpenAnsFile(LPCTSTR filepath)
{
	CFile file;
	if(file.Open(filepath,CFile::modeRead))
	{
		OnAnsiEditor();
		char* title = (char*)_mbsrchr( (BYTE*)LPCTSTR(filepath), '\\');
		if(title)	title++;
		parent->tab.SetItemText( parent->tab.GetCurSel(), title);
		telnet->name = title;

		int len=file.GetLength();
		char *data=new char[len];
		file.Read(data,len);
		char *pdata=data,*eod=data+len;
		if(*pdata==0x0a)
		{
			*pdata=0x0d;
			pdata++;
		}

		while(pdata<eod)
		{
			if(*pdata==0x0a && *(pdata-1)!=0x0d)
				*pdata=0x0d;
			pdata++;
		}

		file.Abort();
		LockWindowUpdate();
		telnet->Send(data,len);
		telnet->scroll_pos=0;
		telnet->cursor_pos.x=telnet->cursor_pos.y=0;
		telnet->UpdateCursorPos();
		SetScrollBar();
		UnlockWindowUpdate();
		delete []data;
		telnet->address=filepath;
		parent->UpdateStatus();
		parent->UpdateAddressBar();
		return TRUE;
	}
	return FALSE;
}


void CTermView::SendAnsiString(CString data)	//�e�XESC�ഫ�᪺ANSI�r��
{
//�ϥαm��K�W�t������X�A�n�Ȯ������۰ʴ���\��
	BOOL tmp_autowrap=AppConfig.site_settings.paste_autowrap;
	AppConfig.site_settings.paste_autowrap=0;

	if( !telnet->is_ansi_editor && !telnet->site_settings.esc_convert.IsEmpty() )
	{
		char* pdata;
		char* buf=pdata=new char[data.GetLength()+1];
		memcpy(pdata,data,data.GetLength()+1);
		data.Empty();
		while( *pdata )
		{
			if(*pdata=='\x1b' && *(pdata+1)=='[' )
			{
				data+=telnet->site_settings.esc_convert;
				data+='[';
				pdata+=2;
				continue;
			}
			data+=*pdata;
			pdata++;
		}
		delete []buf;
	}
	telnet->SendString(data);

//���s��_�۰ʴ���
	AppConfig.site_settings.paste_autowrap=tmp_autowrap;
}

void CTermView::OnDestroyClipboard() 
{
	CWnd::OnDestroyClipboard();
	if(paste_block)
		paste_block=0;
}

void CTermView::OnSmoothDraw() 
{
	if(!AppConfig.smooth_draw){
		if(	MessageBox( LoadString(IDS_SMOOTH_DRAW_INFO)
			, LoadString(IDS_HELP),MB_OKCANCEL|MB_ICONEXCLAMATION)==IDOK)
			AppConfig.smooth_draw=TRUE;
	}else
		AppConfig.smooth_draw=FALSE;
	Invalidate(FALSE);
}

#if defined	_COMBO_
BOOL CTermView::SetWindowPos(const CWnd *pWndInsertAfter, int x, int y, int cx, int cy, UINT nFlags)
{
	if(con && !telnet)
		((CWebConn*)con)->web_browser.SetWindowPos(pWndInsertAfter,x,y,cx,cy,nFlags);
	return CWnd::SetWindowPos(pWndInsertAfter,x,y,cx,cy,nFlags);
}

void CTermView::MoveWindow(int x, int y, int nWidth, int nHeight, BOOL bRepaint)
{
	CWnd::MoveWindow(x, y, nWidth, nHeight, bRepaint);
	if(con && !telnet)
		((CWebConn*)con)->web_browser.MoveWindow(x, y, nWidth, nHeight, bRepaint);
}

CWebConn* CTermView::ConnectWeb(CString address, BOOL act)
{
	CWebConn* newcon = new CWebConn;
	newcon->web_browser.view=this;
	newcon->name = newcon->address = address;
	newcon->web_browser.parent=parent;
	newcon->web_browser.Create(NULL,NULL,WS_CHILD,CRect(0,0,0,0),parent,0);
	newcon->web_browser.wb_ctrl.SetRegisterAsBrowser(TRUE);
	newcon->web_browser.wb_ctrl.SetRegisterAsDropTarget(TRUE);
	parent->NewTab( newcon );

	if(!address.IsEmpty())
	{
		COleVariant v;
		COleVariant url=address;
		newcon->web_browser.wb_ctrl.Navigate2(&url,&v,&v,&v,&v);
	}
	if(act)
		parent->SwitchToConn( newcon );
	else
		newcon->web_browser.EnableWindow(FALSE);

	parent->FilterWebConn( newcon);
	return newcon;
}
#endif

void CMainFrame::SendFreqStr(CString str, BYTE inf)
{
	if(inf & 1<<6)	//control
		str=UnescapeControlChars(str);

	if(inf & 1<<7)
	{
		if( !(inf & 1<<6) )	//�S������X�ഫ
			str.Replace("^[[","\x1b[");		//�ഫ ^[ �� ESC

		view.SendAnsiString(str);
	}
	else
		view.telnet->SendString(str);
}

typedef char* (*strstrfunc)(const char*,const char*);
typedef char* (*strnrstrfunc)(const char*,const char*,int);
LRESULT CTermView::OnFind(WPARAM w, LPARAM l)
{
	if(!pfinddlg)	return 0;

	if( pfinddlg->IsTerminating())
	{
		pfinddlg=NULL;
		return 0;
	}

	if(!telnet)	return 0;

	CString find_str=pfinddlg->GetFindString();

	char* line;		char* found=NULL;
	RECT rc;	long y;	int startx;	int endx;	

	rc.left=left_margin+telnet->sel_start.x*chw;
	rc.right=rc.left+find_text.GetLength()*chw;
	rc.top=top_margin+(telnet->sel_start.y-telnet->scroll_pos)*lineh;
	rc.bottom=rc.top+lineh;

	bool first_time= (telnet->sel_start.x==telnet->sel_end.x)||(find_str!=find_text);

	int first_line=pfinddlg->MatchWholeWord()?0:telnet->first_line;
	if(pfinddlg->SearchDown())	//�V��j�M
	{
		if( first_time )	//�Ĥ@���M��
		{
			telnet->sel_start.y=telnet->sel_end.y=first_line;
			telnet->sel_start.x=telnet->sel_end.x=0;
		}

		strstrfunc pstrstr=pfinddlg->MatchCase()?strstr:strstri;
		y=telnet->sel_start.y;
		startx=telnet->sel_start.x;
		endx=telnet->sel_end.x;
		line=telnet->screen[y]+endx;
		while( !(found=(*pstrstr)(line,find_str)) )
		{
			y++;
			if(y >= telnet->site_settings.line_count )
				break;
			endx=0;
			line=telnet->screen[y];
		}
	}
	else	//�V�e�j�M
	{
		strnrstrfunc pstrnrstr=pfinddlg->MatchCase()?strnrstr:strnrstri;
		if( first_time )	//�Ĥ@���M��
		{
			telnet->sel_start.y=telnet->sel_end.y=telnet->last_line;
			telnet->sel_start.x=telnet->sel_end.x=telnet->site_settings.cols_per_page;
		}
		y=telnet->sel_end.y;
		startx=telnet->sel_start.x;
		endx=telnet->sel_end.x;
		line=telnet->screen[y];

		while( !(found=(*pstrnrstr)(line,find_str,startx)) && y > first_line)
		{
			y--;
			startx=telnet->site_settings.cols_per_page;
			line=telnet->screen[y];
		}
	}

	if(found)	//�p�G�����
	{
		InvalidateRect(&rc,FALSE);	//�M���쥻���¿����
		startx=(found-telnet->screen[y]);
		telnet->sel_start.x=startx;
		telnet->sel_end.y=telnet->sel_start.y=y;
		telnet->sel_end.x=startx+find_str.GetLength();

		if( telnet->scroll_pos> y || y >= telnet->scroll_pos+telnet->site_settings.lines_per_page )	//�p�G���b�ثe������
		{
			int pg=(y / telnet->site_settings.lines_per_page);	//��X�Ҧb������
			telnet->scroll_pos= pg * telnet->site_settings.lines_per_page;
			SetScrollPos(SB_VERT,telnet->scroll_pos);
			Invalidate(FALSE);
		}
	}

	rc.left=left_margin+startx*chw;
	rc.right=rc.left+find_str.GetLength()*chw;
	rc.top=top_margin+(y-telnet->scroll_pos)*lineh;
	rc.bottom=rc.top+lineh;
	InvalidateRect(&rc,FALSE);

	if(!found)
		MessageBox( LoadString(IDS_NOT_FOUND)+" "+find_str);

	find_text=find_str;
	return 0;
}

#include <dlgs.h>

//	AUTOCHECKBOX    "Match &whole word only", chx1, 4, 26, 100, 12, WS_GROUP
//	�����ɥ�FindDialog��Match Case�@���M������w�İϪ��ﶵ :)

void CTermView::FindStart()
{
	telnet->sel_end.x=0;
	telnet->sel_start=telnet->sel_end;
	if(!pfinddlg)
	{
		pfinddlg=new CFindReplaceDialog;
		pfinddlg->m_fr.lpstrFindWhat=(LPTSTR)(LPCTSTR)find_text;
		pfinddlg->Create(TRUE,NULL,NULL,FR_DOWN,this);
		pfinddlg->GetDlgItem(chx1)->SetWindowText( LoadString(IDS_FIND_IN_ALL_BUF));	//"�M���ӽw�İ�(&B)"
		pfinddlg->ShowWindow(SW_SHOW);
	}
	else
		pfinddlg->SetForegroundWindow();
}

inline void CTermView::PtToLineCol(POINT pt, int &x, int &y,bool adjust_x)
{
	pt.x-=left_margin;	pt.y-=top_margin;
	if(pt.x<0)	pt.x=0;
	if(pt.y<0)	pt.y=0;

	x=(pt.x/chw);	y=(pt.y/lineh);
	if( y >= telnet->site_settings.lines_per_page )
		y=telnet->site_settings.lines_per_page-1;

	LPSTR curstr=telnet->screen[telnet->scroll_pos+y];

	if(adjust_x)
	{
		if(telnet->site_settings.auto_dbcs_mouse)
		{
		//-----------�s���䴩���媺�y�Эp��-----------
			if(x>0 && IsBig5(curstr,x-1) )	//�p�G��ܤ����b�q�A�N����U�@�Ӧr
				x++;
			else if(!IsBig5(curstr,x) && (pt.x%chw)*2>chw)	//�p�G�]���O����e�b�A�~�O�^��
				x++;
		}
		else
		{
		//------------���Ҽ{���줸�ժ��y�Эp��-----------
			if( (pt.x%chw)*2>chw )
				x++;
		}
	}

	if(x>telnet->site_settings.cols_per_page)
		x=telnet->site_settings.cols_per_page;

	if(pt.x<chw)
		x=0;
}


inline char* CTermView::HyperLinkHitTest(int x, int y, int& len)	//�ΨӴ��յe���W�Y�I�O�_���W�s��
//x,y���׺ݾ���C�y�СA�Ӥ��O�ƹ��y��
{
	y+=telnet->scroll_pos;
	const char* plink=telnet->screen[y];

	if(!telnet->GetHyperLink(plink))
		return NULL;

	const char* eol=plink+telnet->site_settings.cols_per_page;
	while(plink=AppConfig.hyper_links.FindHyperLink(plink,len))
	{
		char* px=telnet->screen[y]+x;
		if(px >= plink && px< plink+len)
			return (char*)plink;
		plink+=len;
	}
	return NULL;
}

void CTermView::OnCurConSettings() 
{
	CPropertySheet dlg(IDS_CUR_CON_SETTINGS);
	CSiteSettings tmpset;

	if(!telnet->cfg_filepath.IsEmpty())	//���ո��J�ӧO�]�w
		tmpset.Load(telnet->cfg_filepath);
	tmpset = telnet->site_settings;	//�èϥΥثe�]�w���л\�L�k���J������

	CSitePage page1;
	page1.psettings=&tmpset;

	CAutoReplyPage page2;
	page2.triggers=&tmpset.triggers;
	dlg.AddPage(&page1);
	dlg.AddPage(&page2);
	if(dlg.DoModal()==IDOK)
	{
		if(!telnet->cfg_filepath.IsEmpty())
			tmpset.Save(telnet->cfg_filepath);

		//���s���J�r��Ĳ�o
		telnet->site_settings.triggers.CopyFrom(tmpset.triggers);

		if( strncmp(telnet->site_settings.KeyMapName,tmpset.KeyMapName,10) )	//�p�G��L��M���
		{
			telnet->key_map->Release();
			telnet->key_map=CKeyMap::Load(tmpset.KeyMapName);
			strcpy(telnet->site_settings.KeyMapName,tmpset.KeyMapName);
		}
		CRect rc;	GetClientRect(rc);
		telnet->ReSizeBuffer(tmpset.line_count,tmpset.cols_per_page,tmpset.lines_per_page);
		telnet->site_settings = tmpset;
		telnet->SendNaws();
		AdjustFont(rc.right,rc.bottom);
		Invalidate(FALSE);
	}
}

void CTermView::OnEditOpenURL(UINT id) 
{
	CString url=GetSelText();
	url.Replace("\x0d\x0a","");
	url.Replace(" ","");
	telnet->sel_start=telnet->sel_end;
	Invalidate(FALSE);
	switch(id)
	{
	case ID_EDIT_OPENURL_HTTP:
		url="http://"+url;
		break;
	case ID_EDIT_OPENURL_FTP:
		url="ftp://"+url;
		break;
	case ID_EDIT_OPENURL_TELNET:
		parent->OnNewConnectionAds(url);
		return;
	}
	AppConfig.hyper_links.OpenURL(url);
}

void CTermView::OnUpdateBBSList() 
{
	ShellExecute(AfxGetMainWnd()->m_hWnd,"open",AppPath+"UBL.exe",AppPath+"sites.dat",AppPath,SW_SHOW);
}


CString CTermView::GetSelText()
{
	CString ret;
	if(telnet->sel_end.x!=telnet->sel_start.x || telnet->sel_end.y!=telnet->sel_start.y)
	{
		UINT tmp;

		bool bottom2top = (telnet->sel_end.y < telnet->sel_start.y);
		if( bottom2top)
		{
			tmp=telnet->sel_end.y;
			telnet->sel_end.y=telnet->sel_start.y;
			telnet->sel_start.y=tmp;
		}

		long selstarty=telnet->sel_start.y;
		long selendy=telnet->sel_end.y;

		bool single_line_sel = (selstarty == selendy);

		if( (bottom2top || telnet->sel_block || single_line_sel ) && telnet->sel_end.x<telnet->sel_start.x )
		{
			tmp=telnet->sel_end.x;
			telnet->sel_end.x=telnet->sel_start.x;
			telnet->sel_start.x=tmp;
		}

		LPSTR data=NULL;
		int len=0;

		if(single_line_sel)	//if a single line is selected
		{
			len=telnet->sel_end.x-telnet->sel_start.x;
			data=ret.GetBuffer(len+1);
			memset(data,0,len+1);
			strncpy(data,telnet->screen[selstarty]+telnet->sel_start.x,len);
			if(telnet->sel_block)	//�p�G�϶����
				paste_block=TRUE;
			strstriptail(data);
		}
		else	//select several line
		{
			char crlf[]={13,10,0};
			if(telnet->sel_block)	//�p�G�϶����
			{
				paste_block=TRUE;
				int oll=telnet->sel_end.x-telnet->sel_start.x;

				len=(oll+2)*(telnet->sel_end.y-telnet->sel_start.y+1)+1;
				data=ret.GetBuffer(len);
				char* pdata=data;
				for(int i=selstarty;i<=selendy;i++)
				{
					memcpy(pdata,telnet->screen[i]+telnet->sel_start.x,oll);
					*(pdata+oll)=0;
					pdata=strstriptail(pdata);
					memcpy(pdata,crlf,2);
					pdata+=2;
				}
				*(pdata-2)=0;
			}
			else
			{
				len=(telnet->site_settings.cols_per_page+2)-telnet->sel_start.x;
				len+=(telnet->site_settings.cols_per_page+2)*(selendy-selstarty);
				len+=telnet->sel_end.x+1;
				data=ret.GetBuffer(len+10);
				memset(data,0,len+10);
				strcpy(data,telnet->screen[selstarty]+telnet->sel_start.x);
				strstriptail(data);
				strcat(data,crlf);
				for(int i=selstarty+1;i<selendy;i++)
				{
					strcat(data,telnet->screen[i]);
					strstriptail(data);
					strcat(data,crlf);
				}
				len=strlen(data);
				strncpy(data+len,telnet->screen[i],telnet->sel_end.x);
				strstriptail(data);	
			}
		}
		ret.ReleaseBuffer();
	}
	return ret;
}

LRESULT CTermView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if(AppConfig.lock_pcman &&
		message!=WM_TIMER &&
		message != WM_SOCKET &&
		message != WM_DNSLOOKUP_END)
		return 0;
	return CWnd::WindowProc(message, wParam, lParam);
}


void CTermView::AdjustFont(int cx,int cy)
{
//-----�ʺA�r��վ�------
	int cols_per_page=telnet?telnet->site_settings.cols_per_page:AppConfig.site_settings.cols_per_page;
	int lines_per_page=telnet?telnet->site_settings.lines_per_page:AppConfig.site_settings.lines_per_page;
	if(AppConfig.auto_font)
	{
		int x=cx/(cols_per_page/2);
		int y=cy/lines_per_page;

//		if(AppConfig.old_textout)
		{
			if(x%2)
				x--;
			if(y%2)
				y--;
		}
		AppConfig.font_info.lfHeight=y;
		chw=AppConfig.font_info.lfWidth=x/2;
		fnt.DeleteObject();
		fnt.CreateFontIndirect(&AppConfig.font_info);

		CWindowDC dc(this);
		CGdiObject* pold=dc.SelectObject(&fnt);
		CSize &sz=dc.GetTextExtent("�@",2);
		chw=sz.cx/2;
//		lineh=y;
		lineh=sz.cy;
//---------------------
	}
	left_margin=(cx-chw*cols_per_page)/2;
	top_margin=(cy-lineh*lines_per_page)/2;

	CreateCaret();
	ShowCaret();
	if(telnet)
		telnet->UpdateCursorPos();
	else
		SetCaretPos(CPoint(left_margin,top_margin+lineh-2));
}

void CTermView::ConnectStr(CString name, CString dir)
{
	CString address;
	short port;
	int i=name.Find('\t');
	address=name.Mid(i+1);
	char type=name[0];
	name=name.Mid(1,i-1);

#if defined(_COMBO_)
	if(type!='s')
	{
		ConnectWeb(address,TRUE);
		return;
	}
#endif

	i=address.ReverseFind(':');
	if(i==-1)
		port = 23;
	else
	{
		port = (short)atoi( LPCTSTR( address.Mid(i+1) ) );
		address=address.Left(i);
	}
	SetFocus();
	Connect( address, name, port, dir + name );
}


void CTermView::DrawLineOld(CDC &dc, LPSTR line,
		BYTE* pline_selstart, BYTE* pline_selend, int y)
{
	LPBYTE atbline=telnet->GetLineAttr(line);
	LPSTR hol=line;	//head of line
	LPSTR eol=line+telnet->site_settings.cols_per_page;	//end of line
	LPBYTE patb=atbline;	//attributes
	int l;	//�C����X����r����
	int x=left_margin;	//x position
	CRect rc;	//�Ψӿ�X��r��rect
	rc.top=y;	rc.bottom=y+lineh;

//	�}�l��X��r
	while(line<eol)
	{
		BYTE attr;	BOOL bsel;
		for( attr=*patb,bsel=( patb>=pline_selstart && patb< pline_selend);
			attr==*patb && bsel==( patb>=pline_selstart && patb< pline_selend);
			patb++);	//�p�G�O�ۦP�ݩʴN�V�Ჾ��

		l=patb-atbline;	//�r�����
		UINT drawopt=AppConfig.bktype?0:ETO_OPAQUE;
		bool textout=(!IsAttrBlink(attr) || blight || telnet->sel_end!=telnet->sel_start);
		if(IsBig5(hol,line+l-1))	//�̫�@�Ӧr�p�G�O����e�b�q
		{
			int x2=x+l*chw-chw;
			rc.left=x2;
			rc.right=x2+chw*2;
			BOOL bsel2=( patb>=pline_selstart && patb< pline_selend);
			SetDCColors(&dc,*patb,bsel2);
			if(AppConfig.bktype)
			{
				if( GetAttrBkColor(*patb) )
					drawopt|=ETO_OPAQUE;
				else
					FillBkRect(dc,rc,0,bsel2);
			}
			if(!IsAttrBlink(*patb) || blight || telnet->sel_end!=telnet->sel_start)
				ExtTextOut(dc, x2,y,drawopt,rc,line+l-1,2);	//����X��Ӥ���A�A�����e�b
			else // only draw background
				dc.ExtTextOut(x2,y,drawopt,rc,NULL,0,NULL);

			drawopt=ETO_CLIPPED;
			rc.left=x,rc.right=x+chw*l;
			l++;
			SetDCColors(&dc,attr,bsel);
			if(AppConfig.bktype==0 ||GetAttrBkColor(attr))
				drawopt|=ETO_OPAQUE;
			else
				FillBkRect(dc,rc,0,bsel);

			if(textout) // output text
				ExtTextOut(dc,x,y,drawopt,rc,line,l);
			else // only background
				dc.ExtTextOut(x,y,drawopt,rc,NULL,0,NULL);

			line+=l;
			patb++;
		}
		else
		{
			SetDCColors(&dc,attr,bsel);
			CRect rc(x,y,x+l*chw,y+lineh);
			if(AppConfig.bktype==0 ||GetAttrBkColor(attr))
				drawopt|=ETO_OPAQUE;
			else
			{
				FillBkRect(dc,rc,0,bsel);
			}
			if(textout) // output text
				ExtTextOut(dc,x,y,drawopt,rc,line,l);
			else // only background
				dc.ExtTextOut(x,y,drawopt,rc,NULL,0,NULL);

			line+=l;
		}
		atbline=patb;
		x+=chw*l;
	}
}

inline void CTermView::DrawLineBlinkOld(CDC &dc, LPSTR line, int y)
{
	LPBYTE atbline=telnet->GetLineAttr(line);
	LPSTR hol=line;
	LPBYTE eoatb=atbline+telnet->site_settings.cols_per_page;
	LPBYTE pblink;
	int l=0;
	int x=left_margin;
	while( (pblink=find_blink(atbline,eoatb,l)) )
	{
		int pos=int(pblink-atbline);
		x+=chw*pos;
		line+=pos;
		//�}�l��X��r
		CRect rc(x,y,0,y+lineh);
		UINT drawopt=0;
		//�p�G�Ĥ@�Ӧr�O�����b
		if(hol!=line && IsBig5(hol,line-1))
		{
			rc.right=x+chw;
			rc.left-=chw;
			SetDCColors(&dc,*pblink);
			if(AppConfig.bktype==0 || GetAttrBkColor(*pblink))
				drawopt=ETO_OPAQUE;
			if(blight)
				ExtTextOut(dc, rc.left,y,drawopt,rc,line-1,2);
			else
				FillBkRect(dc,rc,*pblink);
			rc.right-=chw;
			drawopt=ETO_CLIPPED;
			BYTE tmpatb=*(pblink-1);
			SetDCColors(&dc,tmpatb);
			if(AppConfig.bktype==0 || GetAttrBkColor(tmpatb))
				drawopt|=ETO_OPAQUE;

			FillBkRect(dc,rc,tmpatb);
			if(blight || !IsAttrBlink(tmpatb))
				ExtTextOut(dc, rc.left,y,drawopt,rc,line-1,2);

			//���L�Ĥ@�Ӧr���ݩʦ�m�M��r���e
			line++;
			x+=chw;
			l--;
			pblink++;
		}

		drawopt=0;
		//�p�G�̫�@�Ӧr�O����e�b
		if(IsBig5(hol,line+l-1))
		{
			rc.left=x+chw*l-chw;
			rc.right=rc.left+chw*2;
			BYTE tmpatb=*(pblink+l);
			SetDCColors(&dc,tmpatb);
			if(AppConfig.bktype==0 || GetAttrBkColor(tmpatb))
				drawopt=ETO_OPAQUE;

			if(blight||!IsAttrBlink(tmpatb))
				ExtTextOut(dc, rc.left,y,drawopt,rc,line+l-1,2);
			else
				FillBkRect(dc,rc,tmpatb);

			drawopt=ETO_CLIPPED;
			rc.right-=chw;
			l++;
		}
		else
			rc.right=x+l*chw;

		rc.left=x;
		SetDCColors(&dc,*pblink);
		if(AppConfig.bktype==0 || GetAttrBkColor(*pblink))
		{
			drawopt|=ETO_OPAQUE;
			if(blight)
				ExtTextOut(dc, rc.left,y,drawopt,rc,line,l);
			else
				FillBkRect(dc,rc,*pblink);
		}
		else
		{
			FillBkRect(dc,rc,*pblink);
			if(blight)
				ExtTextOut(dc, rc.left,y,drawopt,rc,line,l);
		}

		//��X��r����

		atbline=pblink+l;
		line+=l;
		x+=chw*l;
	}
}

BOOL CTermView::ExtTextOut(CDC& dc, int x, int y, UINT nOptions, LPCRECT lpRect, LPCTSTR lpszString, UINT nCount)
{
	char buf[161];	// maximum of every line on the bbs screen.
	if( nCount > 1 )
	{
		// FIXME: support unicode addon here?
		// We can convert characters not defined in big5 to unicode with our own table
		// gotten from UAO project.
		// ::ExtTextOutW() can be used to draw unicode strings, even in Win 98.

		switch( telnet->site_settings.text_output_conv )
		{
		case 0:
			break;
		case GB2BIG5:
			chi_conv.GB2Big5( lpszString, buf, nCount );
			lpszString = buf;
			break;
		case BIG52GB:
			chi_conv.Big52GB( lpszString, buf, nCount );
			lpszString = buf;
		};
	}
	return dc.ExtTextOut(x,y,nOptions,lpRect,lpszString,nCount,NULL);
}


void CTermView::CopySelText()
{
	CString seltext=GetSelText();

	if(!seltext.IsEmpty() )
	{
		CClipboard::SetText(m_hWnd,seltext);
		paste_block=telnet->sel_block;
	}

	if(AppConfig.auto_cancelsel)
	{
		telnet->sel_end=telnet->sel_start;
		Invalidate(FALSE);
	}
}