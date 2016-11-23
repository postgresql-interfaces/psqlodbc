/* Module:			msdtc_enlist.cpp
 *
 * Description:
 *		This module contains routines related to
 *			the enlistment in MSDTC.
 *
 *-------
 */

#ifdef	_HANDLE_ENLIST_IN_DTC_

#undef	_MEMORY_DEBUG_
#ifndef	_WIN32_WINNT
#define	_WIN32_WINNT	0x0400
#endif	/* _WIN32_WINNT */

#define	WIN32_LEAN_AND_MEAN
#include <oleTx2xa.h>
#include <XOLEHLP.h>
/*#include <Txdtc.h>*/
#define	_PGDTC_FUNCS_IMPORT_
#include "connexp.h"

/*#define	_SLEEP_FOR_TEST_*/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <process.h>
#include <map>
#ifndef	WIN32
#include <errno.h>
#endif /* WIN32 */

#include <sql.h>
#define	_MYLOG_FUNCS_IMPORT_
#include "mylog.h"
#define	_PGENLIST_FUNCS_IMPLEMENT_
#include "pgenlist.h"
#include "xalibname.h"

#ifdef WIN32
#ifndef snprintf
#define snprintf _snprintf
#endif /* snprintf */
#endif /* WIN32 */

/* Define a type for defining a constant string expression */
#ifndef CSTR
#define CSTR static const char * const
#endif /* CSTR */

EXTERN_C {
HINSTANCE s_hModule;               /* Saved module handle. */
}
/*      This is where the Driver Manager attaches to this Driver */
BOOL    WINAPI
DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
        switch (ul_reason_for_call)
        {
                case DLL_PROCESS_ATTACH:
                        s_hModule = (HINSTANCE) hInst;  /* Save for dialog boxes */
			break;
                case DLL_PROCESS_DETACH:
                        mylog("DETACHING pgenlist\n");
			break;
	}
	return TRUE;
}

/*
 *	A comment About locks used in this module
 *
 *	  the locks should be acquired with stronger to weaker order.
 *
 *	1:ELOCK -- the strongest per IAsyncPG object lock
 *	  When the *isolated* or *dtcconn* member of an IAsyncPG object
 *	  is changed, this lock should be held.
 *	  While an IAsyncPG object accesses a psqlodbc connection,
 *	  this lock should be held.
 *
 *	2:[CONN_CS] -- per psqlodbc connection lock
 *	  This lock would be held for a pretty long time while accessing
 *	  the psqlodbc connection assigned to an IAsyncPG object. You
 *	  can use the connecion safely by holding a ELOCK for the
 *	  IAsyncPG object because the assignment is ensured to be
 *	  fixed while the ELOCK is held.
 *
 *	3:LIFELOCK -- a global lock to ensure the lives of IAsyncPG objects
 *	  While this lock is held, IAsyncPG objects would never die.
 *
 *	4:SLOCK -- the short term per IAsyncPG object lock
 *	  When any member of an IAsyncPG object is changed, this lock
 *	  should be held.
 */

// #define	_LOCK_DEBUG_
static class INIT_CRIT
{
public:
	CRITICAL_SECTION	life_cs; /* for asdum member of ConnectionClass */
	INIT_CRIT() {
		InitializeCriticalSection(&life_cs);
		}
	~INIT_CRIT() {
			DeleteCriticalSection(&life_cs);
			}
} init_crit;
#define	LIFELOCK_ACQUIRE EnterCriticalSection(&init_crit.life_cs)
#define	LIFELOCK_RELEASE LeaveCriticalSection(&init_crit.life_cs)

/*
 *	Some helper macros about connection handling.
 */
#define	CONN_CS_ACQUIRE(conn)	PgDtc_lock_cntrl((conn), TRUE, FALSE)
#define	TRY_CONN_CS_ACQUIRE(conn) PgDtc_lock_cntrl((conn), TRUE, TRUE)
#define	CONN_CS_RELEASE(conn)	PgDtc_lock_cntrl((conn), FALSE, FALSE)

#define	CONN_IS_IN_TRANS(conn)	PgDtc_get_property((conn), inTrans)


static const char *XidToText(const XID &xid, char *rtext)
{
	int	glen = xid.gtrid_length, blen = xid.bqual_length;
	int	i, j;

	for (i = 0, j = 0; i < glen; i++, j += 2)
		sprintf(rtext + j, "%02x", (unsigned char) xid.data[i]);
	strcat(rtext, "-"); j++;
	for (; i < glen + blen; i++, j += 2)
		sprintf(rtext + j, "%02x", (unsigned char) xid.data[i]);
	return rtext;
}

static LONG	g_cComponents = 0;
static LONG	g_cServerLocks = 0;

//
//	以下のITransactionResourceAsyncオブジェクトは任意のスレッドから
//	自由にアクセス可能なように実装する。各Requestの結果を返すために
//	使用するITransactionEnlistmentAsyncインターフェイスもそのように
//	実装されている（と思われる、下記参照）ので呼び出しにCOMのアパー
//	トメントを意識する(CoMarshalInterThreadInterfaceInStream/CoGetIn
//	terfaceAndReleaseStreamを使用する）必要はない。
//	このDLL内で使用するITransactionResourceAsyncとITransactionEnlist
//	mentAsyncのインターフェイスポインターは任意のスレッドから直接使用
//	することができる。
//

