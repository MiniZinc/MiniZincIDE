#ifndef MDUMP_H
#define MDUMP_H

#if !defined(AFX_MDUMP_H__FCECFEFE6E_FA36_4693_B07C_F8JHT75BB0101B__INCLUDED_)
#define AFX_MDUMP_H__FCECFEFE6E_FA36_4693_B07C_F8JHT75BB0101B__INCLUDED_
#ifdef USE_MINI_DUMP

/**
 * Date: 1st August 2008
 * Modified by James - duy.trinh@fix8.com
 */

/** -------------- How to debug ----------------
1. save pdb file when you compile
2. get dmp file when crash
3. put exe & pdb & dmp file together
4. open dmp file with vs2005, press f5 to run, then you can see the resul
*/

#include "dbghelp.h"

// based on dbghelp.h
typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
                                    CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
                                    CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
                                    CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
                                    );

#define MAX_WARNING_MESSAGE_PATH 1024

class MiniDumper
{
private:
    static LPCTSTR m_szAppName;

    static LPTSTR m_szAppVersion;

    static LPTSTR m_szAppBuildNumber;

    static TCHAR m_szMessageText[MAX_WARNING_MESSAGE_PATH];

    static LPTSTR m_szDumpFilePath;

    static LONG WINAPI TopLevelFilter( struct _EXCEPTION_POINTERS *pExceptionInfo );

public:
    MiniDumper( LPCTSTR szAppName ,LPCTSTR szVersion,LPCTSTR szBuildNumber = NULL);
    ~MiniDumper();
    static void SetVersion(LPCTSTR szVersion);
    static void SetBuildNumber(LPCTSTR szBuildNumber);
    static void SetDumpFilePath(LPCTSTR szFilePath);
    static int SetWarningMessage(LPCTSTR szMessageText)
    {
        if(szMessageText)
        {
            int iLen = _tcslen(szMessageText);
            if(iLen < MAX_WARNING_MESSAGE_PATH - MAX_PATH)
            {
                _tcscpy(m_szMessageText,szMessageText);
                return 0;
            }
        }
        return 1;
    }
};
#endif
#endif

#endif // MDUMP_H

