// PropertyEditor.cpp : implementation file
//

#include "stdafx.h"
#include "PropertyEditor.h"
#include "InterpretProp2.h"
#include "MAPIFunctions.h"
#include "SmartView.h"

_Check_return_ HRESULT DisplayPropertyEditor(_In_ CWnd* pParentWnd,
	UINT uidTitle,
	UINT uidPrompt,
	bool bIsAB,
	_In_opt_ LPVOID lpAllocParent,
	_In_opt_ LPMAPIPROP lpMAPIProp,
	ULONG ulPropTag,
	bool bMVRow,
	_In_opt_ LPSPropValue lpsPropValue,
	_Inout_opt_ LPSPropValue* lpNewValue)
{
	HRESULT hRes = S_OK;
	bool bShouldFreeInputValue = false;

	// We got a MAPI prop object and no input value, go look one up
	if (lpMAPIProp && !lpsPropValue)
	{
		SPropTagArray sTag = { 0 };
		sTag.cValues = 1;
		sTag.aulPropTag[0] = (PT_ERROR == PROP_TYPE(ulPropTag)) ? CHANGE_PROP_TYPE(ulPropTag, PT_UNSPECIFIED) : ulPropTag;
		ULONG ulValues = NULL;

		WC_MAPI(lpMAPIProp->GetProps(&sTag, NULL, &ulValues, &lpsPropValue));

		// Suppress MAPI_E_NOT_FOUND error when the source type is non error
		if (lpsPropValue &&
			PT_ERROR == PROP_TYPE(lpsPropValue->ulPropTag) &&
			MAPI_E_NOT_FOUND == lpsPropValue->Value.err &&
			PT_ERROR != PROP_TYPE(ulPropTag)
			)
		{
			MAPIFreeBuffer(lpsPropValue);
			lpsPropValue = NULL;
		}

		if (MAPI_E_CALL_FAILED == hRes)
		{
			// Just suppress this - let the user edit anyway
			hRes = S_OK;
		}

		// In all cases where we got a value back, we need to reset our property tag to the value we got
		// This will address when the source is PT_UNSPECIFIED, when the returned value is PT_ERROR,
		// or any other case where the returned value has a different type than requested
		if (SUCCEEDED(hRes) && lpsPropValue)
			ulPropTag = lpsPropValue->ulPropTag;

		bShouldFreeInputValue = true;
	}
	else if (lpsPropValue && !ulPropTag)
	{
		ulPropTag = lpsPropValue->ulPropTag;
	}

	// Check for the multivalue prop case
	if (PROP_TYPE(ulPropTag) & MV_FLAG)
	{
		CMultiValuePropertyEditor MyPropertyEditor(
			pParentWnd,
			uidTitle,
			uidPrompt,
			bIsAB,
			lpAllocParent,
			lpMAPIProp,
			ulPropTag,
			lpsPropValue);
		WC_H(MyPropertyEditor.DisplayDialog());

		if (lpNewValue) *lpNewValue = MyPropertyEditor.DetachModifiedSPropValue();
	}
	// Or the single value prop case
	else
	{
		CPropertyEditor MyPropertyEditor(
			pParentWnd,
			uidTitle,
			uidPrompt,
			bIsAB,
			bMVRow,
			lpAllocParent,
			lpMAPIProp,
			ulPropTag,
			lpsPropValue);
		WC_H(MyPropertyEditor.DisplayDialog());

		if (lpNewValue) *lpNewValue = MyPropertyEditor.DetachModifiedSPropValue();
	}

	if (bShouldFreeInputValue)
		MAPIFreeBuffer(lpsPropValue);

	return hRes;
} // DisplayPropertyEditor

static TCHAR* SVCLASS = _T("CPropertyEditor"); // STRING_OK

// Create an editor for a MAPI property
CPropertyEditor::CPropertyEditor(
	_In_ CWnd* pParentWnd,
	UINT uidTitle,
	UINT uidPrompt,
	bool bIsAB,
	bool bMVRow,
	_In_opt_ LPVOID lpAllocParent,
	_In_opt_ LPMAPIPROP lpMAPIProp,
	ULONG ulPropTag,
	_In_opt_ LPSPropValue lpsPropValue) :
	CEditor(pParentWnd, uidTitle, uidPrompt, 0, CEDITOR_BUTTON_OK | CEDITOR_BUTTON_CANCEL)
{
	TRACE_CONSTRUCTOR(SVCLASS);

	m_bIsAB = bIsAB;
	m_bMVRow = bMVRow;
	m_lpAllocParent = lpAllocParent;
	m_lpsOutputValue = NULL;
	m_bDirty = false;

	m_lpMAPIProp = lpMAPIProp;
	if (m_lpMAPIProp) m_lpMAPIProp->AddRef();
	m_ulPropTag = ulPropTag;
	m_lpsInputValue = lpsPropValue;
	m_lpSmartView = NULL;

	// If we didn't have an input value, we are creating a new property
	// So by definition, we're already dirty
	if (!m_lpsInputValue) m_bDirty = true;

	CString szPromptPostFix;
	szPromptPostFix.Format(_T("%s%s"), uidPrompt ? _T("\r\n") : _T(""), (LPCTSTR)TagToString(m_ulPropTag | (m_bMVRow ? MV_FLAG : NULL), m_lpMAPIProp, m_bIsAB, false)); // STRING_OK

	SetPromptPostFix(szPromptPostFix);

	// Let's crack our property open and see what kind of controls we'll need for it
	CreatePropertyControls();

	InitPropertyControls();
} // CPropertyEditor::CPropertyEditor

CPropertyEditor::~CPropertyEditor()
{
	TRACE_DESTRUCTOR(SVCLASS);
	if (m_lpMAPIProp) m_lpMAPIProp->Release();
} // CPropertyEditor::~CPropertyEditor

BOOL CPropertyEditor::OnInitDialog()
{
	BOOL bRet = CEditor::OnInitDialog();
	return bRet;
} // CPropertyEditor::OnInitDialog

void CPropertyEditor::OnOK()
{
	// This is where we write our changes back
	WriteStringsToSPropValue();

	// Write the property to the object if we're not editing a row of a MV property
	if (!m_bMVRow) WriteSPropValueToObject();
	CMyDialog::OnOK(); // don't need to call CEditor::OnOK
} // CPropertyEditor::OnOK

void CPropertyEditor::CreatePropertyControls()
{
	switch (PROP_TYPE(m_ulPropTag))
	{
	case PT_APPTIME:
	case PT_BOOLEAN:
	case PT_DOUBLE:
	case PT_OBJECT:
	case PT_R4:
		CreateControls(1);
		break;
	case PT_ERROR:
		CreateControls(2);
		break;
	case PT_I8:
	case PT_BINARY:
		CreateControls(3);
		break;
	case PT_CURRENCY:
	case PT_LONG:
	case PT_I2:
	case PT_SYSTIME:
		CreateControls(3);
		break;
	case PT_STRING8:
	case PT_UNICODE:
		CreateControls(2);
		break;
	case PT_CLSID:
		CreateControls(1);
		break;
	case PT_ACTIONS:
		CreateControls(1);
		break;
	case PT_SRESTRICTION:
		CreateControls(1);
		break;
	default:
		CreateControls(2);
		break;
	}
} // CPropertyEditor::CreatePropertyControls