// OLE Transactions Standard
//
// OLE Transactions is the Microsoft interface standard for transaction
// management. Applications use OLE Transactions-compliant interfaces to
// initiate, commit, abort, and inquire about transactions. Resource
// managers use OLE Transactions-compliant interfaces to enlist in
// transactions, to propagate transactions to other resource managers,
// to propagate transactions from process to process or from system to
// system, and to participate in the two-phase commit protocol.
//
// The Microsoft DTC system implements most OLE Transactions-compliant
// objects, interfaces, and methods. Resource managers that wish to use
// OLE Transactions must implement some OLE Transactions-compliant objects,
// interfaces, and methods.
//
// The OLE Transactions specification is based on COM but it differs in the
// following respects:
//
// OLE Transactions objects cannot be created using the COM CoCreate APIs.
// References to OLE Transactions objects are always direct. Therefore,
// no proxies or stubs are created for inter-apartment, inter-process,
// or inter-node calls and OLE Transactions references cannot be marshaled
// using standard COM marshaling.
// All references to OLE Transactions objects and their sinks are completely
// free threaded and cannot rely upon COM concurrency control models.
// For example, you cannot pass a reference to an IResourceManagerSink
// interface on a single-threaded apartment and expect the callback to occur
// only on the same single-threaded apartment.

class	IAsyncPG : public ITransactionResourceAsync
{
private:
	IDtcToXaHelperSinglePipe	*helper;
	DWORD				RMCookie;
	void				*dtcconn;
	LONG				refcnt;
	CRITICAL_SECTION		as_spin; // to make this object Both
	CRITICAL_SECTION		as_exec; // to make this object Both
	XID				xid;
	bool				isolated;
	bool				prepared;
	bool				done;
	bool				abort;
	HANDLE				eThread[3];
	bool				eFin[3];
	bool				requestAccepted;
	HRESULT				prepare_result;
	HRESULT				commit_result;
#ifdef	_LOCK_DEBUG_
	int				spin_cnt;
	int				cs_cnt;
#endif /* _LOCK_DEBUG_ */

public:
	enum {
		PrepareExec = 0
		,CommitExec
		,AbortExec
		};

	ITransactionEnlistmentAsync	*enlist;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** ppvObject);
	ULONG	STDMETHODCALLTYPE AddRef(void);
	ULONG	STDMETHODCALLTYPE Release(void);

	HRESULT STDMETHODCALLTYPE PrepareRequest(BOOL fRetaining,
				DWORD grfRM,
				BOOL fWantMoniker,
				BOOL fSinglePhase);
	HRESULT STDMETHODCALLTYPE CommitRequest(DWORD grfRM, XACTUOW * pNewUOW);
	HRESULT STDMETHODCALLTYPE AbortRequest(BOID * pboidReason,
				BOOL fRetaining,
				XACTUOW * pNewUOW);
	HRESULT STDMETHODCALLTYPE TMDown(void);

	IAsyncPG();
	void SetHelper(IDtcToXaHelperSinglePipe *pHelper, DWORD dwRMCookie) {helper = pHelper; RMCookie = dwRMCookie;}

	HRESULT RequestExec(DWORD type, HRESULT res);
	HRESULT ReleaseConnection(void);
	void SetConnection(void *sconn) {SLOCK_ACQUIRE(); dtcconn = sconn; SLOCK_RELEASE();}
	void SetXid(const XID *ixid) {SLOCK_ACQUIRE(); xid = *ixid; SLOCK_RELEASE();}
	void	*separateXAConn(bool spinAcquired, bool continueConnection);
	bool CloseThread(DWORD type);
private:
	~IAsyncPG();
	void SLOCK_ACQUIRE() {EnterCriticalSection(&as_spin);}
	void SLOCK_RELEASE() {LeaveCriticalSection(&as_spin);}
	void ELOCK_ACQUIRE() {EnterCriticalSection(&as_exec);}
	void ELOCK_RELEASE() {LeaveCriticalSection(&as_exec);}
	void	*getLockedXAConn(void);
	void	*generateXAConn(bool spinAcquired);
	void	*isolateXAConn(bool spinAcquired, bool continueConnection);
	void SetPrepareResult(HRESULT res) {SLOCK_ACQUIRE(); prepared = true; prepare_result = res; SLOCK_RELEASE();}
	void SetDone(HRESULT);
	void Wait_pThread(bool slock_hold);
	void Wait_cThread(bool slock_hold, bool once);
};


IAsyncPG::IAsyncPG(void) : helper(NULL), RMCookie(0), enlist(NULL), dtcconn(NULL), refcnt(1), isolated(false), done(false), abort(false), prepared(false), requestAccepted(false)
{
	InterlockedIncrement(&g_cComponents);
	InitializeCriticalSection(&as_spin);
	InitializeCriticalSection(&as_exec);
	eThread[0] = eThread[1] = eThread[2] = NULL;
	eFin[0] = eFin[1] = eFin[2] = false;
	memset(&xid, 0, sizeof(xid));
#ifdef	_LOCK_DEBUG_
	spin_cnt = 0;
	cs_cnt = 0;
#endif /* _LOCK_DEBUG_ */
}

