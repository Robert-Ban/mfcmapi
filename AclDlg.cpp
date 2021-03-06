// AclDlg.cpp : implementation file
// Displays the ACL table for an item

#include "stdafx.h"
#include "AclDlg.h"
#include "ContentsTableListCtrl.h"
#include "Editor.h"
#include "MapiObjects.h"
#include "ColumnTags.h"
#include "SingleMAPIPropListCtrl.h"
#include "InterpretProp2.h"

static TCHAR* CLASS = _T("CAclDlg");

#define ACL_INCLUDE_ID			0x00000001
#define ACL_INCLUDE_OTHER		0x00000002

/////////////////////////////////////////////////////////////////////////////
// CAclDlg dialog

CAclDlg::CAclDlg(_In_ CParentWnd* pParentWnd,
				 _In_ CMapiObjects* lpMapiObjects,
				 _In_ LPEXCHANGEMODIFYTABLE lpExchTbl,
				 bool fFreeBusyVisible)
				 : CContentsTableDlg(pParentWnd,
				 lpMapiObjects,
				 (fFreeBusyVisible ? IDS_ACLFBTABLE : IDS_ACLTABLE),
				 mfcmapiDO_NOT_CALL_CREATE_DIALOG,
				 NULL,
				 (LPSPropTagArray) &sptACLCols,
				 NUMACLCOLUMNS,
				 ACLColumns,
				 IDR_MENU_ACL_POPUP,
				 MENU_CONTEXT_ACL_TABLE)
{
	TRACE_CONSTRUCTOR(CLASS);
	m_lpExchTbl = lpExchTbl;

	if (m_lpExchTbl)
		m_lpExchTbl->AddRef();

	if (fFreeBusyVisible)
		m_ulTableFlags = ACLTABLE_FREEBUSY;
	else
		m_ulTableFlags = 0;

	m_bIsAB = false;

	CreateDialogAndMenu(IDR_MENU_ACL);

	OnRefreshView();
} // CAclDlg::CAclDlg

CAclDlg::~CAclDlg()
{
	TRACE_DESTRUCTOR(CLASS);

	if (m_lpExchTbl)
		m_lpExchTbl->Release();
} // CAclDlg::~CAclDlg

BEGIN_MESSAGE_MAP(CAclDlg, CContentsTableDlg)
	ON_COMMAND(ID_ADDITEM, OnAddItem)
	ON_COMMAND(ID_DELETESELECTEDITEM, OnDeleteSelectedItem)
	ON_COMMAND(ID_MODIFYSELECTEDITEM, OnModifySelectedItem)
END_MESSAGE_MAP()


_Check_return_ HRESULT CAclDlg::OpenItemProp(int /*iSelectedItem*/, __mfcmapiModifyEnum /*bModify*/, _Deref_out_opt_ LPMAPIPROP* lppMAPIProp)
{
	if (lppMAPIProp) *lppMAPIProp = NULL;
	// Don't do anything because we don't want to override the properties that we have
	return S_OK;
} // CAclDlg::OpenItemProp

void CAclDlg::OnInitMenu(_In_ CMenu* pMenu)
{
	if (pMenu)
	{
		if (m_lpContentsTableListCtrl)
		{
			int iNumSel = m_lpContentsTableListCtrl->GetSelectedCount();
			pMenu->EnableMenuItem(ID_DELETESELECTEDITEM,DIMMSOK(iNumSel));
			pMenu->EnableMenuItem(ID_MODIFYSELECTEDITEM,DIMMSOK(iNumSel));
		}
	}
	CContentsTableDlg::OnInitMenu(pMenu);
} // CAclDlg::OnInitMenu

// Clear the current list and get a new one with whatever code we've got in LoadMAPIPropList
void CAclDlg::OnRefreshView()
{
	HRESULT hRes = S_OK;

	if (!m_lpExchTbl || !m_lpContentsTableListCtrl)
		return;

	if (m_lpContentsTableListCtrl->IsLoading())
		m_lpContentsTableListCtrl->OnCancelTableLoad();
	DebugPrintEx(DBGGeneric,CLASS,_T("OnRefreshView"),_T("\n"));

	if (m_lpExchTbl)
	{
		LPMAPITABLE lpMAPITable = NULL;
		// Open a MAPI table on the Exchange table property. This table can be
		// read to determine what the Exchange table looks like.
		EC_MAPI(m_lpExchTbl->GetTable(m_ulTableFlags, &lpMAPITable));

		if (lpMAPITable)
		{
			EC_H(m_lpContentsTableListCtrl->SetContentsTable(
				lpMAPITable,
				dfNormal,
				NULL));

			lpMAPITable->Release();
		}
	}
} // CAclDlg::OnRefreshView