void CPropertyEditor::InitPropertyControls()
{
	switch (PROP_TYPE(m_ulPropTag))
	{
	case PT_I8:
	case PT_I2:
	case PT_BINARY:
	case PT_LONG:
		// This will be freed by the pane that we pass it to.
		m_lpSmartView = (SmartViewPane*)CreateSmartViewPane(IDS_SMARTVIEW);
	}

	LPWSTR szSmartView = NULL;

	int iStructType = InterpretPropSmartView(m_lpsInputValue,
		m_lpMAPIProp,
		NULL,
		NULL,
		m_bIsAB,
		m_bMVRow,
		&szSmartView); // Built from lpProp & lpMAPIProp

	CString szTemp1;
	CString szTemp2;
	CountedTextPane* lpPane = NULL;
	size_t cbStr = 0;
	LPTSTR szGuid = NULL;

	switch (PROP_TYPE(m_ulPropTag))
	{
	case PT_APPTIME:
		InitPane(0, CreateSingleLinePane(IDS_DOUBLE, NULL, false));
		if (m_lpsInputValue)
		{
			SetStringf(0, _T("%f"), m_lpsInputValue->Value.at); // STRING_OK
		}
		else
		{
			SetDecimal(0, 0);
		}

		break;
	case PT_BOOLEAN:
		InitPane(0, CreateCheckPane(IDS_BOOLEAN, m_lpsInputValue ? (0 != m_lpsInputValue->Value.b) : false, false));
		break;
	case PT_DOUBLE:
		InitPane(0, CreateSingleLinePane(IDS_DOUBLE, NULL, false));
		if (m_lpsInputValue)
		{
			SetStringf(0, _T("%f"), m_lpsInputValue->Value.dbl); // STRING_OK
		}
		else
		{
			SetDecimal(0, 0);
		}

		break;
	case PT_OBJECT:
		InitPane(0, CreateSingleLinePaneID(IDS_OBJECT, IDS_OBJECTVALUE, true));
		break;
	case PT_R4:
		InitPane(0, CreateSingleLinePane(IDS_FLOAT, NULL, false));
		if (m_lpsInputValue)
		{
			SetStringf(0, _T("%f"), m_lpsInputValue->Value.flt); // STRING_OK
		}
		else
		{
			SetDecimal(0, 0);
		}

		break;
	case PT_STRING8:
		InitPane(0, CreateCountedTextPane(IDS_ANSISTRING, false, IDS_CCH));
		InitPane(1, CreateCountedTextPane(IDS_BIN, false, IDS_CB));
		if (m_lpsInputValue && CheckStringProp(m_lpsInputValue, PT_STRING8))
		{
			SetStringA(0, m_lpsInputValue->Value.lpszA);

			lpPane = (CountedTextPane*)GetControl(1);
			if (lpPane)
			{
				HRESULT hRes = S_OK;
				EC_H(StringCbLengthA(m_lpsInputValue->Value.lpszA, STRSAFE_MAX_CCH * sizeof(char), &cbStr));

				lpPane->SetCount(cbStr);
				lpPane->SetBinary(
					(LPBYTE)m_lpsInputValue->Value.lpszA,
					cbStr);
			}

			lpPane = (CountedTextPane*)GetControl(0);
			if (lpPane) lpPane->SetCount(cbStr);
		}

		break;
	case PT_UNICODE:
		InitPane(0, CreateCountedTextPane(IDS_UNISTRING, false, IDS_CCH));
		InitPane(1, CreateCountedTextPane(IDS_BIN, false, IDS_CB));
		if (m_lpsInputValue && CheckStringProp(m_lpsInputValue, PT_UNICODE))
		{
			SetStringW(0, m_lpsInputValue->Value.lpszW);

			lpPane = (CountedTextPane*)GetControl(1);
			if (lpPane)
			{
				HRESULT hRes = S_OK;
				EC_H(StringCbLengthW(m_lpsInputValue->Value.lpszW, STRSAFE_MAX_CCH * sizeof(WCHAR), &cbStr));

				lpPane->SetCount(cbStr);
				lpPane->SetBinary(
					(LPBYTE)m_lpsInputValue->Value.lpszW,
					cbStr);
			}

			lpPane = (CountedTextPane*)GetControl(0);
			if (lpPane) lpPane->SetCount((cbStr % sizeof(WCHAR)) ? 0 : cbStr / sizeof(WCHAR));
		}

		break;
	case PT_CURRENCY:
		InitPane(0, CreateSingleLinePane(IDS_HI, NULL, false));
		InitPane(1, CreateSingleLinePane(IDS_LO, NULL, false));
		InitPane(2, CreateSingleLinePane(IDS_CURRENCY, NULL, false));
		if (m_lpsInputValue)
		{
			SetHex(0, m_lpsInputValue->Value.cur.Hi);
			SetHex(1, m_lpsInputValue->Value.cur.Lo);
			SetString(2, CurrencyToString(m_lpsInputValue->Value.cur));
		}
		else
		{
			SetHex(0, 0);
			SetHex(1, 0);
			SetString(2, _T("0.0000")); // STRING_OK
		}

		break;
	case PT_ERROR:
		InitPane(0, CreateSingleLinePane(IDS_ERRORCODEHEX, NULL, true));
		InitPane(1, CreateSingleLinePane(IDS_ERRORNAME, NULL, true));
		if (m_lpsInputValue)
		{
			SetHex(0, m_lpsInputValue->Value.err);
			SetStringW(1, ErrorNameFromErrorCode(m_lpsInputValue->Value.err));
		}

		break;
	case PT_I2:
		InitPane(0, CreateSingleLinePane(IDS_SIGNEDDECIMAL, NULL, false));
		InitPane(1, CreateSingleLinePane(IDS_HEX, NULL, false));
		InitPane(2, m_lpSmartView);
		if (m_lpsInputValue)
		{
			SetDecimal(0, m_lpsInputValue->Value.i);
			SetHex(1, m_lpsInputValue->Value.i);
		}
		else
		{
			SetDecimal(0, 0);
			SetHex(1, 0);
		}

		if (m_lpSmartView)
		{
			m_lpSmartView->DisableDropDown();
			m_lpSmartView->SetStringW(szSmartView);
		}

		break;
	case PT_I8:
		InitPane(0, CreateSingleLinePane(IDS_HIGHPART, NULL, false));
		InitPane(1, CreateSingleLinePane(IDS_LOWPART, NULL, false));
		InitPane(2, CreateSingleLinePane(IDS_DECIMAL, NULL, false));
		InitPane(3, m_lpSmartView);

		if (m_lpsInputValue)
		{
			SetHex(0, (int)m_lpsInputValue->Value.li.HighPart);
			SetHex(1, (int)m_lpsInputValue->Value.li.LowPart);
			SetStringf(2, _T("%I64d"), m_lpsInputValue->Value.li.QuadPart); // STRING_OK
		}
		else
		{
			SetHex(0, 0);
			SetHex(1, 0);
			SetDecimal(2, 0);
		}

		if (m_lpSmartView)
		{
			m_lpSmartView->DisableDropDown();
			m_lpSmartView->SetStringW(szSmartView);
		}

		break;
	case PT_BINARY:
		lpPane = (CountedTextPane*)CreateCountedTextPane(IDS_BIN, false, IDS_CB);
		InitPane(0, lpPane);
		InitPane(1, CreateCountedTextPane(IDS_TEXT, false, IDS_CCH));
		InitPane(2, m_lpSmartView);

		if (m_lpsInputValue)
		{
			if (lpPane)
			{
				lpPane->SetCount(m_lpsInputValue->Value.bin.cb);
				lpPane->SetString(BinToHexString(&m_lpsInputValue->Value.bin, false));
				SetStringA(1, (LPCSTR)m_lpsInputValue->Value.bin.lpb, m_lpsInputValue->Value.bin.cb + 1);
			}

			lpPane = (CountedTextPane*)GetControl(1);
			if (lpPane) lpPane->SetCount(m_lpsInputValue->Value.bin.cb);
		}

		if (m_lpSmartView)
		{
			m_lpSmartView->SetParser(iStructType);
			m_lpSmartView->SetStringW(szSmartView);
		}

		break;
	case PT_LONG:
		InitPane(0, CreateSingleLinePane(IDS_UNSIGNEDDECIMAL, NULL, false));
		InitPane(1, CreateSingleLinePane(IDS_HEX, NULL, false));
		InitPane(2, m_lpSmartView);
		if (m_lpsInputValue)
		{
			SetStringf(0, _T("%d"), m_lpsInputValue->Value.l); // STRING_OK
			SetHex(1, m_lpsInputValue->Value.l);
		}
		else
		{
			SetDecimal(0, 0);
			SetHex(1, 0);
			SetHex(2, 0);
		}

		if (m_lpSmartView)
		{
			m_lpSmartView->DisableDropDown();
			m_lpSmartView->SetStringW(szSmartView);
		}

		break;
	case PT_SYSTIME:
		InitPane(0, CreateSingleLinePane(IDS_LOWDATETIME, NULL, false));
		InitPane(1, CreateSingleLinePane(IDS_HIGHDATETIME, NULL, false));
		InitPane(2, CreateSingleLinePane(IDS_DATE, NULL, true));
		if (m_lpsInputValue)
		{
			SetHex(0, (int)m_lpsInputValue->Value.ft.dwLowDateTime);
			SetHex(1, (int)m_lpsInputValue->Value.ft.dwHighDateTime);
			FileTimeToString(&m_lpsInputValue->Value.ft, &szTemp1, NULL);
			SetString(2, szTemp1);
		}
		else
		{
			SetHex(0, 0);
			SetHex(1, 0);
		}

		break;
	case PT_CLSID:
		InitPane(0, CreateSingleLinePane(IDS_GUID, NULL, false));
		if (m_lpsInputValue)
		{
			szGuid = GUIDToStringAndName(m_lpsInputValue->Value.lpguid);
		}
		else
		{
			szGuid = GUIDToStringAndName(0);
		}

		SetString(0, szGuid);
		delete[] szGuid;
		break;
	case PT_SRESTRICTION:
		InitPane(0, CreateCollapsibleTextPane(IDS_RESTRICTION, true));
		InterpretProp(m_lpsInputValue, &szTemp1, NULL);
		SetString(0, szTemp1);
		break;
	case PT_ACTIONS:
		InitPane(0, CreateCollapsibleTextPane(IDS_ACTIONS, true));
		InterpretProp(m_lpsInputValue, &szTemp1, NULL);
		SetString(0, szTemp1);
		break;
	default:
		InterpretProp(m_lpsInputValue, &szTemp1, &szTemp2);
		InitPane(0, CreateCollapsibleTextPane(IDS_VALUE, true));
		InitPane(1, CreateCollapsibleTextPane(IDS_ALTERNATEVIEW, true));
		SetString(IDS_VALUE, szTemp1);
		SetString(IDS_ALTERNATEVIEW, szTemp2);
		break;
	}

	delete[] szSmartView;
} // CPropertyEditor::InitPropertyControls

