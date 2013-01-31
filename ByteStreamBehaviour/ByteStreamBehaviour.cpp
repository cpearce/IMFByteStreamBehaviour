// ByteStreamBehaviour.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mf.lib")

using std::wstring;

wstring hrToWStr(HRESULT hr)
{
  _com_error err(hr);
  LPCTSTR errMsg = err.ErrorMessage();
  return wstring(errMsg);
}

void DBGMSG(PCWSTR format, ...)
{
  va_list args;
  va_start(args, format);
  WCHAR msg[MAX_PATH];
  if (SUCCEEDED(StringCbVPrintf(msg, sizeof(msg), format, args))) {
    OutputDebugString(msg);
  }
}

#define ENSURE_SUCCESS(hr, ret) \
{ if (FAILED(hr)) { DBGMSG(L"FAILED: hr=0x%x (%S) %S:%d\n", hr, hrToWStr(hr).c_str(), __FILE__, __LINE__); return ret; } }

#define ENSURE_TRUE(condition, ret) \
{ if (!(condition)) { DBGMSG(L"FAILED: %S:%d\n", __FILE__, __LINE__); return ret; } }




class Lock {
  friend class AutoLock;
public:
  Lock() {
    InitializeCriticalSection(&critSec);
  }
  ~Lock() {
    DeleteCriticalSection(&critSec);
  }
private:
  CRITICAL_SECTION critSec;
};

class AutoLock {
public:
  AutoLock(Lock& lock) : mLock(lock) {
    EnterCriticalSection(&mLock.critSec);
  }
  ~AutoLock() {
    LeaveCriticalSection(&mLock.critSec);
  }  
private:
  Lock& mLock;
};

class ReadCallback : public IMFAsyncCallback 
{
public:

  ReadCallback () : m_cRef(1) {
    AutoLock autoLock(lock);
    result = nullptr;
    mReadEvent = CreateEvent(NULL, TRUE, FALSE, L"readEvent");
    ResetEvent(mReadEvent);
    if (!mReadEvent) {
      printf("CreateEvent failed\n");
      exit(-1);
    }
    printf("ReadCallback CTOR\n");
  }
  
  virtual ~ReadCallback() {
    AutoLock autoLock(lock);
    CloseHandle(mReadEvent);
    printf("ReadCallback DTOR\n");
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv){
    static const QITAB qit[] = {
      QITABENT(ReadCallback, IMFAsyncCallback),
      { 0 }
    };
    return QISearch(this, qit, riid, ppv);
  }

  STDMETHODIMP_(ULONG) AddRef() {
    return InterlockedIncrement(&m_cRef);
  }

  STDMETHODIMP_(ULONG) Release() {
    long cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) {
      delete this;
    }
    return cRef;
  }

  STDMETHODIMP GetParameters(DWORD* pdwFlags, DWORD* pdwQueue) {
    // Implementation of this method is optional.
    return E_NOTIMPL;
  }

  IMFAsyncResult* GetResult() {
    AutoLock autoLock(lock);
    return result;
  }

  STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult) {
    AutoLock autoLock(lock);
    result = pAsyncResult;
    printf("ReadCallback called status=0x%x result=0x%p\n", result->GetStatus(), result);
    SetEvent(mReadEvent);
    return S_OK;
  }

  HRESULT Wait() {
    return WaitForSingleObject(mReadEvent, INFINITE);
  }

  // Inside Invoke, IMFAsyncResult::GetStatus to get the status.
  // Then call the EndX method to complete the operation. 

private:
  HANDLE mReadEvent;

  // Protects cross thread access to result.
  Lock lock;

  IMFAsyncResultPtr result;
  long    m_cRef;

};

QWORD GetCurrentPosition(IMFByteStream* stream) {
  QWORD pos = 0;
  HRESULT hr = stream->GetCurrentPosition(&pos);
  if (hr != S_OK) {
    printf("GetCurrentPosition returned 0x%x\n", hr);
  }
  return pos;
}

bool IsEof(IMFByteStream* bs) {
  BOOL eof = false;
  bs->IsEndOfStream(&eof);
  return eof == TRUE;
}