void CAclDlg::OnAddItem()
{
	HRESULT			hRes = S_OK;

	CEditor MyData(
		this,
		IDS_ACLADDITEM,
		IDS_ACLADDITEMPROMPT,
		2,
		CEDITOR_BUTTON_OK|CEDITOR_BUTTON_CANCEL);
	MyData.SetPromptPostFix(AllFlagsToString(PROP_ID(PR_MEMBER_RIGHTS),true));
	MyData.InitPane(0, CreateSingleLinePane(IDS_USEREID, NULL, false));
	MyData.InitPane(1, CreateSingleLinePane(IDS_MASKINHEX, NULL, false));
	MyData.SetHex(1,0);

	WC_H(MyData.DisplayDialog());
	if (S_OK != hRes)
	{
		DebugPrint(DBGGeneric,_T("OnAddItem cancelled.\n"));
		return;
	}

	LPROWLIST lpNewItem = NULL;

	EC_H(MAPIAllocateBuffer(CbNewROWLIST(1),(LPVOID*) &lpNewItem));

	if (lpNewItem)
	{
		lpNewItem->cEntries = 1;
		lpNewItem->aEntries[0].ulRowFlags = ROW_ADD;
		lpNewItem->aEntries[0].cValues = 2;
		lpNewItem->aEntries[0].rgPropVals = 0;

		EC_H(MAPIAllocateMore(2 * sizeof(SPropValue), lpNewItem, (LPVOID*)&lpNewItem->aEntries[0].rgPropVals));

		if (lpNewItem->aEntries[0].rgPropVals)
		{
			LPENTRYID lpEntryID = NULL;
			size_t cbBin = 0;
			EC_H(MyData.GetEntryID(0, false, &cbBin, &lpEntryID));

			lpNewItem->aEntries[0].rgPropVals[0].ulPropTag = PR_MEMBER_ENTRYID;
			lpNewItem->aEntries[0].rgPropVals[0].Value.bin.cb = (ULONG)cbBin;
			lpNewItem->aEntries[0].rgPropVals[0].Value.bin.lpb = (LPBYTE)lpEntryID;
			lpNewItem->aEntries[0].rgPropVals[1].ulPropTag = PR_MEMBER_RIGHTS;
			lpNewItem->aEntries[0].rgPropVals[1].Value.ul = MyData.GetHex(1);

			EC_MAPI(m_lpExchTbl->ModifyTable(
				m_ulTableFlags,
				lpNewItem));
			MAPIFreeBuffer(lpNewItem);
			if (S_OK == hRes)
				OnRefreshView();

			delete[] lpEntryID;
		}
	}
} // CAclDlg::OnAddItem

void CAclDlg::OnDeleteSelectedItem()
{
	HRESULT		hRes = S_OK;
	CWaitCursor	Wait; // Change the mouse to an hourglass while we work.

	LPROWLIST lpSelectedItems = NULL;

	EC_H(GetSelectedItems(ACL_INCLUDE_ID, ROW_REMOVE, &lpSelectedItems));

	if (lpSelectedItems)
	{
		EC_MAPI(m_lpExchTbl->ModifyTable(
			m_ulTableFlags,
			lpSelectedItems));
		MAPIFreeBuffer(lpSelectedItems);
		if (S_OK == hRes)
			OnRefreshView();
	}
} // CAclDlg::OnDeleteSelectedItem


