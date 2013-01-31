// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <stdio.h>
#include <mferror.h>
#include <propvarutil.h>
#include <wmcodecdsp.h>
#include <Shlwapi.h>
#include <atlbase.h>
#include <strsafe.h>

#include <string>
#include <comdef.h>

_COM_SMARTPTR_TYPEDEF(IMFSourceReader, __uuidof(IMFSourceReader));
_COM_SMARTPTR_TYPEDEF(IMFMediaType, __uuidof(IMFMediaType));
_COM_SMARTPTR_TYPEDEF(IMFSample, __uuidof(IMFSample));
_COM_SMARTPTR_TYPEDEF(IMFMediaBuffer, __uuidof(IMFMediaBuffer));
_COM_SMARTPTR_TYPEDEF(IMFTransform, __uuidof(IMFTransform));
_COM_SMARTPTR_TYPEDEF(IMFAttributes, __uuidof(IMFAttributes));
_COM_SMARTPTR_TYPEDEF(IMFSinkWriter, __uuidof(IMFSinkWriter));
_COM_SMARTPTR_TYPEDEF(IMFByteStream, __uuidof(IMFByteStream));
_COM_SMARTPTR_TYPEDEF(IMFSourceResolver, __uuidof(IMFSourceResolver));
_COM_SMARTPTR_TYPEDEF(IMFAsyncResult, __uuidof(IMFAsyncResult));
_COM_SMARTPTR_TYPEDEF(IUnknown, __uuidof(IUnknown));
_COM_SMARTPTR_TYPEDEF(IStream, __uuidof(IStream));



// TODO: reference additional headers your program requires here