void Read(IMFByteStream *bs, int length) {
  printf("\nRead %d bytes at pos=%lld eof=%d\n", length, GetCurrentPosition(bs), IsEof(bs));

  BYTE* buf = new BYTE[length];
  ENSURE_TRUE(buf != nullptr, );

  CComPtr<ReadCallback> callback = new ReadCallback();

  HRESULT hr = bs->BeginRead(buf, length, callback, nullptr);
  printf("BeginRead returned 0x%x (%S) pos=%lld\n", hr, hrToWStr(hr).c_str(), GetCurrentPosition(bs));
  if (FAILED(hr)) {
    printf("BeginRead failed, not waiting for callback!\n");
    delete buf;
    return;
  }

  callback->Wait();
  IMFAsyncResultPtr result = callback->GetResult();
  HRESULT readStatus = result ? result->GetStatus() : E_POINTER;
  ULONG bytesRead = 0;
  hr = bs->EndRead(result, &bytesRead);
  printf("EndRead ret=0x%x, result=0x%p result->GetStatus()=0x%x (%S), bytesRead=%u pos=%lld eof=%d\n",
         hr, result, readStatus, hrToWStr(readStatus).c_str(), bytesRead, GetCurrentPosition(bs), IsEof(bs));

  delete buf;  
}

void ReadSeek(IMFByteStream *bs, int length) {
  QWORD pos = GetCurrentPosition(bs);
  QWORD target = pos + 50;
  printf("\nReadSeek length=%d pos=%lld target=%lld\n", length, pos, target);
  const int buflen = 1024;
  BYTE buf[buflen];

  CComPtr<ReadCallback> callback = new ReadCallback();

  HRESULT hr = bs->BeginRead(buf, length, callback, nullptr);
  HRESULT hr2 = bs->SetCurrentPosition(target);
  printf("BeginRead returned 0x%x SetCurPos(pos/2) ret= 0x%x\n", hr, hr2);
  if (FAILED(hr)) {
    printf("BeginRead failed, not waiting for callback!\n");
    return;
  }

  callback->Wait();

  IMFAsyncResultPtr result = callback->GetResult();
  HRESULT readStatus = result ? result->GetStatus() : E_POINTER;
  ULONG bytesRead = 2;
  hr = bs->EndRead(result, &bytesRead);
  printf("EndRead ret=0x%x, result->GetStatus()=0x%x, bytesRead=%u pos=%lld eof=%d\n",
         hr, readStatus, bytesRead, GetCurrentPosition(bs), IsEof(bs));

}

void DoubleBeginRead(IMFByteStream* bs) {
  printf("\nDoubleBeginRead pos=%lld\n", GetCurrentPosition(bs));

  const int length = 10;
  const int buflen = 1024;
  BYTE buf[buflen];

  CComPtr<ReadCallback> callback1 = new ReadCallback();
  CComPtr<ReadCallback> callback2 = new ReadCallback();

  HRESULT hr = bs->BeginRead(buf, length, callback1, nullptr);
  printf("BeginRead 1 returned 0x%x\n", hr);
  if (FAILED(hr)) {
    printf("BeginRead 1 failed, not waiting for callback!\n");
    return;
  }

  hr = bs->BeginRead(buf, length, callback2, nullptr);
  printf("BeginRead 2 returned 0x%x\n", hr);
  if (FAILED(hr)) {
    printf("BeginRead 2 failed, not waiting for callback!\n");
    return;
  }

  hr = callback1->Wait();
  ENSURE_SUCCESS(hr, );
  hr = callback2->Wait();
  ENSURE_SUCCESS(hr, );

  {
    HRESULT readStatus = callback1->GetResult()->GetStatus();
    ULONG bytesRead = 2;
    hr = bs->EndRead(callback1->GetResult(), &bytesRead);
    printf("EndRead 1 ret=0x%x, result->GetStatus()=0x%x, bytesRead=%u pos=%lld eof=%d\n",
           hr, readStatus, bytesRead, GetCurrentPosition(bs), IsEof(bs));
  }
  {
    HRESULT readStatus = callback2->GetResult()->GetStatus();
    ULONG bytesRead = 2;
    hr = bs->EndRead(callback2->GetResult(), &bytesRead);
    printf("EndRead 2 ret=0x%x, result->GetStatus()=0x%x, bytesRead=%u pos=%lld eof=%d\n",
           hr, readStatus, bytesRead, GetCurrentPosition(bs), IsEof(bs));
  }
}


