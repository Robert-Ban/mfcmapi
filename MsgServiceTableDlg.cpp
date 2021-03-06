// MsgServiceTableDlg.cpp : implementation file
// Displays the list of services in a profile

#include "stdafx.h"
#include "MsgServiceTableDlg.h"
#include "ContentsTableListCtrl.h"
#include "MapiObjects.h"
#include "SingleMAPIPropListCtrl.h"
#include "ColumnTags.h"
#include "MFCUtilityFunctions.h"
#include "ProviderTableDlg.h"
#include "MAPIProfileFunctions.h"
#include "Editor.h"
#include "MAPIFunctions.h"

static TCHAR* CLASS = _T("CMsgServiceTableDlg");

/////////////////////////////////////////////////////////////////////////////
// CMsgServiceTableDlg dialog


CMsgServiceTableDlg::CMsgServiceTableDlg(
	_In_ CParentWnd* pParentWnd,
	_In_ CMapiObjects* lpMapiObjects,
	_In_z_ LPCSTR szProfileName
	):
CContentsTableDlg(
				  pParentWnd,
				  lpMapiObjects,
				  IDS_SERVICES,
				  mfcmapiDO_NOT_CALL_CREATE_DIALOG,
				  NULL,
				  (LPSPropTagArray) &sptSERVICECols,
				  NUMSERVICECOLUMNS,
				  SERVICEColumns,
				  IDR_MENU_MSGSERVICE_POPUP,
				  MENU_CONTEXT_PROFILE_SERVICES)
{
	TRACE_CONSTRUCTOR(CLASS);

	m_szProfileName = NULL;
	m_lpServiceAdmin = NULL;

	CreateDialogAndMenu(IDR_MENU_MSGSERVICE);

	(void) CopyStringA(&m_szProfileName,szProfileName,NULL);
	OnRefreshView();
} // CMsgServiceTableDlg::CMsgServiceTableDlg

CMsgServiceTableDlg::~CMsgServiceTableDlg()
{
	TRACE_DESTRUCTOR(CLASS);
	if (m_szProfileName) MAPIFreeBuffer(m_szProfileName);
	m_szProfileName = NULL;
	// little hack to keep our releases in the right order - crash in o2k3 otherwise
	if (m_lpContentsTable) m_lpContentsTable->Release();
	m_lpContentsTable = NULL;
	if (m_lpServiceAdmin) m_lpServiceAdmin->Release();
} // CMsgServiceTableDlg::~CMsgServiceTableDlg

BEGIN_MESSAGE_MAP(CMsgServiceTableDlg, CContentsTableDlg)
	ON_COMMAND(ID_CONFIGUREMSGSERVICE,OnConfigureMsgService)
	ON_COMMAND(ID_DELETESELECTEDITEM,OnDeleteSelectedItem)
	ON_COMMAND(ID_OPENPROFILESECTION,OnOpenProfileSection)
END_MESSAGE_MAP()

void CMsgServiceTableDlg::OnInitMenu(_In_ CMenu* pMenu)
{
	if (pMenu)
	{
		if (m_lpContentsTableListCtrl)
		{
			int iNumSel = m_lpContentsTableListCtrl->GetSelectedCount();
			pMenu->EnableMenuItem(ID_DELETESELECTEDITEM,DIMMSOK(iNumSel));
			pMenu->EnableMenuItem(ID_CONFIGUREMSGSERVICE,DIMMSOK(iNumSel));
		}
	}
	CContentsTableDlg::OnInitMenu(pMenu);
} // CMsgServiceTableDlg::OnInitMenu

/////////////////////////////////////////////////////////////////////////////
// CMsgServiceTableDlg message handlers

