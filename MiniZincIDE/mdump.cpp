#ifdef USE_MINI_DUMP
#include "stdafx.h"
#include "mdump.h"

#include "FXUtil.h"

LPCTSTR MiniDumper::m_szAppName;

LPTSTR MiniDumper::m_szAppVersion;

LPTSTR MiniDumper::m_szAppBuildNumber;

TCHAR MiniDumper::m_szMessageText[MAX_WARNING_MESSAGE_PATH];

LPTSTR MiniDumper::m_szDumpFilePath;

#define DEFAULT_ENGLISH_MESSAGE_TEXT _T("MiniZinc IDE experienced an unknown error and had to exit. \nHowever, some error information has been saved in %s. \nPlease, email this file to <guido.tack@monash.edu> if you would like to help us debug the problem.")

#define MAX_DUMP_FILE_NUMBER 9999



MiniDumper::MiniDumper( LPCTSTR szAppName ,LPCTSTR szVersion,LPCTSTR szBuildNumber)
{
    // if this assert fires then you have two instances of MiniDumper
    // which is not allowed
    ASSERT( m_szAppName==NULL );

    m_szAppName = szAppName ? _tcsdup(szAppName) : _tcsdup(_T("Application"));
    m_szAppVersion = szVersion ? _tcsdup(szVersion) :_tcsdup( _T("1.0.0.0"));
    m_szAppBuildNumber = szBuildNumber ? _tcsdup(szBuildNumber) :_tcsdup( _T("0000"));

    _tcscpy(m_szMessageText,DEFAULT_ENGLISH_MESSAGE_TEXT);


    m_szDumpFilePath = NULL;

    ::SetUnhandledExceptionFilter( TopLevelFilter );
}

MiniDumper::~MiniDumper()
{
}

void MiniDumper::SetVersion(LPCTSTR szVersion)
{
    if(szVersion)
    {
        free(m_szAppVersion);
        m_szAppVersion = _tcsdup(szVersion);
    }
}

void MiniDumper::SetBuildNumber(LPCTSTR szBuildNumber)
{
    if(szBuildNumber)
    {
        free(m_szAppBuildNumber);
        m_szAppBuildNumber = _tcsdup(szBuildNumber);
    }
}

void MiniDumper::SetDumpFilePath(LPCTSTR szFilePath)
{
    free(m_szDumpFilePath);
    m_szDumpFilePath = NULL;
    if(szFilePath != NULL)
    {
        m_szDumpFilePath = _tcsdup(szFilePath);
    }
}

LONG MiniDumper::TopLevelFilter( struct _EXCEPTION_POINTERS *pExceptionInfo )
{
    LONG retval = EXCEPTION_CONTINUE_SEARCH;
    HWND hParent = NULL;						// find a better value for your app

    // firstly see if dbghelp.dll is around and has the function we need
    // look next to the EXE first, as the one in System32 might be old
    // (e.g. Windows 2000)
    HMODULE hDll = NULL;
    TCHAR szDbgHelpPath[_MAX_PATH];

    if (GetModuleFileName( NULL, szDbgHelpPath, _MAX_PATH ))
    {
        TCHAR *pSlash = _tcsrchr( szDbgHelpPath, _T('\\') );
        if (pSlash)
        {
            _tcscpy( pSlash+1, _T("DBGHELP.DLL") );
            hDll = ::LoadLibrary( szDbgHelpPath );
        }
    }

    if (hDll==NULL)
    {
        // load any version we can
        hDll = ::LoadLibrary( _T("DBGHELP.DLL") );
    }

    LPCTSTR szResult = NULL;

    if (hDll)
    {
        MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress( hDll, "MiniDumpWriteDump" );
        if (pDump)
        {
            TCHAR szDumpPath[_MAX_PATH];
            TCHAR szDumpRootPath[_MAX_PATH];
            TCHAR szScratch [_MAX_PATH];

            // work out a good place for the dump file
            if(m_szDumpFilePath == NULL)
            {
                if (!GetTempPath( _MAX_PATH, szDumpPath ))
                    _tcscpy( szDumpPath, _T("c:\\temp\\") );
            }
            else
            {
                _tcscpy( szDumpPath, m_szDumpFilePath );
            }
            _tcscpy( szDumpRootPath, szDumpPath);

            PrintDebug(_T("[MiniDumper] Mini Dump file:[%s]"),szDumpPath);

            // ask the user if they want to save a dump file
            //if (::MessageBox( NULL, _T("Something bad happened in your program, would you like to save a diagnostic file?"), m_szAppName, MB_YESNO )==IDYES)
            {
                HANDLE hFile = INVALID_HANDLE_VALUE;
                int i = 1;
                TCHAR szFileNumber[_MAX_PATH];
                while(hFile == INVALID_HANDLE_VALUE)
                {
                    _stprintf(szFileNumber,_T("_%04d"),i);
                    _tcscpy( szDumpPath, szDumpRootPath);
                    _tcscat( szDumpPath, m_szAppName );
                    _tcscat( szDumpPath, _T("_") );
                    _tcscat( szDumpPath, m_szAppVersion);
                    _tcscat( szDumpPath, _T("_") );
                    _tcscat( szDumpPath, m_szAppBuildNumber);
                    _tcscat( szDumpPath, szFileNumber);
                    _tcscat( szDumpPath, _T(".dmp") );
                    hFile = ::CreateFile( szDumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW,
                                            FILE_ATTRIBUTE_NORMAL, NULL );
                    i++;
                    if(i > MAX_DUMP_FILE_NUMBER)
                    {
                        hFile = ::CreateFile( szDumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                                            FILE_ATTRIBUTE_NORMAL, NULL );
                        break;
                    }
                }
                // create the file

                if (hFile!=INVALID_HANDLE_VALUE)
                {
                    _MINIDUMP_EXCEPTION_INFORMATION ExInfo;

                    ExInfo.ThreadId = ::GetCurrentThreadId();
                    ExInfo.ExceptionPointers = pExceptionInfo;
                    ExInfo.ClientPointers = NULL;

                    // write the dump
                    BOOL bOK = pDump( GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &ExInfo, NULL, NULL );
                    if (bOK)
                    {
                        _stprintf( szScratch, _T("Saved dump file to '%s'"), szDumpPath );
                        szResult = szScratch;
                        retval = EXCEPTION_EXECUTE_HANDLER;
                    }
                    else
                    {
                        _stprintf( szScratch, _T("Failed to save dump file to '%s' (error %d)"), szDumpPath, GetLastError() );
                        szResult = szScratch;
                    }
                    ::CloseHandle(hFile);

                    TCHAR csOutMessage[MAX_WARNING_MESSAGE_PATH];
                    _stprintf(csOutMessage,m_szMessageText,szDumpPath );

                    PrintError(_T("%s"), csOutMessage);
                    ::MessageBox( NULL, csOutMessage,m_szAppName , MB_OK );
                }
                else
                {
                    _stprintf( szScratch, _T("Failed to create dump file '%s' (error %d)"), szDumpPath, GetLastError() );
                    szResult = szScratch;
                }
            }

        }
        else
        {
            szResult = _T("DBGHELP.DLL too old");
        }
    }
    else
    {
        szResult = _T("DBGHELP.DLL not found");
    }

    if (szResult)
    {
        PrintDebug(_T("[MiniDumper] Mini Dump result:[%s]"),szResult);
    }

    return retval;
}
#endif