//
//	invoked from *delete*.
//	When entered ELOCK -> LIFELOCK -> SLOCK are held
//	and they are released.
//
IAsyncPG::~IAsyncPG(void)
{
	void *fconn = NULL;

	if (dtcconn)
	{
		if (isolated)
			fconn = dtcconn;
		PgDtc_set_async(dtcconn, NULL);
		dtcconn = NULL;
	}
	SLOCK_RELEASE();
	LIFELOCK_RELEASE;
	if (fconn)
	{
		mylog("IAsyncPG Destructor is freeing the connection\n");
		PgDtc_free_connect(fconn);
	}
	DeleteCriticalSection(&as_spin);
	ELOCK_RELEASE();
	DeleteCriticalSection(&as_exec);
	InterlockedDecrement(&g_cComponents);
}
HRESULT STDMETHODCALLTYPE IAsyncPG::QueryInterface(REFIID riid, void ** ppvObject)
{
	mylog("%p QueryInterface called\n", this);
	if (riid == IID_IUnknown || riid == IID_ITransactionResourceAsync)
	{
		*ppvObject = this;
		AddRef();
		return S_OK;
	}
	*ppvObject = NULL;
	return E_NOINTERFACE;
}
//
//	acquire/releases SLOCK.
//
ULONG	STDMETHODCALLTYPE IAsyncPG::AddRef(void)
{
	mylog("%p->AddRef called\n", this);
	SLOCK_ACQUIRE();
	refcnt++;
	SLOCK_RELEASE();
	return refcnt;
}
//
//	acquire/releases [ELOCK -> LIFELOCK -> ] SLOCK.
//
ULONG	STDMETHODCALLTYPE IAsyncPG::Release(void)
{
	mylog("%p->Release called refcnt=%d\n", this, refcnt);
	SLOCK_ACQUIRE();
	refcnt--;
	if (refcnt <= 0)
	{
		SLOCK_RELEASE();
		ELOCK_ACQUIRE();
		LIFELOCK_ACQUIRE;
		SLOCK_ACQUIRE();
		if (refcnt <=0)
		{
			const int refcnt_copy = refcnt;
			mylog("delete %p\n", this);
			delete this;
			return refcnt_copy;
		}
		else
		{
			SLOCK_RELEASE();
			LIFELOCK_RELEASE;
			ELOCK_RELEASE();
		}
	}
	else
		SLOCK_RELEASE();
	return refcnt;
}

//
//	Acquire/release SLOCK.
//
void IAsyncPG::Wait_pThread(bool slock_hold)
{
	mylog("Wait_pThread %d in\n", slock_hold);
	HANDLE	wThread;
	int	wait_idx = PrepareExec;
	DWORD	ret;

	if (!slock_hold)
		SLOCK_ACQUIRE();
	while (NULL != (wThread = eThread[wait_idx]) && !eFin[wait_idx])
	{
		SLOCK_RELEASE();
		ret = WaitForSingleObject(wThread, 2000);
		SLOCK_ACQUIRE();
		if (WAIT_TIMEOUT != ret)
			eFin[wait_idx] = true;
	}
	if (!slock_hold)
		SLOCK_RELEASE();
	mylog("Wait_pThread out\n");
}

//
//	Acquire/releases SLOCK.
//
void IAsyncPG::Wait_cThread(bool slock_hold, bool once)
{
	HANDLE	wThread;
	int	wait_idx;
	DWORD	ret;

	mylog("Wait_cThread %d,%d in\n", slock_hold, once);
	if (!slock_hold)
		SLOCK_ACQUIRE();
	if (NULL != eThread[CommitExec])
		wait_idx = CommitExec;
	else
		wait_idx = AbortExec;
	while (NULL != (wThread = eThread[wait_idx]) && !eFin[wait_idx])
	{
		SLOCK_RELEASE();
		ret = WaitForSingleObject(wThread, 2000);
		SLOCK_ACQUIRE();
		if (WAIT_TIMEOUT != ret)
			eFin[wait_idx] = true;
		else if (once)
			break;
	}
	if (!slock_hold)
		SLOCK_RELEASE();
	mylog("Wait_cThread out\n");
}

/* Processing Prepare/Commit Request */
typedef
struct RequestPara {
	DWORD	type;
	LPVOID	lpr;
	HRESULT	res;
} RequestPara;

//
//	Acquire/releases LIFELOCK -> SLOCK.
//	may acquire/release ELOCK.
//
void	IAsyncPG::SetDone(HRESULT res)
{
	LIFELOCK_ACQUIRE;
	SLOCK_ACQUIRE();
	done = true;
	if (E_FAIL == res ||
	    E_UNEXPECTED == res)
		abort = true;
	requestAccepted = true;
	commit_result = res;
	if (dtcconn)
	{
		PgDtc_set_async(dtcconn, NULL);
		if (isolated)
		{
			SLOCK_RELEASE();
			LIFELOCK_RELEASE;
			ELOCK_ACQUIRE();
			if (dtcconn)
			{
				mylog("Freeing isolated connection=%p\n", dtcconn);
				PgDtc_free_connect(dtcconn);
				SetConnection(NULL);
			}
			ELOCK_RELEASE();
		}
		else
		{
			dtcconn = NULL;
			SLOCK_RELEASE();
			LIFELOCK_RELEASE;
		}
	}
	else
	{
		SLOCK_RELEASE();
		LIFELOCK_RELEASE;
	}
}

//
//	Acquire/releases [ELOCK -> LIFELOCK -> ] SLOCK.
//
void	*IAsyncPG::generateXAConn(bool spinAcquired)
{
mylog("generateXAConn isolated=%d dtcconn=%p\n", isolated, dtcconn);
	if (!spinAcquired)
		SLOCK_ACQUIRE();
	if (isolated || done)
	{
		SLOCK_RELEASE();
		return dtcconn;
	}
	SLOCK_RELEASE();
	ELOCK_ACQUIRE();
	LIFELOCK_ACQUIRE;
	SLOCK_ACQUIRE();
	if (dtcconn && !isolated && !done && prepared)
	{
		void	*sconn = dtcconn;

		dtcconn = PgDtc_isolate(sconn, useAnotherRoom);
		isolated = true;
		SLOCK_RELEASE();
		LIFELOCK_RELEASE;
		// PgDtc_connect(dtcconn); may be called in getLockedXAConn
	}
	else
	{
		SLOCK_RELEASE();
		LIFELOCK_RELEASE;
	}
	ELOCK_RELEASE();
	return dtcconn;
}