void CPropertyEditor::WriteStringsToSPropValue()
{
	HRESULT hRes = S_OK;
	CString szTmpString;
	ULONG cbBin = NULL;

	// Check first if we'll have anything to write
	switch (PROP_TYPE(m_ulPropTag))
	{
	case PT_OBJECT: // Nothing to write back - not supported
	case PT_SRESTRICTION:
	case PT_ACTIONS:
		return;
	case PT_BINARY:
		// Check that we've got valid hex string before we allocate anything. Note that we're
		// reading szTmpString now and will assume it's read when we get to the real PT_BINARY case
		szTmpString = GetStringUseControl(0);
		if (!MyBinFromHex(
			(LPCTSTR)szTmpString,
			NULL,
			&cbBin)) return;
		break;
	case PT_STRING8:
	case PT_UNICODE:
		// Check that we've got valid hex string before we allocate anything. Note that we're
		// reading szTmpString now and will assume it's read when we get to the real PT_STRING8/PT_UNICODE cases
		szTmpString = GetStringUseControl(1);
		if (!MyBinFromHex(
			(LPCTSTR)szTmpString,
			NULL,
			&cbBin)) return;
		if (PROP_TYPE(m_ulPropTag) == PT_UNICODE && cbBin & 1) return;
		break;
	default: break;
	}

	// If nothing has changed, we're done.
	if (!m_bDirty) return;

	if (!m_lpsOutputValue)
	{
		if (m_lpAllocParent)
		{
			EC_H(MAPIAllocateMore(
				sizeof(SPropValue),
				m_lpAllocParent,
				(LPVOID*)&m_lpsOutputValue));
		}
		else
		{
			EC_H(MAPIAllocateBuffer(
				sizeof(SPropValue),
				(LPVOID*)&m_lpsOutputValue));
			m_lpAllocParent = m_lpsOutputValue;
		}
	}

	if (m_lpsOutputValue)
	{
		bool bFailed = false; // set true if we fail to get a prop and have to clean up memory
		m_lpsOutputValue->ulPropTag = m_ulPropTag;
		m_lpsOutputValue->dwAlignPad = NULL;
		short int iVal = 0;
		LONG lVal = 0;
		float fVal = 0;
		double dVal = 0;
		CURRENCY curVal = { 0 };
		double atVal = 0;
		SCODE errVal = 0;
		bool bVal = false;
		LARGE_INTEGER liVal = { 0 };
		FILETIME ftVal = { 0 };

		switch (PROP_TYPE(m_ulPropTag))
		{
		case PT_I2: // treat as signed long
			szTmpString = GetStringUseControl(0);
			iVal = (short int)_tcstol(szTmpString, NULL, 10);
			m_lpsOutputValue->Value.i = iVal;
			break;
		case PT_LONG: // treat as unsigned long
			szTmpString = GetStringUseControl(0);
			lVal = (LONG)_tcstoul(szTmpString, NULL, 10);
			m_lpsOutputValue->Value.l = lVal;
			break;
		case PT_R4:
			szTmpString = GetStringUseControl(0);
			fVal = (float)_tcstod(szTmpString, NULL);
			m_lpsOutputValue->Value.flt = fVal;
			break;
		case PT_DOUBLE:
			szTmpString = GetStringUseControl(0);
			dVal = (double)_tcstod(szTmpString, NULL);
			m_lpsOutputValue->Value.dbl = dVal;
			break;
		case PT_CURRENCY:
			szTmpString = GetStringUseControl(0);
			curVal.Hi = _tcstoul(szTmpString, NULL, 16);
			szTmpString = GetStringUseControl(1);
			curVal.Lo = _tcstoul(szTmpString, NULL, 16);
			m_lpsOutputValue->Value.cur = curVal;
			break;
		case PT_APPTIME:
			szTmpString = GetStringUseControl(0);
			atVal = (double)_tcstod(szTmpString, NULL);
			m_lpsOutputValue->Value.at = atVal;
			break;
		case PT_ERROR: // unsigned
			szTmpString = GetStringUseControl(0);
			errVal = (SCODE)_tcstoul(szTmpString, NULL, 16);
			m_lpsOutputValue->Value.err = errVal;
			break;
		case PT_BOOLEAN:
			bVal = GetCheckUseControl(0);
			m_lpsOutputValue->Value.b = (unsigned short)bVal;
			break;
		case PT_I8:
			szTmpString = GetStringUseControl(0);
			liVal.HighPart = (long)_tcstoul(szTmpString, NULL, 16);
			szTmpString = GetStringUseControl(1);
			liVal.LowPart = (long)_tcstoul(szTmpString, NULL, 16);
			m_lpsOutputValue->Value.li = liVal;
			break;
		case PT_STRING8:
			// We read strings out of the hex control in order to preserve any hex level tweaks the user
			// may have done. The RichEdit control likes throwing them away.
			m_lpsOutputValue->Value.lpszA = NULL;

			EC_H(MAPIAllocateMore(
				cbBin + sizeof(char), // NULL terminator
				m_lpAllocParent,
				(LPVOID*)&m_lpsOutputValue->Value.lpszA));
			if (FAILED(hRes)) bFailed = true;
			else
			{
				EC_B(MyBinFromHex(
					(LPCTSTR)szTmpString,
					(LPBYTE)m_lpsOutputValue->Value.lpszA,
					&cbBin));
				m_lpsOutputValue->Value.lpszA[cbBin] = NULL;
			}

			break;
		case PT_UNICODE:
			// We read strings out of the hex control in order to preserve any hex level tweaks the user
			// may have done. The RichEdit control likes throwing them away.
			m_lpsOutputValue->Value.lpszW = NULL;

			EC_H(MAPIAllocateMore(
				cbBin + sizeof(WCHAR), // NULL terminator
				m_lpAllocParent,
				(LPVOID*)&m_lpsOutputValue->Value.lpszW));
			if (FAILED(hRes)) bFailed = true;
			else
			{
				EC_B(MyBinFromHex(
					(LPCTSTR)szTmpString,
					(LPBYTE)m_lpsOutputValue->Value.lpszW,
					&cbBin));
				m_lpsOutputValue->Value.lpszW[cbBin / sizeof(WCHAR)] = NULL;
			}

			break;
		case PT_SYSTIME:
			szTmpString = GetStringUseControl(0);
			ftVal.dwLowDateTime = _tcstoul(szTmpString, NULL, 16);
			szTmpString = GetStringUseControl(1);
			ftVal.dwHighDateTime = _tcstoul(szTmpString, NULL, 16);
			m_lpsOutputValue->Value.ft = ftVal;
			break;
		case PT_CLSID:
			EC_H(MAPIAllocateMore(
				sizeof(GUID),
				m_lpAllocParent,
				(LPVOID*)&m_lpsOutputValue->Value.lpguid));
			if (m_lpsOutputValue->Value.lpguid)
			{
				szTmpString = GetStringUseControl(0);
				EC_H(StringToGUID((LPCTSTR)szTmpString, m_lpsOutputValue->Value.lpguid));
				if (FAILED(hRes)) bFailed = true;
			}

			break;
		case PT_BINARY:
			// remember we already read szTmpString and ulStrLen and found ulStrLen was even
			m_lpsOutputValue->Value.bin.cb = cbBin;
			if (0 == m_lpsOutputValue->Value.bin.cb)
			{
				m_lpsOutputValue->Value.bin.lpb = 0;
			}
			else
			{
				EC_H(MAPIAllocateMore(
					m_lpsOutputValue->Value.bin.cb,
					m_lpAllocParent,
					(LPVOID*)&m_lpsOutputValue->Value.bin.lpb));
				if (FAILED(hRes)) bFailed = true;
				else
				{
					EC_B(MyBinFromHex(
						(LPCTSTR)szTmpString,
						m_lpsOutputValue->Value.bin.lpb,
						&m_lpsOutputValue->Value.bin.cb));
				}
			}

			break;
		default:
			// We shouldn't ever get here unless some new prop type shows up
			bFailed = true;
			break;
		}

		if (bFailed)
		{
			// If we don't have a parent or we are the parent, then we can free here
			if (!m_lpAllocParent || m_lpAllocParent == m_lpsOutputValue)
			{
				MAPIFreeBuffer(m_lpsOutputValue);
				m_lpsOutputValue = NULL;
				m_lpAllocParent = NULL;
			}
			else
			{
				// If m_lpsOutputValue was allocated off a parent, we can't free it here
				// Just drop the reference and m_lpAllocParent's free will clean it up
				m_lpsOutputValue = NULL;
			}
		}
	}
} // CPropertyEditor::WriteStringsToSPropValue