void CAclDlg::OnModifySelectedItem()
{
	HRESULT		hRes = S_OK;
	CWaitCursor	Wait; // Change the mouse to an hourglass while we work.

	LPROWLIST lpSelectedItems = NULL;

	EC_H(GetSelectedItems(ACL_INCLUDE_ID | ACL_INCLUDE_OTHER, ROW_MODIFY, &lpSelectedItems));

	if (lpSelectedItems)
	{
		EC_MAPI(m_lpExchTbl->ModifyTable(
			m_ulTableFlags,
			lpSelectedItems));
		MAPIFreeBuffer(lpSelectedItems);
		if (S_OK == hRes) OnRefreshView();
	}
} // CAclDlg::OnModifySelectedItem

_Check_return_ HRESULT CAclDlg::GetSelectedItems(ULONG ulFlags, ULONG ulRowFlags, _In_ LPROWLIST* lppRowList)
{
	if (!lppRowList || !m_lpContentsTableListCtrl)
		return MAPI_E_INVALID_PARAMETER;

	*lppRowList = NULL;
	HRESULT hRes = S_OK;
	int iNumItems = m_lpContentsTableListCtrl->GetSelectedCount();

	if (!iNumItems) return S_OK;
	if (iNumItems > MAXNewROWLIST) return MAPI_E_INVALID_PARAMETER;

	LPROWLIST lpTempList = NULL;

	EC_H(MAPIAllocateBuffer(CbNewROWLIST(iNumItems),(LPVOID*) &lpTempList));

	if (lpTempList)
	{
		lpTempList->cEntries = iNumItems;
		int iArrayPos = 0;
		int iSelectedItem = -1;

		for (iArrayPos = 0 ; iArrayPos < iNumItems ; iArrayPos++)
		{
			lpTempList->aEntries[iArrayPos].ulRowFlags = ulRowFlags;
			lpTempList->aEntries[iArrayPos].cValues = 0;
			lpTempList->aEntries[iArrayPos].rgPropVals = 0;
			iSelectedItem = m_lpContentsTableListCtrl->GetNextItem(
				iSelectedItem,
				LVNI_SELECTED);
			if (-1 != iSelectedItem)
			{
				SortListData* lpData = (SortListData*) m_lpContentsTableListCtrl->GetItemData(iSelectedItem);
				if (lpData)
				{
					if (ulFlags & ACL_INCLUDE_ID && ulFlags & ACL_INCLUDE_OTHER)
					{
						LPSPropValue lpSPropValue = NULL;
						EC_H(MAPIAllocateMore(2 * sizeof(SPropValue), lpTempList, (LPVOID*)&lpTempList->aEntries[iArrayPos].rgPropVals));

						lpTempList->aEntries[iArrayPos].cValues = 2;

						lpSPropValue = PpropFindProp(
							lpData->lpSourceProps,
							lpData->cSourceProps,
							PR_MEMBER_ID);

						lpTempList->aEntries[iArrayPos].rgPropVals[0].ulPropTag = lpSPropValue->ulPropTag;
						lpTempList->aEntries[iArrayPos].rgPropVals[0].Value = lpSPropValue->Value;

						lpSPropValue = PpropFindProp(
							lpData->lpSourceProps,
							lpData->cSourceProps,
							PR_MEMBER_RIGHTS);

						lpTempList->aEntries[iArrayPos].rgPropVals[1].ulPropTag = lpSPropValue->ulPropTag;
						lpTempList->aEntries[iArrayPos].rgPropVals[1].Value = lpSPropValue->Value;
					}
					else if (ulFlags & ACL_INCLUDE_ID)
					{
						lpTempList->aEntries[iArrayPos].cValues = 1;
						lpTempList->aEntries[iArrayPos].rgPropVals = PpropFindProp(
							lpData->lpSourceProps,
							lpData->cSourceProps,
							PR_MEMBER_ID);
					}
				}
			}
		}
	}

	*lppRowList = lpTempList;
	return hRes;
} // CAclDlg::GetSelectedItems

void CAclDlg::HandleAddInMenuSingle(
									_In_ LPADDINMENUPARAMS lpParams,
									_In_ LPMAPIPROP /*lpMAPIProp*/,
									_In_ LPMAPICONTAINER /*lpContainer*/)
{
	if (lpParams)
	{
		lpParams->lpExchTbl = m_lpExchTbl;
	}

	InvokeAddInMenu(lpParams);
} // CAclDlg::HandleAddInMenuSingle