//
//	Acquire/releases [ELOCK -> LIFELOCK -> ] SLOCK.
//
void	*IAsyncPG::isolateXAConn(bool spinAcquired, bool continueConnection)
{
	void *sconn;

mylog("isolateXAConn isolated=%d dtcconn=%p\n", isolated, dtcconn);
	if (!spinAcquired)
		SLOCK_ACQUIRE();
	if (isolated || done || NULL == dtcconn)
	{
		SLOCK_RELEASE();
		return dtcconn;
	}
	SLOCK_RELEASE();
	ELOCK_ACQUIRE();
	LIFELOCK_ACQUIRE;
	SLOCK_ACQUIRE();
	if (isolated || done || NULL == dtcconn)
	{
		SLOCK_RELEASE();
		LIFELOCK_RELEASE;
		ELOCK_RELEASE();
		return dtcconn;
	}
	sconn = dtcconn;

	dtcconn = PgDtc_isolate(sconn, continueConnection ? 0 : disposingConnection);

	isolated = true;
	SLOCK_RELEASE();
	LIFELOCK_RELEASE;
	if (continueConnection)
	{
		PgDtc_connect(sconn);
	}
	ELOCK_RELEASE();
	return dtcconn;
}

//
//	Acquire/releases [ELOCK -> LIFELOCK -> ] SLOCK.
//
void	*IAsyncPG::separateXAConn(bool spinAcquired, bool continueConnection)
{
	mylog("%s isolated=%d dtcconn=%p\n", __FUNCTION__, isolated, dtcconn);
	if (!spinAcquired)
		SLOCK_ACQUIRE();
	if (prepared)
		return generateXAConn(true);
	else
		return isolateXAConn(true, continueConnection);
}

//
//	[when entered]
//	ELOCK is held.
//
//	Acquire/releases SLOCK.
//	Try to acquire CONN_CS also.
//
//	[on exit]
//	ELOCK is kept held.
//	If the return connection != NULL
//		the CONN_CS lock for the connection is held.
//
void	*IAsyncPG::getLockedXAConn()
{
	SLOCK_ACQUIRE();
	while (!done && !isolated && NULL != dtcconn)
	{
		/*
		 * Note that COMMIT/ROLLBACK PREPARED command should be
		 * issued outside the transaction.
		 */
		if (!prepared || !CONN_IS_IN_TRANS(dtcconn))
		{
			if (TRY_CONN_CS_ACQUIRE(dtcconn))
			{
				if (prepared && CONN_IS_IN_TRANS(dtcconn))
				{
					CONN_CS_RELEASE(dtcconn);
				}
				else
					break;
			}
		}
		separateXAConn(true, true);
		SLOCK_ACQUIRE(); // SLOCK was released by separateXAConn()
	}
	SLOCK_RELEASE();
	if (isolated && NULL != dtcconn)
	{
		CONN_CS_ACQUIRE(dtcconn);
		if (!PgDtc_get_property(dtcconn, connected))
			PgDtc_connect(dtcconn);
	}
	return dtcconn;
}

//
//	Acquire/release ELOCK -> SLOCK.
//
HRESULT IAsyncPG::RequestExec(DWORD type, HRESULT res)
{
	HRESULT		ret;
	bool		bReleaseEnlist = false;
	void	*econn;
	char		pgxid[258];

	mylog("%p->RequestExec type=%d conn=%p\n", this, type, dtcconn);
	XidToText(xid, pgxid);
#ifdef	_SLEEP_FOR_TEST_
	/*Sleep(2000);*/
#endif	/* _SLEEP_FOR_TEST_ */
	ELOCK_ACQUIRE();
	switch (type)
	{
		case PrepareExec:
			if (done || NULL == dtcconn)
			{
				res = E_UNEXPECTED;
				break;
			}
			if (econn = getLockedXAConn(), NULL != econn)
			{
				PgDtc_set_property(econn, inprogress, (void *) 1);
				if (E_FAIL == res)
					PgDtc_one_phase_operation(econn, ABORT_GLOBAL_TRANSACTION);
				else if (XACT_S_SINGLEPHASE == res)
				{
					if (!PgDtc_one_phase_operation(econn, ONE_PHASE_COMMIT))
						res = E_FAIL;
				}
				else
				{
					if (!PgDtc_two_phase_operation(econn, PREPARE_TRANSACTION, pgxid))
						res = E_FAIL;
				}
				PgDtc_set_property(econn, inprogress, (void *) 0);
				CONN_CS_RELEASE(econn);
			}
			if (S_OK != res)
			{
				SetDone(res);
				bReleaseEnlist = true;
			}
			ret = enlist->PrepareRequestDone(res, NULL, NULL);
			SetPrepareResult(res);
			break;
		case CommitExec:
			Wait_pThread(false);
			if (E_FAIL != res)
			{
				econn = getLockedXAConn();
				if (econn)
				{
					PgDtc_set_property(econn, inprogress, (void *) 1);
					if (!PgDtc_two_phase_operation(econn, COMMIT_PREPARED, pgxid))
						res = E_FAIL;
					PgDtc_set_property(econn, inprogress, (void *) 0);
					CONN_CS_RELEASE(econn);
				}
			}
			SetDone(res);
			ret = enlist->CommitRequestDone(res);
			bReleaseEnlist = true;
			break;
		case AbortExec:
			Wait_pThread(false);
			if (prepared && !done)
			{
				econn = getLockedXAConn();
				if (econn)
				{
					PgDtc_set_property(econn, inprogress, (void *) 1);
					if (!PgDtc_two_phase_operation(econn, ROLLBACK_PREPARED, pgxid))
						res = E_FAIL;
					PgDtc_set_property(econn, inprogress, (void *) 0);
					CONN_CS_RELEASE(econn);
				}
			}
			SetDone(res);
			ret = enlist->AbortRequestDone(res);
			bReleaseEnlist = true;
			break;
		default:
			ret = -1;
	}
	if (bReleaseEnlist)
	{
		helper->ReleaseRMCookie(RMCookie, TRUE);
		enlist->Release();
	}
	ELOCK_RELEASE();
	mylog("%p->Done ret=%d\n", this, ret);
	return ret;
}