void CPropertyEditor::WriteSPropValueToObject()
{
	if (!m_lpsOutputValue || !m_lpMAPIProp) return;

	HRESULT hRes = S_OK;

	LPSPropProblemArray lpProblemArray = NULL;

	EC_MAPI(m_lpMAPIProp->SetProps(
		1,
		m_lpsOutputValue,
		&lpProblemArray));

	EC_PROBLEMARRAY(lpProblemArray);
	MAPIFreeBuffer(lpProblemArray);

	EC_MAPI(m_lpMAPIProp->SaveChanges(KEEP_OPEN_READWRITE));
} // CPropertyEditor::WriteSPropValueToObject

// Callers beware: Detatches and returns the modified prop value - this must be MAPIFreeBuffered!
_Check_return_ LPSPropValue CPropertyEditor::DetachModifiedSPropValue()
{
	LPSPropValue m_lpRet = m_lpsOutputValue;
	m_lpsOutputValue = NULL;
	return m_lpRet;
} // CPropertyEditor::DetachModifiedSPropValue

_Check_return_ ULONG CPropertyEditor::HandleChange(UINT nID)
{
	ULONG i = CEditor::HandleChange(nID);

	if ((ULONG)-1 == i) return (ULONG)-1;

	CString szTmpString;
	LPWSTR szSmartView = NULL;
	SPropValue sProp = { 0 };

	short int iVal = 0;
	LONG lVal = 0;
	CURRENCY curVal = { 0 };
	LARGE_INTEGER liVal = { 0 };
	FILETIME ftVal = { 0 };
	LPBYTE lpb = NULL;
	SBinary Bin = { 0 };

	CountedTextPane* lpPane = NULL;

	// If we get here, something changed - set the dirty flag
	m_bDirty = true;

	switch (PROP_TYPE(m_ulPropTag))
	{
	case PT_I2: // signed 16 bit
		szTmpString = GetStringUseControl(i);
		if (0 == i)
		{
			iVal = (short int)_tcstol(szTmpString, NULL, 10);
			SetHex(1, iVal);
		}
		else if (1 == i)
		{
			iVal = (short int)_tcstol(szTmpString, NULL, 16);
			SetDecimal(0, iVal);
		}

		sProp.ulPropTag = m_ulPropTag;
		sProp.Value.i = iVal;

		InterpretPropSmartView(&sProp,
			m_lpMAPIProp,
			NULL,
			NULL,
			m_bIsAB,
			m_bMVRow,
			&szSmartView);

		if (m_lpSmartView) m_lpSmartView->SetStringW(szSmartView);
		delete[] szSmartView;
		szSmartView = NULL;
		break;
	case PT_LONG: // unsigned 32 bit
		szTmpString = GetStringUseControl(i);
		if (0 == i)
		{
			lVal = (LONG)_tcstoul(szTmpString, NULL, 10);
			SetHex(1, lVal);
		}
		else if (1 == i)
		{
			lVal = (LONG)_tcstoul(szTmpString, NULL, 16);
			SetStringf(0, _T("%d"), lVal); // STRING_OK
		}

		sProp.ulPropTag = m_ulPropTag;
		sProp.Value.l = lVal;

		InterpretPropSmartView(&sProp,
			m_lpMAPIProp,
			NULL,
			NULL,
			m_bIsAB,
			m_bMVRow,
			&szSmartView);

		if (m_lpSmartView) m_lpSmartView->SetStringW(szSmartView);
		delete[] szSmartView;
		szSmartView = NULL;
		break;
	case PT_CURRENCY:
		if (0 == i || 1 == i)
		{
			szTmpString = GetStringUseControl(0);
			curVal.Hi = _tcstoul(szTmpString, NULL, 16);
			szTmpString = GetStringUseControl(1);
			curVal.Lo = _tcstoul(szTmpString, NULL, 16);
			SetString(2, CurrencyToString(curVal));
		}
		else if (2 == i)
		{
			szTmpString = GetStringUseControl(i);
			szTmpString.Remove(_T('.'));
			curVal.int64 = _ttoi64(szTmpString);
			SetHex(0, (int)curVal.Hi);
			SetHex(1, (int)curVal.Lo);
		}

		break;
	case PT_I8:
		if (0 == i || 1 == i)
		{
			szTmpString = GetStringUseControl(0);
			liVal.HighPart = (long)_tcstoul(szTmpString, NULL, 16);
			szTmpString = GetStringUseControl(1);
			liVal.LowPart = (long)_tcstoul(szTmpString, NULL, 16);
			SetStringf(2, _T("%I64d"), liVal.QuadPart); // STRING_OK
		}
		else if (2 == i)
		{
			szTmpString = GetStringUseControl(i);
			liVal.QuadPart = _ttoi64(szTmpString);
			SetHex(0, (int)liVal.HighPart);
			SetHex(1, (int)liVal.LowPart);
		}

		sProp.ulPropTag = m_ulPropTag;
		sProp.Value.li = liVal;

		InterpretPropSmartView(&sProp,
			m_lpMAPIProp,
			NULL,
			NULL,
			m_bIsAB,
			m_bMVRow,
			&szSmartView);

		if (m_lpSmartView) m_lpSmartView->SetStringW(szSmartView);
		delete[] szSmartView;
		szSmartView = NULL;
		break;
	case PT_SYSTIME: // components are unsigned hex
		szTmpString = GetStringUseControl(0);
		ftVal.dwLowDateTime = _tcstoul(szTmpString, NULL, 16);
		szTmpString = GetStringUseControl(1);
		ftVal.dwHighDateTime = _tcstoul(szTmpString, NULL, 16);

		FileTimeToString(&ftVal, &szTmpString, NULL);
		SetString(2, szTmpString);
		break;
	case PT_BINARY:
		if (0 == i || 2 == i)
		{
			if (GetBinaryUseControl(0, (size_t*)&Bin.cb, &lpb))
			{
				Bin.lpb = lpb;
				// Treat as a NULL terminated string
				// GetBinaryUseControl includes extra NULLs at the end of the buffer to make this work
				if (0 == i) SetStringA(1, (LPCSTR)Bin.lpb, Bin.cb + 1); // ansi string
			}
		}
		else if (1 == i)
		{
			size_t cchStr = NULL;
			LPSTR lpszA = GetEditBoxTextA(1, &cchStr); // Do not free this
			Bin.lpb = (LPBYTE)lpszA;

			// What we just read includes a NULL terminator, in both the string and count.
			// When we write binary, we don't want to include this NULL
			if (cchStr) cchStr -= 1;
			Bin.cb = (ULONG)cchStr * sizeof(CHAR);

			SetBinary(0, (LPBYTE)Bin.lpb, Bin.cb);
		}

		lpPane = (CountedTextPane*)GetControl(0);
		if (lpPane) lpPane->SetCount(Bin.cb);

		lpPane = (CountedTextPane*)GetControl(1);
		if (lpPane) lpPane->SetCount(Bin.cb);

		if (m_lpSmartView) m_lpSmartView->Parse(Bin);

		delete[] lpb;
		lpb = NULL;
		break;
	case PT_STRING8:
		if (0 == i)
		{
			size_t cbStr = 0;
			size_t cchStr = 0;
			LPSTR lpszA = GetEditBoxTextA(0, &cchStr);

			lpPane = (CountedTextPane*)GetControl(1);
			if (lpPane)
			{
				// What we just read includes a NULL terminator, in both the string and count.
				// When we write binary, we don't want to include this NULL
				if (cchStr) cchStr -= 1;
				cbStr = cchStr * sizeof(CHAR);

				// Even if we don't have a string, still make the call to SetBinary
				// This will blank out the binary control when lpszA is NULL
				lpPane->SetBinary((LPBYTE)lpszA, cbStr);
				lpPane->SetCount(cbStr);
			}

			lpPane = (CountedTextPane*)GetControl(0);
			if (lpPane) lpPane->SetCount(cbStr);
		}
		else if (1 == i)
		{
			size_t cb = 0;

			(void)GetBinaryUseControl(1, &cb, &lpb);

			// GetBinaryUseControl includes extra NULLs at the end of the buffer to make this work
			SetStringA(0, (LPCSTR)lpb, cb + 1);

			lpPane = (CountedTextPane*)GetControl(0);
			if (lpPane) lpPane->SetCount(cb);

			lpPane = (CountedTextPane*)GetControl(1);
			if (lpPane) lpPane->SetCount(cb);
			delete[] lpb;
			lpb = NULL;
		}

		break;
	case PT_UNICODE:
		if (0 == i)
		{
			size_t cbStr = 0;
			size_t cchStr = 0;
			LPWSTR lpszW = GetEditBoxTextW(0, &cchStr);

			lpPane = (CountedTextPane*)GetControl(1);
			if (lpPane)
			{
				// What we just read includes a NULL terminator, in both the string and count.
				// When we write binary, we don't want to include this NULL
				if (cchStr) cchStr -= 1;
				cbStr = cchStr * sizeof(WCHAR);

				// Even if we don't have a string, still make the call to SetBinary
				// This will blank out the binary control when lpszW is NULL
				lpPane->SetBinary((LPBYTE)lpszW, cbStr);
				lpPane->SetCount(cbStr);
			}

			lpPane = (CountedTextPane*)GetControl(0);
			if (lpPane) lpPane->SetCount(cchStr);
		}
		else if (1 == i)
		{
			size_t cb = 0;

			if (GetBinaryUseControl(1, &cb, &lpb) && !(cb % sizeof(WCHAR)))
			{
				// GetBinaryUseControl includes extra NULLs at the end of the buffer to make this work
				SetStringW(0, (LPCWSTR)lpb, cb / sizeof(WCHAR)+1);
			}
			else
			{
				SetStringW(0, NULL);
			}

			lpPane = (CountedTextPane*)GetControl(1);
			if (lpPane) lpPane->SetCount(cb);

			lpPane = (CountedTextPane*)GetControl(0);
			if (lpPane) lpPane->SetCount((cb % sizeof(WCHAR)) ? 0 : cb / sizeof(WCHAR));
			delete[] lpb;
			lpb = NULL;
		}

		break;
	default:
		break;
	}

	OnRecalcLayout();
	return i;
} // CPropertyEditor::HandleChange

