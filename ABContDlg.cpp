// AbContDlg.cpp : implementation file
// Displays the hierarchy tree of address books

#include "stdafx.h"
#include "AbContDlg.h"
#include "HierarchyTableTreeCtrl.h"
#include "MapiFunctions.h"
#include "MapiObjects.h"
#include "SingleMAPIPropListCtrl.h"

/////////////////////////////////////////////////////////////////////////////
// CAbContDlg dialog


static TCHAR* CLASS = _T("CAbContDlg");

CAbContDlg::CAbContDlg(
					   _In_ CParentWnd* pParentWnd,
					   _In_ CMapiObjects* lpMapiObjects
					   ):
CHierarchyTableDlg(
				   pParentWnd,
				   lpMapiObjects,
				   IDS_ABCONT,
				   NULL,
				   IDR_MENU_ABCONT_POPUP,
				   MENU_CONTEXT_AB_TREE)
{
	TRACE_CONSTRUCTOR(CLASS);

	HRESULT	hRes = S_OK;

	m_bIsAB = true;

	if (m_lpMapiObjects)
	{
		LPADRBOOK lpAddrBook = m_lpMapiObjects->GetAddrBook(false); // do not release
		if (lpAddrBook)
		{
			// Open root address book (container).
			EC_H(CallOpenEntry(
				NULL,lpAddrBook,NULL,NULL,
				0,
				NULL,
				MAPI_BEST_ACCESS,
				NULL,
				(LPUNKNOWN*)&m_lpContainer));
		}
	}

	CreateDialogAndMenu(IDR_MENU_ABCONT);
} // CAbContDlg::CAbContDlg

CAbContDlg::~CAbContDlg()
{
	TRACE_DESTRUCTOR(CLASS);
} // CAbContDlg::~CAbContDlg

BEGIN_MESSAGE_MAP(CAbContDlg, CHierarchyTableDlg)
	ON_COMMAND(ID_SETDEFAULTDIR, OnSetDefaultDir)
	ON_COMMAND(ID_SETPAB, OnSetPAB)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////////////
//  Menu Commands

void CAbContDlg::OnSetDefaultDir()
{
	HRESULT hRes = S_OK;
	CWaitCursor	Wait; // Change the mouse to an hourglass while we work.

	if (!m_lpMapiObjects || !m_lpHierarchyTableTreeCtrl) return;

	LPSBinary lpItemEID = NULL;
	lpItemEID = m_lpHierarchyTableTreeCtrl->GetSelectedItemEID();

	if (lpItemEID)
	{
		LPADRBOOK lpAddrBook = m_lpMapiObjects->GetAddrBook(false); // Do not release
		if (lpAddrBook)
		{
			EC_MAPI(lpAddrBook->SetDefaultDir(
				lpItemEID->cb,
				(LPENTRYID)lpItemEID->lpb));
		}
	}
} // CAbContDlg::OnSetDefaultDir

void CAbContDlg::OnSetPAB()
{
	HRESULT			hRes = S_OK;
	CWaitCursor	Wait; // Change the mouse to an hourglass while we work.

	if (!m_lpMapiObjects || !m_lpHierarchyTableTreeCtrl) return;

	LPSBinary	lpItemEID = NULL;
	lpItemEID = m_lpHierarchyTableTreeCtrl->GetSelectedItemEID();

	if (lpItemEID)
	{
		LPADRBOOK lpAddrBook = m_lpMapiObjects->GetAddrBook(false); // do not release
		if (lpAddrBook)
		{
			EC_MAPI(lpAddrBook->SetPAB(
				lpItemEID->cb,
				(LPENTRYID)lpItemEID->lpb));
		}
	}
} // CAbContDlg::OnSetPAB

void CAbContDlg::HandleAddInMenuSingle(
									   _In_ LPADDINMENUPARAMS lpParams,
									   _In_ LPMAPIPROP /*lpMAPIProp*/,
									   _In_ LPMAPICONTAINER lpContainer)
{
	if (lpParams)
	{
		lpParams->lpAbCont = (LPABCONT) lpContainer;
	}

	InvokeAddInMenu(lpParams);
} // CAbContDlg::HandleAddInMenuSingle