//
//	Acquire/releses SLOCK
//	 	or 	[ELOCK -> LIFELOCK -> ] SLOCK.
//
HRESULT IAsyncPG::ReleaseConnection(void)
{
	mylog("%p->ReleaseConnection\n", this);

	SLOCK_ACQUIRE();
	if (isolated || NULL == dtcconn)
	{
		SLOCK_RELEASE();
		return SQL_SUCCESS;
	}
	Wait_pThread(true);
	if (NULL != eThread[CommitExec] || NULL != eThread[AbortExec] || requestAccepted)
	{
		if (!done)
			Wait_cThread(true, true);
	}
	if (!isolated && !done && dtcconn && PgDtc_get_property(dtcconn, connected))
	{
		isolateXAConn(true, false);
	}
	else
		SLOCK_RELEASE();
	mylog("%p->ReleaseConnection exit\n", this);
	return SQL_SUCCESS;
}

EXTERN_C static unsigned WINAPI DtcRequestExec(LPVOID para);
EXTERN_C static void __cdecl ClosePrepareThread(LPVOID para);
EXTERN_C static void __cdecl CloseCommitThread(LPVOID para);
EXTERN_C static void __cdecl CloseAbortThread(LPVOID para);

//
//	Acquire/release [ELOCK -> ] SLOCK.
//
HRESULT STDMETHODCALLTYPE IAsyncPG::PrepareRequest(BOOL fRetaining, DWORD grfRM,
				BOOL fWantMoniker, BOOL fSinglePhase)
{
	HRESULT	ret, res;
	RequestPara	*reqp;
	const DWORD	reqtype = PrepareExec;

	mylog("%p PrepareRequest called grhRM=%d enl=%p\n", this, grfRM, enlist);
	SLOCK_ACQUIRE();
	if (dtcconn && 0 != PgDtc_get_property(dtcconn, errorNumber))
		res = ret = E_FAIL;
	else
	{
		ret = S_OK;
		if (fSinglePhase)
		{
			res = XACT_S_SINGLEPHASE;
			mylog("XACT is singlePhase\n");
		}
		else
			res = S_OK;
	}
	SLOCK_RELEASE();
	ELOCK_ACQUIRE();
#ifdef	_SLEEP_FOR_TEST_
	Sleep(2000);
#endif	/* _SLEEP_FOR_TEST_ */
	reqp = new RequestPara;
	reqp->type = reqtype;
	reqp->lpr = (LPVOID) this;
	reqp->res = res;
#define	DONT_CALL_RETURN_FROM_HERE ???
	AddRef();
	HANDLE hThread = (HANDLE) _beginthreadex(NULL, 0, DtcRequestExec, reqp, 0, NULL);
	if (NULL == hThread)
	{
		delete(reqp);
		ret = E_FAIL;
	}
	else
	{
		SLOCK_ACQUIRE();
		eThread[reqtype] = hThread;
		SLOCK_RELEASE();
		/*
		 * We call here _beginthread not _beginthreadex
		 * so as not to call CloseHandle() to clean up
		 * the thread.
		 */
		_beginthread(ClosePrepareThread, 0, (void *) this);
	}
	ELOCK_RELEASE();
	Release();
#undef	return
	return ret;
}
//
//	Acquire/release [ELOCK -> ] SLOCK.
//
HRESULT STDMETHODCALLTYPE IAsyncPG::CommitRequest(DWORD grfRM, XACTUOW * pNewUOW)
{
	HRESULT		res = S_OK, ret = S_OK;
	RequestPara	*reqp;
	const DWORD	reqtype = CommitExec;

	mylog("%p CommitRequest called grfRM=%d enl=%p\n", this, grfRM, enlist);

	SLOCK_ACQUIRE();
	if (!prepared || done)
		ret = E_UNEXPECTED;
	else if (S_OK != prepare_result)
		ret = E_UNEXPECTED;
	SLOCK_RELEASE();
	if (S_OK != ret)
		return ret;
#define	DONT_CALL_RETURN_FROM_HERE ???
	AddRef();
	ELOCK_ACQUIRE();
#ifdef	_SLEEP_FOR_TEST_
	Sleep(1000);
#endif	/* _SLEEP_FOR_TEST_ */
	reqp = new RequestPara;
	reqp->type = reqtype;
	reqp->lpr = (LPVOID) this;
	reqp->res = res;
	enlist->AddRef();
	HANDLE hThread = (HANDLE) _beginthreadex(NULL, 0, DtcRequestExec, reqp, 0, NULL);
	if (NULL == hThread)
	{
		delete(reqp);
		enlist->Release();
		ret = E_FAIL;
	}
	else
	{
		SLOCK_ACQUIRE();
		eThread[reqtype] = hThread;
		SLOCK_RELEASE();
		/*
		 * We call here _beginthread not _beginthreadex
		 * so as not to call CloseHandle() to clean up
		 * the thread.
		 */
		_beginthread(CloseCommitThread, 0, (void *) this);
	}
	mylog("CommitRequest ret=%d\n", ret);
	requestAccepted = true;
	ELOCK_RELEASE();
	Release();
#undef	return
	return ret;
}
//
//	Acquire/release [ELOCK -> ] SLOCK.
//
HRESULT STDMETHODCALLTYPE IAsyncPG::AbortRequest(BOID * pboidReason, BOOL fRetaining,
				XACTUOW * pNewUOW)
{
	HRESULT		res = S_OK, ret = S_OK;
	RequestPara	*reqp;
	const DWORD	reqtype = AbortExec;

	mylog("%p AbortRequest called\n", this);
	SLOCK_ACQUIRE();
	if (done)
		ret = E_UNEXPECTED;
	else if (prepared && S_OK != prepare_result)
		ret = E_UNEXPECTED;
	SLOCK_RELEASE();
	if (S_OK != ret)
		return ret;
#define	return	DONT_CALL_RETURN_FROM_HERE ???
	AddRef();
	ELOCK_ACQUIRE();
	if (!prepared && dtcconn)
	{
		PgDtc_set_property(dtcconn, inprogress, (void *) 1);
		PgDtc_one_phase_operation(dtcconn, ONE_PHASE_ROLLBACK);
		PgDtc_set_property(dtcconn, inprogress, (void *) 0);
	}
	reqp = new RequestPara;
	reqp->type = reqtype;
	reqp->lpr = (LPVOID) this;
	reqp->res = res;
	enlist->AddRef();
	HANDLE hThread = (HANDLE) _beginthreadex(NULL, 0, DtcRequestExec, reqp, 0, NULL);
	if (NULL == hThread)
	{
		delete(reqp);
		enlist->Release();
		ret = E_FAIL;
	}
	else
	{
		SLOCK_ACQUIRE();
		eThread[reqtype] = hThread;
		SLOCK_RELEASE();
		/*
		 * We call here _beginthread not _beginthreadex
		 * so as not to call CloseHandle() to clean up
		 * the thread.
		 */
		_beginthread(CloseAbortThread, 0, (void *) this);
	}
	mylog("AbortRequest ret=%d\n", ret);
	requestAccepted = true;
	ELOCK_RELEASE();
	Release();
#undef	return
	return	ret;
}
HRESULT STDMETHODCALLTYPE IAsyncPG::TMDown(void)
{
	mylog("%p TMDown called\n", this);
	return	S_OK;
}