void DoubleBeginReadSeek(IMFByteStream* bs) {
  printf("\nDoubleBeginReadSeek pos=%lld\n", GetCurrentPosition(bs));

  const int length = 10;
  const int buflen = 1024;
  BYTE buf[buflen];

  CComPtr<ReadCallback> callback1 = new ReadCallback();
  CComPtr<ReadCallback> callback2 = new ReadCallback();

  HRESULT hr = bs->BeginRead(buf, length, callback1, nullptr);
  HRESULT hr2 = bs->BeginRead(buf, length, callback2, nullptr);
  HRESULT hr3 = bs->SetCurrentPosition(500);

  printf("BeginRead 1 returned 0x%x\n", hr);
  printf("BeginRead 2 returned 0x%x\n", hr2);
  printf("SetCurrentPosition(500) ret 0x%x pos=%lld\n", hr3, GetCurrentPosition(bs));

  if (FAILED(hr)) {
    printf("BeginRead 1 failed, not waiting for callback!\n");
    return;
  }
  if (FAILED(hr2)) {
    printf("BeginRead 2 failed, not waiting for callback!\n");
    return;
  }
  if (FAILED(hr3)) {
    printf("SetCurrentPosition failed, not waiting for callback!\n");
    return;
  }

  callback1->Wait();
  callback2->Wait();

  {
    HRESULT readStatus = callback1->GetResult()->GetStatus();
    ULONG bytesRead = 2;
    hr = bs->EndRead(callback1->GetResult(), &bytesRead);
    printf("EndRead 1 ret=0x%x, result=0x%x result->GetStatus()=0x%x, bytesRead=%u pos=%lld eof=%d\n",
           hr, callback1->GetResult(), readStatus, bytesRead, GetCurrentPosition(bs), IsEof(bs));
  }
  {
    HRESULT readStatus = callback2->GetResult() ? callback2->GetResult()->GetStatus() : E_FAIL;
    ULONG bytesRead = 2;
    hr = bs->EndRead(callback2->GetResult(), &bytesRead);
    printf("EndRead 2 ret=0x%x, result=0x%x result->GetStatus()=0x%x, bytesRead=%u pos=%lld eof=%d\n",
           hr, callback2->GetResult(), readStatus, bytesRead, GetCurrentPosition(bs), IsEof(bs));
  }
}

HRESULT
CreateByteStream(IMFByteStream** byteStream)
{
  IMFSourceResolverPtr resolver;
  HRESULT hr;

  hr = MFCreateSourceResolver(&resolver);
  ENSURE_SUCCESS(hr, hr);

  IMFByteStreamPtr bs;
  IUnknownPtr unknown;
  MF_OBJECT_TYPE objectType;
  hr = resolver->CreateObjectFromURL(L"http://pearce.org.nz/video/bruce_vs_ironman.mp4",
                                     MF_RESOLUTION_BYTESTREAM,
                                     NULL,
                                     &objectType,
                                     &unknown);
  ENSURE_SUCCESS(hr, hr);

  bs = unknown;
  ENSURE_TRUE(bs != nullptr, E_FAIL);
  
  *byteStream = bs.Detach();

  return S_OK;
}