static TCHAR* MVCLASS = _T("CMultiValuePropertyEditor"); // STRING_OK

// Create an editor for a MAPI property
CMultiValuePropertyEditor::CMultiValuePropertyEditor(
	_In_ CWnd* pParentWnd,
	UINT uidTitle,
	UINT uidPrompt,
	bool bIsAB,
	_In_opt_ LPVOID lpAllocParent,
	_In_opt_ LPMAPIPROP lpMAPIProp,
	ULONG ulPropTag,
	_In_opt_ LPSPropValue lpsPropValue) :
	CEditor(pParentWnd, uidTitle, uidPrompt, 0, CEDITOR_BUTTON_OK | CEDITOR_BUTTON_CANCEL)
{
	TRACE_CONSTRUCTOR(MVCLASS);

	m_bIsAB = bIsAB;
	m_lpAllocParent = lpAllocParent;
	m_lpsOutputValue = NULL;

	m_lpMAPIProp = lpMAPIProp;
	if (m_lpMAPIProp) m_lpMAPIProp->AddRef();
	m_ulPropTag = ulPropTag;
	m_lpsInputValue = lpsPropValue;

	CString szPromptPostFix;
	szPromptPostFix.Format(_T("\r\n%s"), (LPCTSTR)TagToString(m_ulPropTag, m_lpMAPIProp, m_bIsAB, false)); // STRING_OK

	SetPromptPostFix(szPromptPostFix);

	// Let's crack our property open and see what kind of controls we'll need for it
	CreatePropertyControls();

	InitPropertyControls();
} // CMultiValuePropertyEditor::CMultiValuePropertyEditor