// Clear the current list and get a new one with whatever code we've got in LoadMAPIPropList
void CMsgServiceTableDlg::OnRefreshView()
{
	DebugPrintEx(DBGGeneric,CLASS,_T("OnRefreshView"),_T("\n"));

	HRESULT hRes = S_OK;

	// Make sure we've got something to work with
	if (!m_szProfileName || !m_lpContentsTableListCtrl || !m_lpMapiObjects) return;

	// cancel any loading which may be occuring
	if (m_lpContentsTableListCtrl->IsLoading()) m_lpContentsTableListCtrl->OnCancelTableLoad();

	// Clean up our table and admin in reverse order from which we obtained them
	// Failure to do this leads to crashes in Outlook's profile code
	EC_H(m_lpContentsTableListCtrl->SetContentsTable(
		NULL,
		dfNormal,
		NULL));

	if (m_lpServiceAdmin) m_lpServiceAdmin->Release();
	m_lpServiceAdmin = NULL;

	LPPROFADMIN lpProfAdmin = NULL;
	EC_MAPI(MAPIAdminProfiles(0, &lpProfAdmin));

	if (lpProfAdmin)
	{
#pragma warning(push)
#pragma warning(disable:4616)
#pragma warning(disable:6276)
		EC_MAPI(lpProfAdmin->AdminServices(
			(TCHAR*)m_szProfileName,
			(TCHAR*)"",
			NULL,
			MAPI_DIALOG,
			&m_lpServiceAdmin));
#pragma warning(pop)
		if (m_lpServiceAdmin)
		{
			LPMAPITABLE lpServiceTable = NULL;

			EC_MAPI(m_lpServiceAdmin->GetMsgServiceTable(
				0, // fMapiUnicode is not supported
				&lpServiceTable));

			if (lpServiceTable)
			{
				EC_H(m_lpContentsTableListCtrl->SetContentsTable(
					lpServiceTable,
					dfNormal,
					NULL));

				lpServiceTable->Release();
			}
		}
		lpProfAdmin->Release();
	}
} // CMsgServiceTableDlg::OnRefreshView

void CMsgServiceTableDlg::OnDisplayItem()
{
	HRESULT			hRes = S_OK;
	LPSBinary		lpServiceUID = NULL;
	LPPROVIDERADMIN	lpProviderAdmin = NULL;
	LPMAPITABLE		lpProviderTable = NULL;
	int				iItem = -1;
	SortListData*	lpListData = NULL;
	CWaitCursor	Wait; // Change the mouse to an hourglass while we work.

	if (!m_lpContentsTableListCtrl || !m_lpServiceAdmin) return;

	do
	{
		hRes = S_OK;
		lpListData = m_lpContentsTableListCtrl->GetNextSelectedItemData(&iItem);
		if (lpListData)
		{
			lpServiceUID = lpListData->data.Contents.lpServiceUID;
			if (lpServiceUID)
			{
				EC_MAPI(m_lpServiceAdmin->AdminProviders(
					(LPMAPIUID) lpServiceUID->lpb,
					0, // fMapiUnicode is not supported
					&lpProviderAdmin));

				if (lpProviderAdmin)
				{
					EC_MAPI(lpProviderAdmin->GetProviderTable(
						0, // fMapiUnicode is not supported
						&lpProviderTable));

					if (lpProviderTable)
					{
						new CProviderTableDlg(
							m_lpParent,
							m_lpMapiObjects,
							lpProviderTable,
							lpProviderAdmin);
						lpProviderTable->Release();
						lpProviderTable = NULL;
					}
					lpProviderAdmin->Release();
					lpProviderAdmin = NULL;
				}
			}
		}
	}
	while (iItem != -1);
} // CMsgServiceTableDlg::OnDisplayItem

void CMsgServiceTableDlg::OnConfigureMsgService()
{
	HRESULT			hRes = S_OK;
	LPSBinary		lpServiceUID = NULL;
	int				iItem = -1;
	SortListData*	lpListData = NULL;
	CWaitCursor	Wait; // Change the mouse to an hourglass while we work.

	if (!m_lpContentsTableListCtrl || !m_lpServiceAdmin) return;

	do
	{
		hRes = S_OK;
		lpListData = m_lpContentsTableListCtrl->GetNextSelectedItemData(&iItem);
		if (lpListData)
		{
			lpServiceUID = lpListData->data.Contents.lpServiceUID;
			if (lpServiceUID)
			{
				EC_H_CANCEL(m_lpServiceAdmin->ConfigureMsgService(
					(LPMAPIUID) lpServiceUID->lpb,
					(ULONG_PTR) m_hWnd,
					SERVICE_UI_ALWAYS,
					0,
					0));
			}
		}
	}
	while (iItem != -1);
} // CMsgServiceTableDlg::OnConfigureMsgService

