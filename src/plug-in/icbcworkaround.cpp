#include "ietab.h" // class's header file
#include "resource.h"
#include "plugin.h"
#include <commctrl.h>
#include <intrin.h>
#include ".\ietab.h"
#include "ScriptableObject.h"

static PVOID vehHandler = NULL;
DWORD dwInitCount = 0;

typedef 
PVOID (WINAPI *PFNADDVECTOREDEXCEPTIONHANDLER) (
    __in ULONG First,
    __in PVECTORED_EXCEPTION_HANDLER Handler
    );

typedef
ULONG (WINAPI *PFNREMOVEVECTOREDEXCEPTIONHANDLER) (
    __in PVOID Handle
    );

LONG NTAPI vehProcedure( PEXCEPTION_POINTERS exc )
{
	DWORD dwOldProtect;

	// Ensure that the error is generated by something trying to access the data segment
	if( exc->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION )
	{
		return( EXCEPTION_CONTINUE_SEARCH );
	}
	else if( exc->ExceptionRecord->ExceptionInformation[0] != 8 )
	{
		// 8 == DEP
		return ( EXCEPTION_CONTINUE_SEARCH );
	}

	// Check whether this is our signature
	BYTE *bytes = (BYTE *)exc->ExceptionRecord->ExceptionInformation[1];
	if(( bytes[0] != 0xC7 ) ||
	   ( bytes[1] != 0x44 ) ||
	   ( bytes[2] != 0x24 ) ||
	   ( bytes[3] != 0x04 ) )
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
	// Remove protection, re-do checksum
	//VirtualProtect( ( LPVOID ) DATA_BEGIN, ( DATA_END - DATA_BEGIN ), PAGE_READWRITE, &OldProtect );
	VirtualProtect((LPVOID) exc->ExceptionRecord->ExceptionInformation[1], 32, PAGE_EXECUTE_READWRITE, &dwOldProtect );

	return( EXCEPTION_CONTINUE_EXECUTION );
}

void InitICBCWorkaround()
{
	if(dwInitCount)
	{
		dwInitCount++;
		return;
	}

	HMODULE hKernel32 = LoadLibrary("kernel32.dll");
	if(hKernel32)
	{
		PFNADDVECTOREDEXCEPTIONHANDLER pfnAddVectoredExceptionHandler = NULL;
		pfnAddVectoredExceptionHandler = (PFNADDVECTOREDEXCEPTIONHANDLER) GetProcAddress( hKernel32, "AddVectoredExceptionHandler" );

		if(pfnAddVectoredExceptionHandler)
		{
			vehHandler = pfnAddVectoredExceptionHandler( 0, vehProcedure );
			dwInitCount = 1;
		}
		FreeLibrary(hKernel32);
	}
}

void UninitICBCWorkaround()
{
	dwInitCount--;
	if(dwInitCount <= 0)
	{
		if(vehHandler)
		{
			HMODULE hKernel32 = LoadLibrary("kernel32.dll");
			if(hKernel32)
			{
				PFNREMOVEVECTOREDEXCEPTIONHANDLER pfnRemoveVectoredExceptionHandler = NULL;
				pfnRemoveVectoredExceptionHandler = (PFNREMOVEVECTOREDEXCEPTIONHANDLER) GetProcAddress( hKernel32, "RemoveVectoredExceptionHandler" );

				if(pfnRemoveVectoredExceptionHandler)
				{
					pfnRemoveVectoredExceptionHandler(vehHandler);
				}
				FreeLibrary(hKernel32);
			}
		}

		vehHandler = NULL;
		dwInitCount = 0;
	}
}