CMultiValuePropertyEditor::~CMultiValuePropertyEditor()
{
	TRACE_DESTRUCTOR(MVCLASS);
	if (m_lpMAPIProp) m_lpMAPIProp->Release();
} // CMultiValuePropertyEditor::~CMultiValuePropertyEditor

BOOL CMultiValuePropertyEditor::OnInitDialog()
{
	BOOL bRet = CEditor::OnInitDialog();

	ReadMultiValueStringsFromProperty();
	ResizeList(0, false);

	LPWSTR szSmartView = NULL;
	ULONG iStructType = InterpretPropSmartView(m_lpsInputValue,
		m_lpMAPIProp,
		NULL,
		NULL,
		m_bIsAB,
		true,
		&szSmartView);
	if (szSmartView)
	{
		SmartViewPane* lpPane = (SmartViewPane*)GetControl(1);
		if (lpPane)
		{
			lpPane->SetParser(iStructType);
			lpPane->SetStringW(szSmartView);
		}
	}

	delete[] szSmartView;

	UpdateListButtons();

	return bRet;
} // CMultiValuePropertyEditor::OnInitDialog

void CMultiValuePropertyEditor::OnOK()
{
	// This is where we write our changes back
	WriteMultiValueStringsToSPropValue();
	WriteSPropValueToObject();
	CMyDialog::OnOK(); // don't need to call CEditor::OnOK
} // CMultiValuePropertyEditor::OnOK

void CMultiValuePropertyEditor::CreatePropertyControls()
{
	if (PT_MV_BINARY == PROP_TYPE(m_ulPropTag) ||
		PT_MV_LONG == PROP_TYPE(m_ulPropTag))
	{
		CreateControls(2);
	}
	else
	{
		CreateControls(1);
	}
} // CMultiValuePropertyEditor::CreatePropertyControls

void CMultiValuePropertyEditor::InitPropertyControls()
{
	InitPane(0, CreateListPane(IDS_PROPVALUES, false, false, this));
	if (PT_MV_BINARY == PROP_TYPE(m_ulPropTag) ||
		PT_MV_LONG == PROP_TYPE(m_ulPropTag))
	{
		SmartViewPane* lpPane = (SmartViewPane*)CreateSmartViewPane(IDS_SMARTVIEW);
		InitPane(1, lpPane);

		if (lpPane && PT_MV_LONG == PROP_TYPE(m_ulPropTag))
		{
			lpPane->DisableDropDown();
		}
	}
} // CMultiValuePropertyEditor::InitPropertyControls

// Function must be called AFTER dialog controls have been created, not before
void CMultiValuePropertyEditor::ReadMultiValueStringsFromProperty()
{
	if (!IsValidList(0)) return;

	InsertColumn(0, 0, IDS_ENTRY);
	InsertColumn(0, 1, IDS_VALUE);
	InsertColumn(0, 2, IDS_ALTERNATEVIEW);
	if (PT_MV_LONG == PROP_TYPE(m_ulPropTag) ||
		PT_MV_BINARY == PROP_TYPE(m_ulPropTag))
	{
		InsertColumn(0, 3, IDS_SMARTVIEW);
	}

	if (!m_lpsInputValue) return;
	if (!(PROP_TYPE(m_lpsInputValue->ulPropTag) & MV_FLAG)) return;

	CString szTmp;
	ULONG iMVCount = 0;
	// All the MV structures are basically the same, so we can cheat when we pull the count
	ULONG cValues = m_lpsInputValue->Value.MVi.cValues;
	for (iMVCount = 0; iMVCount < cValues; iMVCount++)
	{
		szTmp.Format(_T("%u"), iMVCount); // STRING_OK
		SortListData* lpData = InsertListRow(0, iMVCount, szTmp);

		if (lpData)
		{
			lpData->ulSortDataType = SORTLIST_MVPROP;
			switch (PROP_TYPE(m_lpsInputValue->ulPropTag))
			{
			case PT_MV_I2:
				lpData->data.MV.val.i = m_lpsInputValue->Value.MVi.lpi[iMVCount];
				break;
			case PT_MV_LONG:
				lpData->data.MV.val.l = m_lpsInputValue->Value.MVl.lpl[iMVCount];
				break;
			case PT_MV_DOUBLE:
				lpData->data.MV.val.dbl = m_lpsInputValue->Value.MVdbl.lpdbl[iMVCount];
				break;
			case PT_MV_CURRENCY:
				lpData->data.MV.val.cur = m_lpsInputValue->Value.MVcur.lpcur[iMVCount];
				break;
			case PT_MV_APPTIME:
				lpData->data.MV.val.at = m_lpsInputValue->Value.MVat.lpat[iMVCount];
				break;
			case PT_MV_SYSTIME:
				lpData->data.MV.val.ft = m_lpsInputValue->Value.MVft.lpft[iMVCount];
				break;
			case PT_MV_I8:
				lpData->data.MV.val.li = m_lpsInputValue->Value.MVli.lpli[iMVCount];
				break;
			case PT_MV_R4:
				lpData->data.MV.val.flt = m_lpsInputValue->Value.MVflt.lpflt[iMVCount];
				break;
			case PT_MV_STRING8:
				lpData->data.MV.val.lpszA = m_lpsInputValue->Value.MVszA.lppszA[iMVCount];
				break;
			case PT_MV_UNICODE:
				lpData->data.MV.val.lpszW = m_lpsInputValue->Value.MVszW.lppszW[iMVCount];
				break;
			case PT_MV_BINARY:
				lpData->data.MV.val.bin = m_lpsInputValue->Value.MVbin.lpbin[iMVCount];
				break;
			case PT_MV_CLSID:
				lpData->data.MV.val.lpguid = &m_lpsInputValue->Value.MVguid.lpguid[iMVCount];
				break;
			default:
				break;
			}

			SPropValue sProp = { 0 };
			sProp.ulPropTag = CHANGE_PROP_TYPE(m_lpsInputValue->ulPropTag, PROP_TYPE(m_lpsInputValue->ulPropTag) & ~MV_FLAG);
			sProp.Value = lpData->data.MV.val;
			UpdateListRow(&sProp, iMVCount);

			lpData->bItemFullyLoaded = true;
		}
	}
} // CMultiValuePropertyEditor::ReadMultiValueStringsFromProperty

