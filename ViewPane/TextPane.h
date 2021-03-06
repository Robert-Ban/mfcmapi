#pragma once
// TextPane.h : header file

#include "ViewPane.h"

ViewPane* CreateMultiLinePaneA(UINT uidLabel, _In_opt_z_ LPCSTR szVal, bool bReadOnly);
ViewPane* CreateMultiLinePaneW(UINT uidLabel, _In_opt_z_ LPCWSTR szVal, bool bReadOnly);
ViewPane* CreateSingleLinePaneA(UINT uidLabel, _In_opt_z_ LPCSTR szVal, bool bReadOnly, bool bMultiLine = false);
ViewPane* CreateSingleLinePaneW(UINT uidLabel, _In_opt_z_ LPCWSTR szVal, bool bReadOnly, bool bMultiLine = false);
ViewPane* CreateSingleLinePaneID(UINT uidLabel, UINT uidVal, bool bReadOnly);
#ifdef UNICODE
#define CreateMultiLinePane CreateMultiLinePaneW
#define CreateSingleLinePane CreateSingleLinePaneW
#else
#define CreateMultiLinePane CreateMultiLinePaneA
#define CreateSingleLinePane CreateSingleLinePaneA
#endif

#define LINES_MULTILINEEDIT 4

class TextPane : public ViewPane
{
public:
	TextPane(UINT uidLabel, bool bReadOnly, bool bMultiLine);
	virtual ~TextPane();

	virtual bool IsType(__ViewTypes vType);
	virtual void Initialize(int iControl, _In_ CWnd* pParent, _In_ HDC hdc);
	virtual void SetWindowPos(int x, int y, int width, int height);
	virtual void CommitUIValues();
	virtual ULONG GetFlags();
	virtual int GetFixedHeight();
	virtual int GetLines();

	void ClearView();
	virtual void SetStringA(_In_opt_z_ LPCSTR szMsg, size_t cchsz = -1);
	virtual void SetStringW(_In_opt_z_ LPCWSTR szMsg, size_t cchsz = -1);
	void SetBinary(_In_opt_count_(cb) LPBYTE lpb, size_t cb);
	void InitEditFromBinaryStream(_In_ LPSTREAM lpStreamIn);
	void WriteToBinaryStream(_In_ LPSTREAM lpStreamOut);
	void AppendString(_In_z_ LPCTSTR szMsg);
	void ShowWindow(int nCmdShow);

	void SetEditReadOnly();

	LPWSTR GetStringW();
	LPSTR GetStringA();
	_Check_return_ LPSTR GetEditBoxTextA(_Out_ size_t* lpcchText);
	_Check_return_ LPWSTR GetEditBoxTextW(_Out_ size_t* lpcchText);
	_Check_return_ CString GetStringUseControl();

protected:
	CRichEditCtrl m_EditBox;

private:
	void SetEditBoxText();
	void ClearString();

	LPWSTR m_lpszW;
	LPSTR m_lpszA; // on demand conversion of lpszW
	size_t m_cchsz; // length of string - maintained to preserve possible internal NULLs, includes NULL terminator
	bool m_bMultiline;
};