bool IAsyncPG::CloseThread(DWORD type)
{
	CSTR		func = "CloseThread";
	HANDLE		th;
	DWORD		ret, excode = S_OK;
	bool		rls_async = false;

	mylog("%s for %p thread=%d\n", func, this, eThread[type]);
	if (th = eThread[type], NULL == th || eFin[type])
		return false;
	ret = WaitForSingleObject(th, INFINITE);
	if (WAIT_OBJECT_0 == ret)
	{
		switch (type)
		{
			case IAsyncPG::AbortExec:
			case IAsyncPG::CommitExec:
				rls_async = true;
				break;
			default:
				GetExitCodeThread(th, &excode);
				if (S_OK != excode)
					rls_async = true;
		}
		SLOCK_ACQUIRE();
		eThread[type] = NULL;
		eFin[type] = true;
		SLOCK_RELEASE();
		CloseHandle(th);
	}
	mylog("%s ret=%d\n", func, ret);
	return rls_async;
}

EXTERN_C static void __cdecl ClosePrepareThread(LPVOID para)
{
	CSTR		func = "ClosePrepareThread";
	IAsyncPG	*async = (IAsyncPG *) para;
	bool		release;

	mylog("%s for %p", func, async);
	if (release = async->CloseThread(IAsyncPG::PrepareExec), release)
		async->Release();
	mylog("%s release=%d\n", func, release);
}

EXTERN_C static void __cdecl CloseCommitThread(LPVOID para)
{
	CSTR		func = "CloseCommitThread";
	IAsyncPG	*async = (IAsyncPG *) para;
	bool		release;

	mylog("%s for %p", func, async);
	if (release = async->CloseThread(IAsyncPG::CommitExec), release)
		async->Release();
	mylog("%s release=%d\n", func, release);
}

EXTERN_C static void __cdecl CloseAbortThread(LPVOID para)
{
	CSTR		func = "CloseAbortThread";
	IAsyncPG	*async = (IAsyncPG *) para;
	bool		release;

	mylog("%s for %p", func, async);
	if (release = async->CloseThread(IAsyncPG::AbortExec), release)
		async->Release();
	mylog("%s release=%d\n", func, release);
}

EXTERN_C static unsigned WINAPI DtcRequestExec(LPVOID para)
{
	RequestPara	*reqp = (RequestPara *) para;
	DWORD		type = reqp->type;
	IAsyncPG *async = (IAsyncPG *) reqp->lpr;
	HRESULT	res = reqp->res, ret;

	mylog("DtcRequestExec type=%d", reqp->type);
	delete(reqp);
	ret = async->RequestExec(type, res);
	mylog(" Done ret=%d\n", ret);
	return ret;
}

CSTR	regKey = "SOFTWARE\\Microsoft\\MSDTC\\XADLL";

