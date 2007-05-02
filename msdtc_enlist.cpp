/*------
 * Module:			msdtc_enlist.cpp
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
#include "connection.h"

/*#define	_SLEEP_FOR_TEST_*/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <process.h>
#include <map>
#ifndef	WIN32
#include <errno.h>
#endif /* WIN32 */

#include "qresult.h"
#include "dlg_specific.h"

#include "pgapifunc.h"
#include "pgenlist.h"

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
                        s_hModule = (HINSTANCE) hInst;  /* Save for dialog boxes
 */
			break;
	}
	return TRUE;
}

static class INIT_CRIT
{
public:
	CRITICAL_SECTION	life_cs;
	CRITICAL_SECTION	map_cs;
	INIT_CRIT() {
		InitializeCriticalSection(&life_cs);
		InitializeCriticalSection(&map_cs);
		}
	~INIT_CRIT() {
			DeleteCriticalSection(&life_cs);
			DeleteCriticalSection(&map_cs);
			}
} init_crit;
#define	LIFELOCK_ACQUIRE EnterCriticalSection(&init_crit.life_cs)
#define	LIFELOCK_RELEASE LeaveCriticalSection(&init_crit.life_cs)
#define	MLOCK_ACQUIRE	EnterCriticalSection(&init_crit.map_cs)
#define	MLOCK_RELEASE	LeaveCriticalSection(&init_crit.map_cs)


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

/*#define	_LOCK_DEBUG_ */
class	IAsyncPG : public ITransactionResourceAsync
{
	friend class	AsyncThreads;
private:
	IDtcToXaHelperSinglePipe	*helper;
	DWORD				RMCookie;
	ConnectionClass			*conn;
	ConnectionClass			*xaconn;
	LONG				refcnt;
	CRITICAL_SECTION		as_spin; // to make this object Both
	CRITICAL_SECTION		as_exec; // to make this object Both
	XID				xid;
	bool				prepared;
	HANDLE				eThread[3];
	HRESULT				prepare_result;
	bool				requestAccepted;
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
	void SetConnection(ConnectionClass *sconn) {SLOCK_ACQUIRE(); conn = sconn; SLOCK_RELEASE();}
	void SetXid(const XID *ixid) {SLOCK_ACQUIRE(); xid = *ixid; SLOCK_RELEASE();}
private:
	~IAsyncPG();
#ifdef	_LOCK_DEBUG_
	void SLOCK_ACQUIRE() {forcelog("SLOCK_ACQUIRE %d\n", spin_cnt); EnterCriticalSection(&as_spin); spin_cnt++;}
	void SLOCK_RELEASE() {forcelog("SLOCK_RELEASE=%d\n", spin_cnt); LeaveCriticalSection(&as_spin); spin_cnt--;}
#else
	void SLOCK_ACQUIRE() {EnterCriticalSection(&as_spin);}
	void SLOCK_RELEASE() {LeaveCriticalSection(&as_spin);}
#endif /* _LOCK_DEBUG_ */
	void ELOCK_ACQUIRE() {EnterCriticalSection(&as_exec);}
	void ELOCK_RELEASE() {LeaveCriticalSection(&as_exec);}
	ConnectionClass	*getLockedXAConn(void);
	ConnectionClass	*generateXAConn(bool spinAcquired);
	void SetPrepareResult(HRESULT res) {SLOCK_ACQUIRE(); prepared = true; prepare_result = res; SLOCK_RELEASE();} 
	void SetDone(HRESULT);
	void Reset_eThread(int idx) {SLOCK_ACQUIRE(); eThread[idx] = NULL; SLOCK_RELEASE();}
	void Wait_pThread(bool slock_hold);
	void Wait_cThread(bool slock_hold, bool once);
};

//
//	For thread control.
//
class	AsyncWait {
private:
	IAsyncPG	*obj;
	DWORD		type;
	int		waiting_count;
public:
	AsyncWait(IAsyncPG *async, DWORD itype) : obj(async), type(itype), waiting_count(0) {}
	AsyncWait(const AsyncWait &a_th) : obj(a_th.obj), type(a_th.type), waiting_count(a_th.waiting_count) {}
	~AsyncWait()	{}
	IAsyncPG *GetObj()  const {return obj;}
	DWORD	GetType()  const {return type;}
	int	WaitCount()  const {return waiting_count;}
	int	StartWaiting() {return ++waiting_count;}
	int	StopWaiting() {return --waiting_count;}
};
//
//	List of threads invoked from IAsyncPG objects.
//
class	AsyncThreads {
private:
	static std::map <HANDLE, AsyncWait>	th_list;
public:
	static void insert(HANDLE, IAsyncPG *, DWORD);
	static void CleanupThreads(DWORD millisecond);
	static bool WaitThread(IAsyncPG *, DWORD type, DWORD millisecond);
};