// Perisist the data in the controls to m_lpsOutputValue
void CMultiValuePropertyEditor::WriteMultiValueStringsToSPropValue()
{
	if (!IsValidList(0)) return;

	// If we're not dirty, don't write
	// Unless we had no input value. Then we're creating a new property.
	// So we're implicitly dirty.
	if (!IsDirty(0) && m_lpsInputValue) return;

	HRESULT hRes = S_OK;
	// Take care of allocations first
	if (!m_lpsOutputValue)
	{
		if (m_lpAllocParent)
		{
			EC_H(MAPIAllocateMore(
				sizeof(SPropValue),
				m_lpAllocParent,
				(LPVOID*)&m_lpsOutputValue));
		}
		else
		{
			EC_H(MAPIAllocateBuffer(
				sizeof(SPropValue),
				(LPVOID*)&m_lpsOutputValue));
			m_lpAllocParent = m_lpsOutputValue;
		}
	}

	if (m_lpsOutputValue)
	{
		WriteMultiValueStringsToSPropValue((LPVOID)m_lpAllocParent, m_lpsOutputValue);
	}
} // CMultiValuePropertyEditor::WriteMultiValueStringsToSPropValue

// Given a pointer to an SPropValue structure which has already been allocated, fill out the values
void CMultiValuePropertyEditor::WriteMultiValueStringsToSPropValue(_In_ LPVOID lpParent, _In_ LPSPropValue lpsProp)
{
	if (!lpParent || !lpsProp) return;

	HRESULT hRes = S_OK;
	ULONG ulNumVals = GetListCount(0);
	ULONG iMVCount = 0;

	lpsProp->ulPropTag = m_ulPropTag;
	lpsProp->dwAlignPad = NULL;

	switch (PROP_TYPE(lpsProp->ulPropTag))
	{
	case PT_MV_I2:
		EC_H(MAPIAllocateMore(sizeof(short int)* ulNumVals, lpParent, (LPVOID*)&lpsProp->Value.MVi.lpi));
		lpsProp->Value.MVi.cValues = ulNumVals;
		break;
	case PT_MV_LONG:
		EC_H(MAPIAllocateMore(sizeof(LONG)* ulNumVals, lpParent, (LPVOID*)&lpsProp->Value.MVl.lpl));
		lpsProp->Value.MVl.cValues = ulNumVals;
		break;
	case PT_MV_DOUBLE:
		EC_H(MAPIAllocateMore(sizeof(double)* ulNumVals, lpParent, (LPVOID*)&lpsProp->Value.MVdbl.lpdbl));
		lpsProp->Value.MVdbl.cValues = ulNumVals;
		break;
	case PT_MV_CURRENCY:
		EC_H(MAPIAllocateMore(sizeof(CURRENCY)* ulNumVals, lpParent, (LPVOID*)&lpsProp->Value.MVcur.lpcur));
		lpsProp->Value.MVcur.cValues = ulNumVals;
		break;
	case PT_MV_APPTIME:
		EC_H(MAPIAllocateMore(sizeof(double)* ulNumVals, lpParent, (LPVOID*)&lpsProp->Value.MVat.lpat));
		lpsProp->Value.MVat.cValues = ulNumVals;
		break;
	case PT_MV_SYSTIME:
		EC_H(MAPIAllocateMore(sizeof(FILETIME)* ulNumVals, lpParent, (LPVOID*)&lpsProp->Value.MVft.lpft));
		lpsProp->Value.MVft.cValues = ulNumVals;
		break;
	case PT_MV_I8:
		EC_H(MAPIAllocateMore(sizeof(LARGE_INTEGER)* ulNumVals, lpParent, (LPVOID*)&lpsProp->Value.MVli.lpli));
		lpsProp->Value.MVli.cValues = ulNumVals;
		break;
	case PT_MV_R4:
		EC_H(MAPIAllocateMore(sizeof(float)* ulNumVals, lpParent, (LPVOID*)&lpsProp->Value.MVflt.lpflt));
		lpsProp->Value.MVflt.cValues = ulNumVals;
		break;
	case PT_MV_STRING8:
		EC_H(MAPIAllocateMore(sizeof(LPSTR)* ulNumVals, lpParent, (LPVOID*)&lpsProp->Value.MVszA.lppszA));
		lpsProp->Value.MVszA.cValues = ulNumVals;
		break;
	case PT_MV_UNICODE:
		EC_H(MAPIAllocateMore(sizeof(LPWSTR)* ulNumVals, lpParent, (LPVOID*)&lpsProp->Value.MVszW.lppszW));
		lpsProp->Value.MVszW.cValues = ulNumVals;
		break;
	case PT_MV_BINARY:
		EC_H(MAPIAllocateMore(sizeof(SBinary)* ulNumVals, lpParent, (LPVOID*)&lpsProp->Value.MVbin.lpbin));
		lpsProp->Value.MVbin.cValues = ulNumVals;
		break;
	case PT_MV_CLSID:
		EC_H(MAPIAllocateMore(sizeof(GUID)* ulNumVals, lpParent, (LPVOID*)&lpsProp->Value.MVguid.lpguid));
		lpsProp->Value.MVguid.cValues = ulNumVals;
		break;
	default:
		break;
	}
	// Allocation is now done

	// Now write our data into the space we allocated
	for (iMVCount = 0; iMVCount < ulNumVals; iMVCount++)
	{
		SortListData* lpData = GetListRowData(0, iMVCount);

		if (lpData)
		{
			switch (PROP_TYPE(lpsProp->ulPropTag))
			{
			case PT_MV_I2:
				lpsProp->Value.MVi.lpi[iMVCount] = lpData->data.MV.val.i;
				break;
			case PT_MV_LONG:
				lpsProp->Value.MVl.lpl[iMVCount] = lpData->data.MV.val.l;
				break;
			case PT_MV_DOUBLE:
				lpsProp->Value.MVdbl.lpdbl[iMVCount] = lpData->data.MV.val.dbl;
				break;
			case PT_MV_CURRENCY:
				lpsProp->Value.MVcur.lpcur[iMVCount] = lpData->data.MV.val.cur;
				break;
			case PT_MV_APPTIME:
				lpsProp->Value.MVat.lpat[iMVCount] = lpData->data.MV.val.at;
				break;
			case PT_MV_SYSTIME:
				lpsProp->Value.MVft.lpft[iMVCount] = lpData->data.MV.val.ft;
				break;
			case PT_MV_I8:
				lpsProp->Value.MVli.lpli[iMVCount] = lpData->data.MV.val.li;
				break;
			case PT_MV_R4:
				lpsProp->Value.MVflt.lpflt[iMVCount] = lpData->data.MV.val.flt;
				break;
			case PT_MV_STRING8:
				EC_H(CopyStringA(&lpsProp->Value.MVszA.lppszA[iMVCount], lpData->data.MV.val.lpszA, lpParent));
				break;
			case PT_MV_UNICODE:
				EC_H(CopyStringW(&lpsProp->Value.MVszW.lppszW[iMVCount], lpData->data.MV.val.lpszW, lpParent));
				break;
			case PT_MV_BINARY:
				EC_H(CopySBinary(&lpsProp->Value.MVbin.lpbin[iMVCount], &lpData->data.MV.val.bin, lpParent));
				break;
			case PT_MV_CLSID:
				if (lpData->data.MV.val.lpguid)
				{
					lpsProp->Value.MVguid.lpguid[iMVCount] = *lpData->data.MV.val.lpguid;
				}

				break;
			default:
				break;
			}
		}
	}
} // CMultiValuePropertyEditor::WriteMultiValueStringsToSPropValue