int _tmain(int argc, _TCHAR* argv[])
{
  HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
  ENSURE_SUCCESS(hr, -1);

  hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  ENSURE_SUCCESS(hr, -1);

  IMFByteStreamPtr bs;

  /*
  const int bufLen = 1000;
  BYTE buf[bufLen ];
  memset(buf, 0xaa, bufLen);
  IStreamPtr stream = SHCreateMemStream(buf, bufLen);
  hr = MFCreateMFByteStreamOnStream(stream, &bs);
  ENSURE_SUCCESS(hr, -1);
  */

  hr = CreateByteStream(&bs);
  ENSURE_SUCCESS(hr, hr);

  DWORD caps = 0;
  hr = bs->GetCapabilities(&caps);
  ENSURE_SUCCESS(hr, -1);

  QWORD length = 0;
  hr = bs->GetLength(&length);
  ENSURE_SUCCESS(hr, -1);
  printf("Created IMFByteStream caps=0x%x length=%lld\n", caps, length);

  hr = bs->SetCurrentPosition(length - 10);
  printf("\nSeek to length-10. hr=0x%x pos=%lld\n", hr, GetCurrentPosition(bs));
  Read(bs, 20);


  hr = bs->SetCurrentPosition(2*length);
  printf("Seek after end hr=0x%x pos=%lld\n", hr, GetCurrentPosition(bs));


  Read(bs, 10);

  printf("Reading again\n");

  Read(bs, 10);

  hr = bs->SetCurrentPosition(length-30);
  printf("\nSeek just before end hr=0x%x pos=%lld correct=%d\n", hr, GetCurrentPosition(bs), GetCurrentPosition(bs)==(length-30));

  Read(bs, 10);
  Read(bs, 10);
  Read(bs, 10);
  Read(bs, 10);

  hr = bs->SetCurrentPosition(0);
  printf("\nSeek to 0. hr=0x%x pos=%lld\n", hr, GetCurrentPosition(bs));
  Read(bs, 10);

  ReadSeek(bs, 10);

  // Call BeginRead again before calling EndRead.
  DoubleBeginRead(bs);

  DoubleBeginReadSeek(bs);

  hr = bs->SetCurrentPosition(-100);
  printf("\nNegSeekRead seek=-100 pos=%lld hr=0x%x eof=%d\n", GetCurrentPosition(bs), hr, IsEof(bs));

  Read(bs, 0);

  Read(bs, 10);

  Read(bs, 190);

  Read(bs, 10);

  Read(bs, 0);

  hr = bs->SetCurrentPosition(length - 10);
  printf("\nSeek to length-10. hr=0x%x pos=%lld\n", hr, GetCurrentPosition(bs));
  Read(bs, 20);

  printf("\n\n Closing and re-creating stream\n");
  // Seeking to negative seems to poison the stream, re-create it for more tests.
  bs->Close();
  bs = nullptr;
  hr = CreateByteStream(&bs);
  ENSURE_SUCCESS(hr, hr);

  Read(bs, 500);

  const int buflen = 100000;
  BYTE* buf = new BYTE[buflen];

  CComPtr<ReadCallback> callback = new ReadCallback();
  hr = bs->BeginRead(buf, buflen, callback, nullptr);
  printf("BeginRead returned 0x%x (%S) pos=%lld\n", hr, hrToWStr(hr).c_str(), GetCurrentPosition(bs));
  if (FAILED(hr)) {
    printf("BeginRead failed, not waiting for callback!\n");
    return -1;
  }
  printf("Closing stream! :D\n");
  bs->Close();

  callback->Wait();
  IMFAsyncResultPtr result = callback->GetResult();
  HRESULT readStatus = result ? result->GetStatus() : E_POINTER;
  ULONG bytesRead = 0;
  hr = bs->EndRead(result, &bytesRead);
  printf("EndRead after closed stream: ret=0x%x, result=0x%p result->GetStatus()=0x%x (%S), bytesRead=%u pos=%lld eof=%d\n",
         hr, result, readStatus, hrToWStr(readStatus).c_str(), bytesRead, GetCurrentPosition(bs), IsEof(bs));


  printf("Closed stream eof=%d\n", IsEof(bs));
  Read(bs, 20);

  hr = bs->SetCurrentPosition(100);
  printf("\nSeek after read to 100 hr=0x%x pos=%lld\n", hr, GetCurrentPosition(bs));

  
  hr = bs->GetCapabilities(&caps);
  printf("GetCaps after closing returns hr=0x%x caps=0x%x\n", hr, caps);

  hr = bs->GetLength(&length);
  printf("GetLength after closing returns hr=0x%x length=%llu\n", hr, caps);

  printf("After closing eof=%d\n", IsEof(bs));


  MFShutdown();

  CoUninitialize();
	return 0;
}


/*

Behaviour determined by this test:
* Reads at EOF return S_OK, but 0 bytes read, and don't advance read cursor.
* SetCurrentPosition after end is clamped to end.
* BeginRead with a negative position set return 0x80070057 / E_INVALID_ARG.
* Reads starting before end but finishing after end are clamped at eof, and return S_OK. read cursor is also clamped.
* Seeks after closing still succeed.
* GetCaps is unchanged afte closing, but GetLength returns S_OK but  a garbage result.
* After closing EOF still returns false.
* Seeking to negative number and reading forever poisons the stream? The BeginRead starts, but end read reports E_INVALID_ARG.
* EndRead definintely returns the status from the read.
*/