static int regkeyCheck(const char *xalibname, const char *xalibpath)
{
	int	retcode = 0;
	LONG	ret;
	HKEY	sKey;
	DWORD	rSize;

	ret = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, regKey, 0, KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, &sKey);
	switch (ret)
	{
		case ERROR_SUCCESS:
			break;
		case ERROR_FILE_NOT_FOUND:
			ret = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, regKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &sKey, NULL);
			mylog("%s:CreateKeyEx ret=%d\n", __FUNCTION__, ret);
			break;
		default:
			mylog("%s:OpenKeyEx ret=%d\n", __FUNCTION__, ret);
	}
	if (ERROR_SUCCESS != ret)
		return -1;
	else
	{
		char keyval[1024];

		rSize = sizeof(keyval);
		switch (ret = ::RegQueryValueEx(sKey, xalibname, NULL, NULL, (LPBYTE) keyval, &rSize))
		{
			case ERROR_SUCCESS:
				if (rSize > 0)
				{
					if (0 == _stricmp(keyval, xalibpath))
						break;
					mylog("%s:XADLL value %s is different from %s\n", __FUNCTION__, keyval, xalibpath);
					if (IsWow64())
					{
						mylog("%s:avoid RegSetValue operation from wow64 process\n", __FUNCTION__);
						break;
					}
				}
			case ERROR_FILE_NOT_FOUND:
				mylog("%s:Setting value %s\n", __FUNCTION__, xalibpath);
				ret = ::RegSetValueEx(sKey, xalibname, 0, REG_SZ, (CONST BYTE *) xalibpath, (DWORD) strlen(xalibpath) + 1);
				if (ERROR_SUCCESS == ret)
					retcode = 1;
				else
				{
					retcode = -1;
					mylog("%s:SetValuEx ret=%d\n", __FUNCTION__, ret);
				}
				break;
			default:
				retcode = -1;
				mylog("%s:QueryValuEx ret=%d\n", __FUNCTION__, ret);
				break;
		}
		::RegCloseKey(sKey);
	}
	return retcode;
}

RETCODE static EnlistInDtc_1pipe(void *conn, ITransaction *pTra, ITransactionDispenser *pDtc, int method)
{
	CSTR	func = "EnlistInDtc_1pipe";
	static	IDtcToXaHelperSinglePipe	*pHelper = NULL;
	ITransactionResourceAsync		*pRes = NULL;
	IAsyncPG				*asdum;
	HRESULT	res;
	DWORD	dwRMCookie;
	XID	xid;
	const char *xalibname = GetXaLibName();
	const char *xalibpath = GetXaLibPath();

	int	recovLvl;
	char	errmsg[256];
	char	reason[128];

	if (!pHelper)
	{
		res = pDtc->QueryInterface(IID_IDtcToXaHelperSinglePipe, (void **) &pHelper);
		if (res != S_OK || !pHelper)
		{
			mylog("DtcToXaHelperSingelPipe get error %d\n", res);
			pHelper = NULL;
			return SQL_ERROR;
		}
	}
	res = (NULL != (asdum = new IAsyncPG)) ? S_OK : E_FAIL;
	if (S_OK != res)
	{
		mylog("CoCreateInstance error %d\n", res);
		return SQL_ERROR;
	}

	recovLvl = PgDtc_is_recovery_available(conn, reason, sizeof(reason));
	switch (method)
	{
		case DTC_CHECK_BEFORE_LINK:
			if (0 == recovLvl)
			{
				snprintf(errmsg, sizeof(errmsg), "%s is unavailable in distributed transactions", reason);
				PgDtc_set_error(conn, errmsg, func);
				return SQL_ERROR;
			}
	}
/*mylog("dllname=%s dsn=%s\n", xalibname, conn->connInfo.dsn); res = 0;*/
	char	dtcname[1024];
	PgDtc_create_connect_string(conn, dtcname, sizeof(dtcname));

	bool	confirmedRegkey = false, confirmingLink = false, xarmerr = false;
	char	error_header[64];
	while (true)
	{
		res = pHelper->XARMCreate(dtcname, (char *) xalibname, &dwRMCookie);

		mylog("XARMcreate error code=%x (%d %d)\n", res, confirmedRegkey, confirmingLink);
		xarmerr = true;
		if (!confirmingLink)
			snprintf(error_header, sizeof(error_header), "XARMcreate error code=%x", res);
		switch (res)
		{
			case S_OK:
				if (confirmingLink)
				{
					switch (recovLvl)
					{
						case 0:
							snprintf(errmsg, sizeof(errmsg), "%s:%s is currently unavailable in distributed transactions", error_header, reason);
							break;
						case -1:
							snprintf(errmsg, sizeof(errmsg), "%s:Possibly you connect to the database whose authentication method is %s or ident", error_header, reason);
							break;
						case 1:
							snprintf(errmsg, sizeof(errmsg), "%s:Are you trying to connect to the database whose authentication method is ident?", error_header);
							break;
					}
				}
				else
					xarmerr = false;
				break;
			case XACT_E_XA_TX_DISABLED:
				snprintf(errmsg, sizeof(errmsg), "%s:Please enable XA transaction in MSDTC security configuration", error_header);
				break;
			case XACT_E_TMNOTAVAILABLE:
				snprintf(errmsg, sizeof(errmsg), "%s:Please start Distributed Transaction Coordinator service", error_header);
				break;
			case E_FAIL:
				if (!confirmedRegkey)
				{
					int retcode = regkeyCheck(xalibname, xalibpath);
					confirmedRegkey = true;
					if (retcode > 0)
						continue;
				}
				switch (method)
				{
					case DTC_CHECK_RM_CONNECTION:
						if (!confirmingLink)
						{
							confirmingLink = true;
							strcat(dtcname, ";" KEYWORD_DTC_CHECK "=0");
							continue;
						}
					default:
						snprintf(errmsg, sizeof(errmsg), "%s:Failed to link with DTC service. Please look at the log of Event Viewer etc.", error_header);
				}
				break;
			case XACT_E_CONNECTION_DOWN:
				snprintf(errmsg, sizeof(errmsg), "%s:Lost connection with DTC transaction manager\nMSDTC has some trouble?", error_header);
				break;
			default:
				snprintf(errmsg, sizeof(errmsg), "%s\n", error_header);
				break;
		}
		break;
	}
	if (xarmerr)
	{
		PgDtc_set_error(conn, errmsg, func);
		return SQL_ERROR;
	}

	res = pHelper->ConvertTridToXID((DWORD *) pTra, dwRMCookie, &xid);
	if (res != S_OK)
	{
		mylog("ConvertTridToXid error %d\n", res);
		return SQL_ERROR;
	}
{
char	pgxid[258];
XidToText(xid, pgxid);
mylog("ConvertTridToXID -> %s\n", pgxid);
}
	asdum->SetXid(&xid);
	/* Create an IAsyncPG instance by myself */
	/* DLLGetClassObject(GUID_IAsyncPG, IID_ITransactionResourceAsync, (void **) &asdum); */

	asdum->SetHelper(pHelper, dwRMCookie);
	res = pHelper->EnlistWithRM(dwRMCookie, pTra, asdum, &asdum->enlist);
	if (res != S_OK)
	{
		mylog("EnlistWithRM error %d\n", res);
		pHelper->ReleaseRMCookie(dwRMCookie, TRUE);
		return SQL_ERROR;
	}

	mylog("asdum=%p start transaction\n", asdum);
	asdum->SetConnection(conn);
	LIFELOCK_ACQUIRE;
	PgDtc_set_async(conn, asdum);
	LIFELOCK_RELEASE;

	return 	SQL_SUCCESS;
}


