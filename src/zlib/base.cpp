#include "pch.h"

////////////////////////////////////////////////////////////////////////////////
//
// Helper Functions
//
////////////////////////////////////////////////////////////////////////////////

bool IsWindows9x()
{
    static bool bChecked = false;
    static bool bIs9x;

    if (!bChecked)
    {
        OSVERSIONINFO osversioninfo;

        osversioninfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

        ZVerify(GetVersionEx(&osversioninfo));

        bIs9x = osversioninfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS;
        bChecked = true;
    }

    return bIs9x;
}


////////////////////////////////////////////////////////////////////////////////
//
// ZFile
//
////////////////////////////////////////////////////////////////////////////////

// BUILD_DX9: added for DX9 but can stay for DX7 as well
ZFile::ZFile( )
{
	m_p = NULL;
	m_handle = INVALID_HANDLE_VALUE;
}


ZFile::ZFile(const PathString& strPath, DWORD how) :
	m_p(NULL),
	m_pathString(strPath) // BT - STEAM
{
	DWORD dwDesiredAccess = GENERIC_READ;
	DWORD dwShareMode = FILE_SHARE_WRITE;
	DWORD dwCreationDisposition = OPEN_EXISTING;

	if((how & OF_WRITE) == OF_WRITE)
		dwDesiredAccess = GENERIC_WRITE;

	if((how & OF_SHARE_DENY_WRITE) == OF_SHARE_DENY_WRITE)
		dwShareMode = FILE_SHARE_READ;

	if((how & OF_CREATE) == OF_CREATE)
		dwCreationDisposition = CREATE_ALWAYS;

	m_handle = CreateFile(strPath.MakeBSTR(), dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
}

ZFile::~ZFile()
{
    if (m_p) {
        UnmapViewOfFile(m_p);
        CloseHandle(m_hfileMapping);
    }
}

bool ZFile::IsValid()
{
     return m_handle != (HANDLE)HFILE_ERROR;
}

DWORD ZFile::Read(void* p, DWORD length)
{
    DWORD cbActual;

    ReadFile(m_handle, p, length, (LPDWORD)&cbActual, NULL);
    return cbActual;
}

DWORD ZFile::Write(void* p, DWORD length)
{
    DWORD cbActual;

    WriteFile(m_handle, p, length, (LPDWORD)&cbActual, NULL);

    ZAssert(cbActual == length);

    return cbActual;
}

bool ZFile::Write(const ZString& str)
{
    return (Write((void*)(PCC)str, str.GetLength()) != 0);
}

bool  ZFile::WriteString(const ZString& str)
{
    return (Write((void*)(PCC)str, str.GetLength() + 1) != 0);
}

bool  ZFile::WriteAlignedString(const ZString& str)
{
    int length = str.GetLength() + 1;

    if (Write((void*)(PCC)str, str.GetLength() + 1) != 0) {
        return WritePad(length);
    }

    return false;
}

bool ZFile::WritePad(int length)
{
    static BYTE zeros[] = { 0, 0, 0 };
    int pad = 4 - (length & 3);

    if (pad != 4) {
        return (Write(zeros, pad) != 0);
    }

    return true;
}

bool ZFile::Write(DWORD value)
{
    return (Write(&value, 4) != 0);
}

bool ZFile::Write(int value)
{
    return (Write(&value, 4) != 0);
}

bool ZFile::Write(float value)
{
    return (Write(&value, 4) != 0);
}

int ZFile::GetLength()
{
    return GetFileSize(m_handle, NULL);
}

BYTE* ZFile::GetPointer(bool bWrite, bool bCopyOnWrite)
{
    ZAssert(!bWrite || !bCopyOnWrite);

    if (m_p == NULL) {
        m_hfileMapping = 
            CreateFileMapping(
                m_handle,
                0,
                bWrite
                    ? PAGE_READWRITE
                    : PAGE_READONLY,
                0,
                0,
                NULL
            );

        ZAssert(m_hfileMapping != NULL);

        m_p = 
            (BYTE*)MapViewOfFile(
                m_hfileMapping, 
                bWrite
                    ? FILE_MAP_WRITE
                    : (bCopyOnWrite
                        ? FILE_MAP_COPY
                        : FILE_MAP_READ), 
                0, 
                0, 
                0
            );

        ZAssert(m_p != NULL);
    }

    return m_p;
}

ZWriteFile::ZWriteFile(const PathString& strPath) :
    ZFile(strPath, OF_CREATE | OF_WRITE | OF_SHARE_EXCLUSIVE)
{
}

// KGJV 32B - added Tell and Seek
long   ZFile::Tell()
{
    DWORD dwPtr = SetFilePointer(m_handle,0,NULL,FILE_CURRENT);
    if (dwPtr != INVALID_SET_FILE_POINTER)
        return (long) dwPtr;
    else
        return -1;
}
int   ZFile::Seek(long offset, int origin)
{
    DWORD dwPtr = SetFilePointer(m_handle,offset,NULL,origin);
    return (dwPtr != INVALID_SET_FILE_POINTER);
}


// BT - Steam
// https://social.msdn.microsoft.com/Forums/vstudio/en-US/c43c3b36-c698-45da-89f4-ecd0e825c059/implementing-sha1-hash-using-windows-cryptography-api-and-c?forum=vcgeneral
ZString ZFile::GetSha1Hash()
{
	const int bufsize = 1024;
	const int sha1len = 20;

	DWORD dwStatus = 0;
	BOOL bResult = FALSE;
	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;
	BYTE rgbFile[bufsize];
	DWORD cbRead = 0;
	BYTE rgbHash[sha1len];
	DWORD cbHash = 0;
	CHAR rgbDigits[] = "0123456789abcdef";

	char hexBuffer[3];
	ZString returnValue = "";

	// Get handle to the crypto provider
	if (!CryptAcquireContext(&hProv,
		NULL,
		NULL,
		PROV_RSA_FULL,
		CRYPT_VERIFYCONTEXT))
	{
		dwStatus = GetLastError();
		ZDebugOutput(ZString("CryptAcquireContext failed: ") + ZString((int) dwStatus));
		
		return "0";
	}

	if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash))
	{
		dwStatus = GetLastError();

		ZDebugOutput(ZString("CryptCreateHash failed: ") + ZString((int)dwStatus));
		CryptReleaseContext(hProv, 0);
		return "0";
	}

	for (cbRead = Read(rgbFile, bufsize); cbRead > 0; cbRead = Read(rgbFile, bufsize))
	{
		if (!CryptHashData(hHash, rgbFile, cbRead, 0))
		{
			dwStatus = GetLastError();

			ZDebugOutput(ZString("CryptHashData failed: ") + ZString((int)dwStatus));

			CryptReleaseContext(hProv, 0);
			CryptDestroyHash(hHash);
			return "0";
		}
	}


	
	cbHash = sha1len;
	if (CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0))
	{
		//ZDebugOutput(ZString("SHA1 hash of file ") + m_pathString.GetFilename() + " is: ");
		
		for (DWORD i = 0; i < cbHash; i++)
		{
			sprintf(hexBuffer, "%c%c", rgbDigits[rgbHash[i] >> 4],
				rgbDigits[rgbHash[i] & 0xf]);

			returnValue += hexBuffer;

			//ZDebugOutput(hexBuffer);
		}
		//ZDebugOutput("\n");
	}
	else
	{
		dwStatus = GetLastError();
		ZDebugOutput(ZString("CryptGetHashParam failed: ") + ZString((int)dwStatus));
	}

	CryptDestroyHash(hHash);
	CryptReleaseContext(hProv, 0);

	Seek(0, 0);

	return returnValue;
}