void CMultiValuePropertyEditor::WriteSPropValueToObject()
{
	if (!m_lpsOutputValue || !m_lpMAPIProp) return;

	HRESULT hRes = S_OK;

	LPSPropProblemArray lpProblemArray = NULL;

	EC_MAPI(m_lpMAPIProp->SetProps(
		1,
		m_lpsOutputValue,
		&lpProblemArray));

	EC_PROBLEMARRAY(lpProblemArray);
	MAPIFreeBuffer(lpProblemArray);

	EC_MAPI(m_lpMAPIProp->SaveChanges(KEEP_OPEN_READWRITE));
} // CMultiValuePropertyEditor::WriteSPropValueToObject

// Callers beware: Detatches and returns the modified prop value - this must be MAPIFreeBuffered!
_Check_return_ LPSPropValue CMultiValuePropertyEditor::DetachModifiedSPropValue()
{
	LPSPropValue m_lpRet = m_lpsOutputValue;
	m_lpsOutputValue = NULL;
	return m_lpRet;
} // CMultiValuePropertyEditor::DetachModifiedSPropValue

_Check_return_ bool CMultiValuePropertyEditor::DoListEdit(ULONG /*ulListNum*/, int iItem, _In_ SortListData* lpData)
{
	if (!lpData) return false;
	if (!IsValidList(0)) return false;

	HRESULT hRes = S_OK;
	SPropValue tmpPropVal = { 0 };
	// Strip off MV_FLAG since we're displaying only a row
	tmpPropVal.ulPropTag = m_ulPropTag & ~MV_FLAG;
	tmpPropVal.Value = lpData->data.MV.val;

	LPSPropValue lpNewValue = NULL;
	WC_H(DisplayPropertyEditor(
		this,
		IDS_EDITROW,
		NULL,
		m_bIsAB,
		NULL, // not passing an allocation parent because we know we're gonna free the result
		m_lpMAPIProp,
		NULL,
		true, // This is a row from a multivalued property. Only case we pass true here.
		&tmpPropVal,
		&lpNewValue));

	if (S_OK == hRes && lpNewValue)
	{
		ULONG ulBufSize = 0;
		size_t cbStr = 0;

		// This handles most cases by default - cases needing a buffer copied are handled below
		lpData->data.MV.val = lpNewValue->Value;
		switch (PROP_TYPE(lpNewValue->ulPropTag))
		{
		case PT_STRING8:
			// When the lpData is ultimately freed, MAPI will take care of freeing this.
			// This will be true even if we do this multiple times off the same lpData!
			EC_H(StringCbLengthA(lpNewValue->Value.lpszA, STRSAFE_MAX_CCH * sizeof(char), &cbStr));
			cbStr += sizeof(char);

			EC_H(MAPIAllocateMore(
				(ULONG)cbStr,
				lpData,
				(LPVOID*)&lpData->data.MV.val.lpszA));

			if (S_OK == hRes)
			{
				memcpy(lpData->data.MV.val.lpszA, lpNewValue->Value.lpszA, cbStr);
			}

			break;
		case PT_UNICODE:
			EC_H(StringCbLengthW(lpNewValue->Value.lpszW, STRSAFE_MAX_CCH * sizeof(WCHAR), &cbStr));
			cbStr += sizeof(WCHAR);

			EC_H(MAPIAllocateMore(
				(ULONG)cbStr,
				lpData,
				(LPVOID*)&lpData->data.MV.val.lpszW));

			if (S_OK == hRes)
			{
				memcpy(lpData->data.MV.val.lpszW, lpNewValue->Value.lpszW, cbStr);
			}

			break;
		case PT_BINARY:
			ulBufSize = lpNewValue->Value.bin.cb;
			EC_H(MAPIAllocateMore(
				ulBufSize,
				lpData,
				(LPVOID*)&lpData->data.MV.val.bin.lpb));

			if (S_OK == hRes)
			{
				memcpy(lpData->data.MV.val.bin.lpb, lpNewValue->Value.bin.lpb, ulBufSize);
			}

			break;
		case PT_CLSID:
			ulBufSize = sizeof(GUID);
			EC_H(MAPIAllocateMore(
				ulBufSize,
				lpData,
				(LPVOID*)&lpData->data.MV.val.lpguid));

			if (S_OK == hRes)
			{
				memcpy(lpData->data.MV.val.lpguid, lpNewValue->Value.lpguid, ulBufSize);
			}

			break;
		default:
			break;
		}

		// update the UI
		UpdateListRow(lpNewValue, iItem);
		UpdateSmartView();
		return true;
	}

	// Remember we didn't have an allocation parent - this is safe
	MAPIFreeBuffer(lpNewValue);
	return false;
} // CMultiValuePropertyEditor::DoListEdit

void CMultiValuePropertyEditor::UpdateListRow(_In_ LPSPropValue lpProp, ULONG iMVCount)
{
	CString szTmp;
	CString szAltTmp;

	InterpretProp(lpProp, &szTmp, &szAltTmp);
	SetListString(0, iMVCount, 1, szTmp);
	SetListString(0, iMVCount, 2, szAltTmp);

	if (PT_MV_LONG == PROP_TYPE(m_ulPropTag) ||
		PT_MV_BINARY == PROP_TYPE(m_ulPropTag))
	{
		LPWSTR szSmartView = NULL;

		InterpretPropSmartView(lpProp,
			m_lpMAPIProp,
			NULL,
			NULL,
			m_bIsAB,
			true,
			&szSmartView);

		if (szSmartView) SetListStringW(0, iMVCount, 3, szSmartView);
		delete[] szSmartView;
		szSmartView = NULL;
	}
} // CMultiValuePropertyEditor::UpdateListRow

void CMultiValuePropertyEditor::UpdateSmartView()
{
	HRESULT hRes = S_OK;
	SmartViewPane* lpPane = (SmartViewPane*)GetControl(1);
	if (lpPane)
	{
		LPSPropValue lpsProp = NULL;
		EC_H(MAPIAllocateBuffer(
			sizeof(SPropValue),
			(LPVOID*)&lpsProp));
		if (lpsProp)
		{
			WriteMultiValueStringsToSPropValue((LPVOID)lpsProp, lpsProp);

			DWORD_PTR iStructType = NULL;
			LPWSTR szSmartView = NULL;
			switch (PROP_TYPE(m_ulPropTag))
			{
			case PT_MV_LONG:
				(void)InterpretPropSmartView(lpsProp, m_lpMAPIProp, NULL, NULL, m_bIsAB, true, &szSmartView);
				break;
			case PT_MV_BINARY:
				iStructType = lpPane->GetDropDownSelectionValue();
				if (iStructType)
				{
					InterpretMVBinaryAsString(lpsProp->Value.MVbin, iStructType, m_lpMAPIProp, lpsProp->ulPropTag, &szSmartView);
				}
				break;
			}

			if (szSmartView)
			{
				lpPane->SetStringW(szSmartView);
			}

			delete[] szSmartView;
		}

		MAPIFreeBuffer(lpsProp);
	}
} // CMultiValuePropertyEditor::UpdateSmartView

_Check_return_ ULONG CMultiValuePropertyEditor::HandleChange(UINT nID)
{
	ULONG i = (ULONG)-1;

	// We check against the list pane first so we can track if it handled the change,
	// because if it did, we're going to recalculate smart view.
	ListPane* lpPane = (ListPane*)GetControl(0);
	if (lpPane)
	{
		i = lpPane->HandleChange(nID);
	}

	if (-1 == i)
	{
		i = CEditor::HandleChange(nID);
	}

	if ((ULONG)-1 == i) return (ULONG)-1;

	UpdateSmartView();
	OnRecalcLayout();

	return i;
}