EXTERN_C RETCODE
IsolateDtcConn(void *conn, BOOL continueConnection)
{
	IAsyncPG *async;

	LIFELOCK_ACQUIRE;
	if (async = (IAsyncPG *) PgDtc_get_async(conn), NULL != async)
	{
		if (PgDtc_get_property(conn, idleInGlobalTransaction))
		{
			async->AddRef();
			LIFELOCK_RELEASE;
			async->separateXAConn(false, continueConnection ? true : false);
			async->Release();
		}
		else
			LIFELOCK_RELEASE;
	}
	else
		LIFELOCK_RELEASE;
	return SQL_SUCCESS;
}


static ITransactionDispenser *getITransactionDispenser(DWORD grfOptions, HRESULT *hres)
{
	static	ITransactionDispenser	*pDtc = NULL;
	HRESULT	res = S_OK;

	if (!pDtc)
	{
		res = DtcGetTransactionManagerEx(NULL, NULL, IID_ITransactionDispenser,
			
			grfOptions, NULL, (void **) &pDtc);
		if (FAILED(res))
		{
			mylog("DtcGetTransactionManager error %x\n", res);
			pDtc = NULL;
		}
	}
	if (hres) 
		*hres = res;

	return pDtc;
}

EXTERN_C void	*GetTransactionObject(HRESULT *hres)
{
	
	ITransaction	*pTra = NULL;
	ITransactionDispenser	*pDtc = NULL;

	if (pDtc = getITransactionDispenser(OLE_TM_FLAG_NONE, hres), NULL == pDtc)
		return pTra;
	HRESULT res = pDtc->BeginTransaction(NULL, ISOLATIONLEVEL_READCOMMITTED,
		0, NULL, &pTra);
	switch (res)
	{
		case S_OK:
			break;
		default:
			pTra = NULL;
	}
	if (hres)
		*hres = res;
	return pTra; 
}

EXTERN_C void	ReleaseTransactionObject(void *pObj)
{
	ITransaction	*pTra = (ITransaction *) pObj;

	if (!pTra)	return;
	pTra->Release();
}

EXTERN_C RETCODE EnlistInDtc(void *conn, void *pTra, int method)
{
	ITransactionDispenser	*pDtc = NULL;
	RETCODE	ret;

	if (!pTra)
	{
		IAsyncPG *asdum = (IAsyncPG *) PgDtc_get_async(conn);
		PgDtc_set_property(conn, enlisted, (void *) 0);
		return SQL_SUCCESS;
	}
	if (CONN_IS_IN_TRANS(conn))
	{
		PgDtc_one_phase_operation(conn, SHUTDOWN_LOCAL_TRANSACTION);
	}
	HRESULT	hres;
	pDtc = getITransactionDispenser(OLE_TM_FLAG_NODEMANDSTART, &hres);
	if (!pDtc)
	{
		char	errmsg[128];
		snprintf(errmsg, sizeof(errmsg), "enlistment error:DtcGetTransactionManager error code=%x", hres);
		PgDtc_set_error(conn, errmsg, __FUNCTION__);
		return SQL_ERROR;
	}
	ret = EnlistInDtc_1pipe(conn, (ITransaction *) pTra, pDtc, method);
	if (SQL_SUCCEEDED(ret))
		PgDtc_set_property(conn, enlisted, (void *) 1);
	return ret;
}

EXTERN_C RETCODE DtcOnDisconnect(void *conn)
{
	mylog("DtcOnDisconnect\n");
	LIFELOCK_ACQUIRE;
	IAsyncPG *asdum = (IAsyncPG *) PgDtc_get_async(conn);
	if (asdum)
	{
		asdum->AddRef();
		LIFELOCK_RELEASE;
		asdum->ReleaseConnection();
		asdum->Release();
	}
	else
		LIFELOCK_RELEASE;
	return SQL_SUCCESS;
}

#endif /* _HANDLE_ENLIST_IN_DTC_ */