_Check_return_ HRESULT CMsgServiceTableDlg::OpenItemProp(int iSelectedItem, __mfcmapiModifyEnum /*bModify*/, _Deref_out_opt_ LPMAPIPROP* lppMAPIProp)
{
	HRESULT		hRes = S_OK;
	LPSBinary	lpServiceUID = NULL;
	SortListData*	lpListData = NULL;

	DebugPrintEx(DBGOpenItemProp,CLASS,_T("OpenItemProp"),_T("iSelectedItem = 0x%X\n"),iSelectedItem);

	*lppMAPIProp = NULL;

	if (!m_lpServiceAdmin || !m_lpContentsTableListCtrl || !lppMAPIProp) return MAPI_E_INVALID_PARAMETER;

	lpListData = (SortListData*) m_lpContentsTableListCtrl->GetItemData(iSelectedItem);
	if (lpListData)
	{
		lpServiceUID = lpListData->data.Contents.lpServiceUID;
		if (lpServiceUID)
		{
			EC_H(OpenProfileSection(
				m_lpServiceAdmin,
				lpServiceUID,
				(LPPROFSECT*) lppMAPIProp));
		}
	}
	return hRes;
} // CMsgServiceTableDlg::OpenItemProp

void CMsgServiceTableDlg::OnOpenProfileSection()
{
	HRESULT			hRes = S_OK;

	if (!m_lpServiceAdmin) return;

	CEditor MyUID(
		this,
		IDS_OPENPROFSECT,
		IDS_OPENPROFSECTPROMPT,
		2,
		CEDITOR_BUTTON_OK|CEDITOR_BUTTON_CANCEL);

	MyUID.InitPane(0, CreateDropDownGuidPane(IDS_MAPIUID, false));
	MyUID.InitPane(1, CreateCheckPane(IDS_MAPIUIDBYTESWAPPED, false, false));

	WC_H(MyUID.DisplayDialog());
	if (S_OK != hRes) return;

	GUID guid = {0};
	SBinary MapiUID = {sizeof(GUID),(LPBYTE) &guid};
	(void) MyUID.GetSelectedGUID(0, MyUID.GetCheck(1), &guid);

	LPPROFSECT lpProfSect = NULL;
	EC_H(OpenProfileSection(
		m_lpServiceAdmin,
		&MapiUID,
		&lpProfSect));
	if (lpProfSect)
	{
		LPMAPIPROP lpTemp = NULL;
		EC_MAPI(lpProfSect->QueryInterface(IID_IMAPIProp,(LPVOID*) &lpTemp));
		if (lpTemp)
		{
			EC_H(DisplayObject(
				lpTemp,
				MAPI_PROFSECT,
				otContents,
				this));
			lpTemp->Release();
		}
		lpProfSect->Release();
	}
	MAPIFreeBuffer(MapiUID.lpb);
} // CMsgServiceTableDlg::OnOpenProfileSection

void CMsgServiceTableDlg::OnDeleteSelectedItem()
{
	HRESULT		hRes = S_OK;
	int			iItem = -1;
	LPSBinary	lpServiceUID = NULL;
	SortListData*	lpListData = NULL;

	if (!m_lpServiceAdmin || !m_lpContentsTableListCtrl) return;

	do
	{
		hRes = S_OK;
		// Find the highlighted item AttachNum
		lpListData = m_lpContentsTableListCtrl->GetNextSelectedItemData(&iItem);
		if (!lpListData) break;

		DebugPrintEx(DBGDeleteSelectedItem,CLASS,_T("OnDeleteSelectedItem"),_T("Deleting service from \"%hs\"\n"),lpListData->data.Contents.szProfileDisplayName);

		lpServiceUID = lpListData->data.Contents.lpServiceUID;
		if (lpServiceUID)
		{
			WC_MAPI(m_lpServiceAdmin->DeleteMsgService(
				(LPMAPIUID) lpServiceUID->lpb));
		}
	}
	while (iItem != -1);

	OnRefreshView(); // Update the view since we don't have notifications here.
} // CMsgServiceTableDlg::OnDeleteSelectedItem

void CMsgServiceTableDlg::HandleAddInMenuSingle(
	_In_ LPADDINMENUPARAMS lpParams,
	_In_ LPMAPIPROP lpMAPIProp,
	_In_ LPMAPICONTAINER /*lpContainer*/)
{
	if (lpParams)
	{
		lpParams->lpProfSect = (LPPROFSECT) lpMAPIProp; // OpenItemProp returns LPPROFSECT
	}

	InvokeAddInMenu(lpParams);
} // CMsgServiceTableDlg::HandleAddInMenuSingle
