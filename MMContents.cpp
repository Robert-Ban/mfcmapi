#include "stdafx.h"
#include "MrMAPI.h"
#include "MMStore.h"
#include "MMFolder.h"
#include "DumpStore.h"
#include "File.h"

void DumpContentsTable(
	_In_z_ LPWSTR lpszProfile,
	_In_ LPMDB lpMDB,
	_In_ LPMAPIFOLDER lpFolder,
	_In_z_ LPWSTR lpszDir,
	_In_ ULONG ulOptions,
	_In_ ULONG ulFolder,
	_In_z_ LPWSTR lpszFolder,
	_In_ ULONG ulCount,
	_In_opt_ LPSRestriction lpRes)
{
	DebugPrint(DBGGeneric, "DumpContentsTable: Outputting folder %u / %ws from profile %ws to %ws\n", ulFolder, lpszFolder ? lpszFolder : L"", lpszProfile, lpszDir);
	if (ulOptions & OPT_DOCONTENTS)  DebugPrint(DBGGeneric, "DumpContentsTable: Outputting Contents\n");
	if (ulOptions & OPT_DOASSOCIATEDCONTENTS) DebugPrint(DBGGeneric, "DumpContentsTable: Outputting Associated Contents\n");
	if (ulOptions & OPT_MSG) DebugPrint(DBGGeneric, "DumpContentsTable: Outputting as MSG\n");
	if (ulOptions & OPT_RETRYSTREAMPROPS) DebugPrint(DBGGeneric, "DumpContentsTable: Will retry stream properties\n");
	if (ulOptions & OPT_LIST) DebugPrint(DBGGeneric, "DumpContentsTable: List only mode\n");
	if (ulCount) DebugPrint(DBGGeneric, "DumpContentsTable: Limiting output to %u messages.\n", ulCount);

	if (lpFolder)
	{
		CDumpStore MyDumpStore;
		SSortOrderSet SortOrder = { 0 };
		MyDumpStore.InitMDB(lpMDB);
		MyDumpStore.InitFolder(lpFolder);
		MyDumpStore.InitFolderPathRoot(lpszDir);
		MyDumpStore.InitFolderContentsRestriction(lpRes);
		if (ulOptions & OPT_MSG) MyDumpStore.EnableMSG();
		if (ulOptions & OPT_LIST) MyDumpStore.EnableList();
		if (ulCount)
		{
			MyDumpStore.InitMaxOutput(ulCount);
			SortOrder.cSorts = 1;
			SortOrder.cCategories = 0;
			SortOrder.cExpanded = 0;
			SortOrder.aSort[0].ulPropTag = PR_MESSAGE_DELIVERY_TIME;
			SortOrder.aSort[0].ulOrder = TABLE_SORT_DESCEND;
			MyDumpStore.InitSortOrder(&SortOrder);
		}
		if (!(ulOptions & OPT_RETRYSTREAMPROPS)) MyDumpStore.DisableStreamRetry();
		MyDumpStore.ProcessFolders(
			0 != (ulOptions & OPT_DOCONTENTS),
			0 != (ulOptions & OPT_DOASSOCIATEDCONTENTS),
			false);
	}
} // DumpContentsTable

void DumpMSG(_In_z_ LPCWSTR lpszMSGFile, _In_z_ LPCWSTR lpszXMLFile, _In_ bool bRetryStreamProps)
{
	HRESULT hRes = S_OK;
	LPMESSAGE lpMessage = NULL;

	WC_H(LoadMSGToMessage(lpszMSGFile, &lpMessage));

	if (lpMessage)
	{
		CDumpStore MyDumpStore;
		MyDumpStore.InitMessagePath(lpszXMLFile);
		if (!bRetryStreamProps) MyDumpStore.DisableStreamRetry();

		// Just assume this message might have attachments
		MyDumpStore.ProcessMessage(lpMessage, true, NULL);
		lpMessage->Release();
	}
} // DumpMSG

void DoContents(_In_ MYOPTIONS ProgOpts)
{
	SRestriction sResTop = { 0 };
	SRestriction sResMiddle[2] = { 0 };
	SRestriction sResSubject[2] = { 0 };
	SRestriction sResMessageClass[2] = { 0 };
	SPropValue sPropValue[2] = { 0 };
	LPSRestriction lpRes = NULL;
	if (ProgOpts.lpszSubject || ProgOpts.lpszMessageClass)
	{
		// RES_AND
		//   RES_AND (optional)
		//     RES_EXIST - PR_SUBJECT_W
		//     RES_CONTENT - lpszSubject
		//   RES_AND (optional)
		//     RES_EXIST - PR_MESSAGE_CLASS_W
		//     RES_CONTENT - lpszMessageClass
		int i = 0;
		if (ProgOpts.lpszSubject)
		{
			sResMiddle[i].rt = RES_AND;
			sResMiddle[i].res.resAnd.cRes = 2;
			sResMiddle[i].res.resAnd.lpRes = &sResSubject[0];
			sResSubject[0].rt = RES_EXIST;
			sResSubject[0].res.resExist.ulPropTag = PR_SUBJECT_W;
			sResSubject[1].rt = RES_CONTENT;
			sResSubject[1].res.resContent.ulPropTag = PR_SUBJECT_W;
			sResSubject[1].res.resContent.ulFuzzyLevel = FL_FULLSTRING | FL_IGNORECASE;
			sResSubject[1].res.resContent.lpProp = &sPropValue[0];
			sPropValue[0].ulPropTag = PR_SUBJECT_W;
			sPropValue[0].Value.lpszW = ProgOpts.lpszSubject;
			i++;
		}
		if (ProgOpts.lpszMessageClass)
		{
			sResMiddle[i].rt = RES_AND;
			sResMiddle[i].res.resAnd.cRes = 2;
			sResMiddle[i].res.resAnd.lpRes = &sResMessageClass[0];
			sResMessageClass[0].rt = RES_EXIST;
			sResMessageClass[0].res.resExist.ulPropTag = PR_MESSAGE_CLASS_W;
			sResMessageClass[1].rt = RES_CONTENT;
			sResMessageClass[1].res.resContent.ulPropTag = PR_MESSAGE_CLASS_W;
			sResMessageClass[1].res.resContent.ulFuzzyLevel = FL_FULLSTRING | FL_IGNORECASE;
			sResMessageClass[1].res.resContent.lpProp = &sPropValue[1];
			sPropValue[1].ulPropTag = PR_MESSAGE_CLASS_W;
			sPropValue[1].Value.lpszW = ProgOpts.lpszMessageClass;
			i++;
		}
		sResTop.rt = RES_AND;
		sResTop.res.resAnd.cRes = i;
		sResTop.res.resAnd.lpRes = &sResMiddle[0];
		lpRes = &sResTop;
		DebugPrintRestriction(DBGGeneric, lpRes, NULL);
	}
	DumpContentsTable(
		ProgOpts.lpszProfile,
		ProgOpts.lpMDB,
		ProgOpts.lpFolder,
		ProgOpts.lpszOutput ? ProgOpts.lpszOutput : L".",
		ProgOpts.ulOptions,
		ProgOpts.ulFolder,
		ProgOpts.lpszFolderPath,
		ProgOpts.ulCount,
		lpRes);
} // DoContents

void DoMSG(_In_ MYOPTIONS ProgOpts)
{
	DumpMSG(
		ProgOpts.lpszInput,
		ProgOpts.lpszOutput ? ProgOpts.lpszOutput : L".",
		0 != (ProgOpts.ulOptions & OPT_RETRYSTREAMPROPS));
} // DoMAPIMIME