#define	SYNC_AUTOCOMMIT(conn)	(SQL_AUTOCOMMIT_OFF != conn->connInfo.autocommit_public ? (conn->transact_status |= CONN_IN_AUTOCOMMIT) : (conn->transact_status &= ~CONN_IN_AUTOCOMMIT))

IAsyncPG::IAsyncPG(void) : helper(NULL), RMCookie(0), enlist(NULL), conn(NULL), xaconn(NULL), refcnt(1), prepared(false), requestAccepted(false)
{
	InterlockedIncrement(&g_cComponents);
	InitializeCriticalSection(&as_spin);
	InitializeCriticalSection(&as_exec);
	eThread[0] = eThread[1] = eThread[2] = NULL;
	memset(&xid, 0, sizeof(xid));
#ifdef	_LOCK_DEBUG_
	spin_cnt = 0;
	cs_cnt = 0;
#endif /* _LOCK_DEBUG_ */
}

//
//	invoked from *delete*.
//	When entered ELOCK -> LIFELOCK -> SLOCK are acquired
//	and they are released.
//	
IAsyncPG::~IAsyncPG(void)
{
	ConnectionClass *fconn = NULL;

	if (conn)
	{
		conn->asdum = NULL;
		conn = NULL;
	}
	if (xaconn)
	{
		fconn = xaconn;
		xaconn->asdum = NULL;
		xaconn = NULL;
	}
	SLOCK_RELEASE();
	LIFELOCK_RELEASE;
	if (fconn)
		PGAPI_FreeConnect((HDBC) fconn);
	DeleteCriticalSection(&as_spin);
	ELOCK_RELEASE();
	DeleteCriticalSection(&as_exec);
	InterlockedDecrement(&g_cComponents);
}
HRESULT STDMETHODCALLTYPE IAsyncPG::QueryInterface(REFIID riid, void ** ppvObject)
{
forcelog("%x QueryInterface called\n", this);
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
	mylog("%x->AddRef called\n", this);
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
	mylog("%x->Release called refcnt=%d\n", this, refcnt);
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
	mylog("delete %x\n", this);
			delete this;
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
//	Acquire/release [MLOCK -> ] SLOCK.
//
void IAsyncPG::Wait_pThread(bool slock_hold)
{
	mylog("Wait_pThread %d in\n", slock_hold);
	HANDLE	wThread;
	int	wait_idx = PrepareExec;
	bool	th_found;
	if (!slock_hold)
		SLOCK_ACQUIRE();
	while (NULL != eThread[wait_idx])
	{
		wThread = eThread[wait_idx];
		SLOCK_RELEASE();
		th_found = AsyncThreads::WaitThread(this, wait_idx, 2000);
		SLOCK_ACQUIRE();
		if (th_found)
			break;
	}
	if (!slock_hold)
		SLOCK_RELEASE();
	mylog("Wait_pThread out\n");
}

//
//	Acquire/releases [MLOCK -> ] SLOCK.
//
void IAsyncPG::Wait_cThread(bool slock_hold, bool once)
{
	HANDLE	wThread;
	int	wait_idx;
	bool	th_found;

	mylog("Wait_cThread %d,%d in\n", slock_hold, once);
	if (!slock_hold)
		SLOCK_ACQUIRE();
	if (NULL != eThread[CommitExec])
		wait_idx = CommitExec;
	else
		wait_idx = AbortExec;
	while (NULL != eThread[wait_idx])
	{
		wThread = eThread[wait_idx];
		SLOCK_RELEASE();
		th_found = AsyncThreads::WaitThread(this, wait_idx, 2000);
		SLOCK_ACQUIRE();
		if (once || th_found)
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
	prepared = false;
	requestAccepted = true;
	commit_result = res;
	if (conn || xaconn)
	{
		if (conn)
		{
			conn->asdum = NULL;
			SYNC_AUTOCOMMIT(conn);
			conn = NULL;
		}
		SLOCK_RELEASE();
		LIFELOCK_RELEASE;
		ELOCK_ACQUIRE();
		if (xaconn)
		{
			xaconn->asdum = NULL;
			PGAPI_FreeConnect(xaconn);
			xaconn = NULL;
		}
		ELOCK_RELEASE();
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
ConnectionClass	*IAsyncPG::generateXAConn(bool spinAcquired)
{
	if (!spinAcquired)
		SLOCK_ACQUIRE();
	if (prepared && !xaconn)
	{
		SLOCK_RELEASE();
		ELOCK_ACQUIRE();
		LIFELOCK_ACQUIRE;
		SLOCK_ACQUIRE();
		if (prepared && !xaconn)
		{
			PGAPI_AllocConnect(conn->henv, (HDBC *) &xaconn);
			memcpy(&xaconn->connInfo, &conn->connInfo, sizeof(ConnInfo));
			conn->asdum = NULL;
			SYNC_AUTOCOMMIT(conn);
			conn = NULL;
			SLOCK_RELEASE();
			LIFELOCK_RELEASE;
			CC_connect(xaconn, AUTH_REQ_OK, NULL);
		}
		else
		{
			SLOCK_RELEASE();
			LIFELOCK_RELEASE;
		}
		ELOCK_RELEASE();
	}
	else
		SLOCK_RELEASE();
	return xaconn;
}

//
//	[when entered]
//	ELOCK is acquired.
//
//	Acquire/releases SLOCK.
//	Try to acquire CONNLOCK also.
//
//	[on exit]
//	ELOCK is kept acquired.
//	If the return connection != NULL
//		the CONNLOCK for the connection is acquired.
//	
ConnectionClass	*IAsyncPG::getLockedXAConn()
{
	SLOCK_ACQUIRE();
	if (!xaconn && conn && !CC_is_in_trans(conn))
	{
		if (TRY_ENTER_CONN_CS(conn))
		{
			if (CC_is_in_trans(conn))
			{
				LEAVE_CONN_CS(conn);
			}
			else
			{
				SLOCK_RELEASE();
				return conn;
			}
		}
	}
	generateXAConn(true);
	if (xaconn)
		ENTER_CONN_CS(xaconn);
	return xaconn;
}

//
//	Acquire/release ELOCK [ -> MLOCK] -> SLOCK.
//
HRESULT IAsyncPG::RequestExec(DWORD type, HRESULT res)
{
	HRESULT		ret;
	bool		bReleaseEnlist = false;
	ConnectionClass	*econn;
	QResultClass	*qres;
	char		pgxid[258], cmd[512];

	mylog("%x->RequestExec type=%d\n", this, type);
	XidToText(xid, pgxid);
#ifdef	_SLEEP_FOR_TEST_
	/*Sleep(2000);*/
#endif	/* _SLEEP_FOR_TEST_ */
	ELOCK_ACQUIRE();
	switch (type)
	{
		case PrepareExec:
			if (XACT_S_SINGLEPHASE == res)
			{
				if (!CC_commit(conn))
					res = E_FAIL;	
				bReleaseEnlist = true;
			}
			else if (E_FAIL != res)
			{
				snprintf(cmd, sizeof(cmd), "PREPARE TRANSACTION '%s'", pgxid);
				qres = CC_send_query(conn, cmd, NULL, 0, NULL);
				if (!QR_command_maybe_successful(qres))
					res = E_FAIL;
				QR_Destructor(qres);
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
					snprintf(cmd, sizeof(cmd), "COMMIT PREPARED '%s'", pgxid);
					qres = CC_send_query(econn, cmd, NULL, 0, NULL);
					if (!QR_command_maybe_successful(qres))
						res = E_FAIL;
					QR_Destructor(qres);
					LEAVE_CONN_CS(econn);
				}
			}
			SetDone(res);
			ret = enlist->CommitRequestDone(res);
			bReleaseEnlist = true;
			break;
		case AbortExec:
			Wait_pThread(false);
			if (prepared)
			{
				econn = getLockedXAConn();
				if (econn)
				{
					snprintf(cmd, sizeof(cmd), "ROLLBACK PREPARED '%s'", pgxid);
					qres = CC_send_query(econn, cmd, NULL, 0, NULL);
					if (!QR_command_maybe_successful(qres))
						res = E_FAIL;
					QR_Destructor(qres);
					LEAVE_CONN_CS(econn);
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
	mylog("%x->Done ret=%d\n", this, ret);
	return ret;
}

//
//	Acquire/releses [MLOCK -> ] SLOCK
//	 	or 	[ELOCK -> LIFELOCK -> ] SLOCK.
//
HRESULT IAsyncPG::ReleaseConnection(void)
{
	mylog("%x->ReleaseConnection\n", this);
	ConnectionClass	*iconn;
	bool	done = false;

	SLOCK_ACQUIRE();
	if (iconn = conn)
	{
		Wait_pThread(true);
		if (NULL != eThread[CommitExec] || NULL != eThread[AbortExec] || requestAccepted)
		{
			if (prepared)
			{
				Wait_cThread(true, true);
				if (!prepared)
					done = true;
			}
			else
				done = true;
			if (done)
				Wait_cThread(true, false);
		}
		if (conn && CONN_CONNECTED == conn->status && !done)
		{
			generateXAConn(true);
		}
		else
			SLOCK_RELEASE();
	}
	else
		SLOCK_RELEASE();
	mylog("%x->ReleaseConnection exit\n", this);
	return SQL_SUCCESS;
}

//
//	Acquire/release [ELOCK -> ] [MLOCK -> ] SLOCK.
//
EXTERN_C static unsigned WINAPI DtcRequestExec(LPVOID para);
HRESULT STDMETHODCALLTYPE IAsyncPG::PrepareRequest(BOOL fRetaining, DWORD grfRM,
				BOOL fWantMoniker, BOOL fSinglePhase)
{
	HRESULT	ret, res;
	RequestPara	*reqp;

	mylog("%x PrepareRequest called grhRM=%d enl=%x\n", this, grfRM, enlist);
	SLOCK_ACQUIRE();
	if (0 != CC_get_errornumber(conn))
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
	reqp->type = PrepareExec;
	reqp->lpr = (LPVOID) this;
	reqp->res = res;
	AddRef();
	HANDLE hThread = (HANDLE) _beginthreadex(NULL, 0, DtcRequestExec, reqp, 0, NULL);
	if (NULL == hThread)
	{
		delete(reqp);
		ret = E_FAIL;
	}
	else
	{
		AsyncThreads::insert(hThread, this, reqp->type);
	}
	ELOCK_RELEASE();
	Release();
	return ret;
}
//
//	Acquire/release [ELOCK -> ] [MLOCK -> ] SLOCK.
//
HRESULT STDMETHODCALLTYPE IAsyncPG::CommitRequest(DWORD grfRM, XACTUOW * pNewUOW)
{
	HRESULT		res = S_OK, ret = S_OK;
	RequestPara	*reqp;
	mylog("%x CommitRequest called grfRM=%d enl=%x\n", this, grfRM, enlist);

	SLOCK_ACQUIRE();
	if (!prepared)
		ret = E_UNEXPECTED;
	else if (S_OK != prepare_result)
		ret = E_UNEXPECTED;
	SLOCK_RELEASE();
	if (S_OK != ret)
		return ret;
	AddRef();
	ELOCK_ACQUIRE();
#ifdef	_SLEEP_FOR_TEST_
	Sleep(1000);
#endif	/* _SLEEP_FOR_TEST_ */
	reqp = new RequestPara;
	reqp->type = CommitExec;
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
		AsyncThreads::insert(hThread, this, reqp->type);
	}
	mylog("CommitRequest ret=%d\n", ret);
	requestAccepted = true;
	ELOCK_RELEASE();
	Release();
	return ret;
}
//
//	Acquire/release [ELOCK -> ] [MLOCK -> ] SLOCK.
//
HRESULT STDMETHODCALLTYPE IAsyncPG::AbortRequest(BOID * pboidReason, BOOL fRetaining,
				XACTUOW * pNewUOW)
{
	HRESULT		res = S_OK, ret = S_OK;
	RequestPara	*reqp;

	mylog("%x AbortRequest called\n", this);
	AddRef();
	ELOCK_ACQUIRE();
	if (!prepared && conn)
		CC_abort(conn);
	reqp = new RequestPara;
	reqp->type = AbortExec;
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
		AsyncThreads::insert(hThread, this, reqp->type);
	}
	mylog("AbortRequest ret=%d\n", ret);
	requestAccepted = true;
	ELOCK_RELEASE();
	Release();
	return	ret;
}
HRESULT STDMETHODCALLTYPE IAsyncPG::TMDown(void)
{
forcelog("%x TMDown called\n", this);
	return	S_OK;
}

//
//	Acquire/releases MLOCK -> SLOCK.
//
std::map<HANDLE, AsyncWait>	AsyncThreads::th_list;
void	AsyncThreads::insert(HANDLE th, IAsyncPG *obj, DWORD type)
{
	if (!obj)	return;
	MLOCK_ACQUIRE;
	th_list.insert(std::pair<HANDLE, AsyncWait>(th, AsyncWait(obj, type)));
	obj->SLOCK_ACQUIRE();
	obj->eThread[type] = th;
	obj->SLOCK_RELEASE();
	MLOCK_RELEASE;
}

//
//	Acquire/releases MLOCK -> SLOCK.
//
bool	AsyncThreads::WaitThread(IAsyncPG *obj, DWORD type, DWORD millisecond)
{
	HANDLE	th = NULL;
	DWORD	gtype;
	bool	typematch;
	int	wait_count;

	MLOCK_ACQUIRE;
	std::map<HANDLE, AsyncWait>::iterator p;
	for (p = th_list.begin(); p != th_list.end(); p++)
	{
		gtype = p->second.GetType();
		typematch = (gtype == type);
		if (p->second.GetObj() == obj && typematch)
		{
			th = p->first;
			break;
		}
	}
	if (NULL == th)
	{
		MLOCK_RELEASE;
		forcelog("WaitThread thread(%x, %d) not found\n", obj, type);
		return false;
	}
	p->second.StartWaiting();
	MLOCK_RELEASE;
	
	DWORD ret = WaitForSingleObject(th, millisecond);
	MLOCK_ACQUIRE;
	wait_count = p->second.StopWaiting();
	if (WAIT_OBJECT_0 == ret)
	{
		IAsyncPG *async = p->second.GetObj();

		if (type >= 0 && type <= IAsyncPG::AbortExec)
			async->Reset_eThread(type);
		if (wait_count <= 0)
		{
			th_list.erase(th);
			MLOCK_RELEASE;
			CloseHandle(th);
			if (type >= IAsyncPG::CommitExec)
			{
				async->Release();
			}
		}
		else
			MLOCK_RELEASE;
	}
	else
		MLOCK_RELEASE;
	return true;
}

void	AsyncThreads::CleanupThreads(DWORD millisecond)
{
	int	msize;
	DWORD	nCount;

	MLOCK_ACQUIRE;
	if (msize = th_list.size(), msize <= 0)
	{
		MLOCK_RELEASE;
		return;
	}

	mylog("CleanupThreads size=%d\n", msize);
	HANDLE	*hds = new HANDLE[msize];
	std::map<HANDLE, AsyncWait>::iterator p;
	for (p = th_list.begin(), nCount = 0; p != th_list.end(); p++)
	{
		hds[nCount++] = p->first;
		p->second.StartWaiting();
	}
	MLOCK_RELEASE;
	int	i;
	while (nCount > 0)
	{
		DWORD ret = WaitForMultipleObjects(nCount, hds, 0, millisecond);
		if (ret >= nCount)
			break;
		HANDLE	th = hds[ret];
		MLOCK_ACQUIRE;
		p = th_list.find(th);
		if (p != th_list.end())
		{
			int wait_count = p->second.StopWaiting();
			DWORD	type = p->second.GetType();
			IAsyncPG * async = p->second.GetObj();

			if (type >= IAsyncPG::PrepareExec && type <= IAsyncPG::AbortExec)
				async->Reset_eThread(type);
			if (wait_count <= 0)
			{
				th_list.erase(th);
				MLOCK_RELEASE;
				CloseHandle(th);
				if (type >= IAsyncPG::CommitExec)
				{
					async->Release();
				}
			}
			else
				MLOCK_RELEASE;
		}
		else
			MLOCK_RELEASE;
		for (i = ret; i < (int) nCount - 1; i++)
			hds[i] = hds[i + 1];
		nCount--;
	}
	for (i = 0; i < (int) nCount; i++)
	{
		p = th_list.find(hds[i]);
		if (p != th_list.end())
			p->second.StopWaiting();
	}
	delete [] hds;
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

RETCODE static EnlistInDtc_1pipe(ConnectionClass *conn, ITransaction *pTra, ITransactionDispenser *pDtc)
{
	CSTR	func = "EnlistInDtc_1pipe";
	static	IDtcToXaHelperSinglePipe	*pHelper = NULL;
	ITransactionResourceAsync		*pRes = NULL;
	IAsyncPG				*asdum;
	HRESULT	res;
	bool	retry, errset;
	DWORD	dwRMCookie;
	XID	xid;

	if (!pHelper)
	{
		res = pDtc->QueryInterface(IID_IDtcToXaHelperSinglePipe, (void **) &pHelper);
		if (res != S_OK || !pHelper)
		{
			forcelog("DtcToXaHelperSingelPipe get error %d\n", res);
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

mylog("dllname=%s dsn=%s\n", GetXaLibName(), conn->connInfo.dsn); res = 0;
	retry = false;
	errset = false;
	ConnInfo *ci = &(conn->connInfo);
	char	dtcname[1024];
	snprintf(dtcname, sizeof(dtcname), "DRIVER={%s};SERVER=%s;PORT=%s;DATABASE=%s;UID=%s;PWD=%s;" ABBR_SSLMODE "=%s", 
		ci->drivername, ci->server, ci->port, ci->database, ci->username, ci->password, ci->sslmode);
	do { 
		res = pHelper->XARMCreate(dtcname, (char *) GetXaLibName(), &dwRMCookie);
		if (S_OK == res)
			break;
		mylog("XARMCreate error code=%x\n", res);
		if (XACT_E_XA_TX_DISABLED == res)
		{
			CC_set_error(conn, CONN_UNSUPPORTED_OPTION, "XARMcreate error:Please enable XA transaction in MSDTC security configuration", func);
			errset = true;
		}
		else if (!retry)
		{
			LONG	ret;
			HKEY	sKey;
			DWORD	rSize;

			ret = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, regKey, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &sKey);
			if (ERROR_SUCCESS != ret)
				ret = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, regKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &sKey, NULL);
			if (ERROR_SUCCESS == ret)
			{
				switch (ret = ::RegQueryValueEx(sKey, "XADLL", NULL, NULL, NULL, &rSize))
				{
					case ERROR_SUCCESS:
						if (rSize > 0)
							break;
					default:
						ret = ::RegSetValueEx(sKey, GetXaLibName(), 0, REG_SZ,
							(CONST BYTE *) GetXaLibPath(), strlen(GetXaLibPath()) + 1);
						if (ERROR_SUCCESS == ret)
						{
							retry = true;
							continue; // retry
						}
						CC_set_error(conn, CONN_UNSUPPORTED_OPTION, "XARMCreate error:Please register HKLM\\SOFTWARE\\Microsoft\\MSDTC\\XADLL", func);
						break;
				}
				::RegCloseKey(sKey);			
			}
		}
		if (!errset)
			CC_set_error(conn, CONN_UNSUPPORTED_OPTION, "MSDTC XARMCreate error", func);
		return SQL_ERROR;
	} while (1);
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
	CC_set_autocommit(conn, FALSE);
	asdum->SetConnection(conn);
	conn->asdum = asdum;

	return 	SQL_SUCCESS;
}


EXTERN_C RETCODE EnlistInDtc(ConnectionClass *conn, void *pTra, int method)
{
	static	ITransactionDispenser	*pDtc = NULL;

	if (!pTra)
	{
		IAsyncPG *asdum = (IAsyncPG *) conn->asdum;
		if (asdum)
		{
			/* asdum->Release(); */
		}
		else
			SYNC_AUTOCOMMIT(conn);
		return SQL_SUCCESS;
	}
	if (CC_is_in_trans(conn))
	{ 
		CC_abort(conn);
	}
	if (!pDtc)
	{
		HRESULT	res;

		res = DtcGetTransactionManager(NULL, NULL, IID_ITransactionDispenser,
			0, 0, NULL,  (void **) &pDtc);
		if (res != S_OK || !pDtc)
		{
			forcelog("TransactionManager get error %d\n", res);
			pDtc = NULL;
		}
	}
	return EnlistInDtc_1pipe(conn, (ITransaction *) pTra, pDtc);
}

EXTERN_C RETCODE DtcOnDisconnect(ConnectionClass *conn)
{
	mylog("DtcOnDisconnect\n");
	LIFELOCK_ACQUIRE;
	IAsyncPG *asdum = (IAsyncPG *) conn->asdum;
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

EXTERN_C RETCODE DtcOnRelease(void)
{
	AsyncThreads::CleanupThreads(2000);
	return SQL_SUCCESS;
}


#endif /* _HANDLE_ENLIST_IN_DTC_ */
