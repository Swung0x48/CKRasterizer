#ifndef VXTHREAD_H
#define VXTHREAD_H

#include "XString.h"
#include "XHashTable.h"
#include "VxMutex.h"

#define VXTERROR_TIMEOUT		9	// The timeout of an operation was reached.
#define VXTERROR_NULLTHREAD		50	// The thread object is null.
#define VXTERROR_WAIT			51	// An error occurred while you try to join (wait for) a thread.
#define VXTERROR_EXITCODE		52	// An error while you try to get the exit code of a tread.
#define VXT_OK					53	// No error. 

/*************************************************
Summary: VxThread state

See also: VxThread, VxThread::Join.
*************************************************/
typedef enum VXTHREAD_STATE
{
    VXTS_INITIALE = 0x00000000L,	// Initial state of a thread object (at this time the system thread isn't created).
    VXTS_MAIN     = 0x00000001L,	// The thread is the main thread
    VXTS_CREATED  = 0x00000002L,	// The system thread associated with the current thread object was correctly created.
    VXTS_STARTED  = 0x00000004L,	// The thread has been started.
    VXTS_JOINABLE = 0x00000008L	// The thread is joinable (it means you can call VxThread::Join). Under Windows thread are always joinable.
} VXTHREAD_STATE;

/*************************************************
Summary: Enumeration of possible priority for a thread.

See also: VxThread,VxThread::SetPriority(unsigned int priority).
*************************************************/
typedef enum VXTHREAD_PRIORITY
{
    VXTP_NORMAL       = 0,
    VXTP_ABOVENORMAL  = 1,
    VXTP_BELOWNORMAL  = 2,
    VXTP_HIGHLEVEL    = 3,
    VXTP_LOWLEVEL     = 4,
    VXTP_IDLE         = 5,
    VXTP_TIMECRITICAL = 6,
} VXTHREAD_PRIORITY;

/*************************************************
Summary: Thread is still active.
{PartOf:VxThread::GetExitCode}
*************************************************/
const unsigned int VXT_STILLACTIVE = 1000000;

/*************************************************
Summary: Thread has been terminated with VxThread::Terminate.
{PartOf:VxThread::GetExitCode}
*************************************************/
const unsigned int VXT_TERMINATEFORCED = 1000001;

/*************************************************
Summary: Prototype of a function which will be executed by a VxThread.

Remarks:
You can also overload VxThread::Run instead of creating an external function
Return value:
    You must return a value greater than 0 if you want define your own return value.
    Otherwise you can return a NKERROR value.

See also: VxThread, VxThread::Run, NKERROR.
*************************************************/
typedef unsigned int VxThreadFunction(void *args);

/*************************************************
Summary: Represents a system thread.

Remarks:
There is to way to use thread :

    1) you can inherit from the VxThread class and overload
    the VxThread::Run() method.

    2) or you can associate a ThreadFunction to you're
    while the creation.

See also: VxThread::Run,VxThread::CreateThread,VxMutex.
*************************************************/
class VxThread
{

public:
    VX_EXPORT VxThread();
    VX_EXPORT virtual ~VxThread();

    VX_EXPORT XBOOL CreateThread(VxThreadFunction *func = 0, void *args = 0);
    VX_EXPORT void SetPriority(unsigned int priority);
    VX_EXPORT void SetName(const char *name);
    VX_EXPORT void Close();
    VX_EXPORT const XString &GetName() const;
    VX_EXPORT unsigned int GetPriority() const;
    VX_EXPORT XBOOL IsCreated() const;
    VX_EXPORT XBOOL IsJoinable() const;
    VX_EXPORT XBOOL IsMainThread() const;
    VX_EXPORT XBOOL IsStarted() const;
    VX_EXPORT static VxThread *GetCurrentVxThread();

    VX_EXPORT int Wait(unsigned int *status = 0, unsigned int timeout = 0);
    VX_EXPORT const GENERIC_HANDLE GetHandle() const;
    VX_EXPORT XULONG GetID() const;
    VX_EXPORT XBOOL GetExitCode(unsigned int &status);
    VX_EXPORT XBOOL Terminate(unsigned int *status = 0);
    VX_EXPORT static XULONG GetCurrentVxThreadId();

protected:
    /*************************************************
    Summary: Method to be executed by the thread if it was created
    without ThreadFunction.

    Return Value:
        The exit code of the thread.
        By default this method return VXT_OK.
        Otherwise you can return your own return value (must be greater than 0)
        or a NKERROR.

    Remarks:
        If you create the thread without ThreadFunction you need
        create your own thread object which derived VxThread
        and to overload the Run method.

    See also: CreateThread, ThreadFunction.
    *************************************************/
    virtual unsigned int Run() { return VXT_OK; }

    XString m_Name;

private:
    friend void InitVxMath();

    friend void ShutDownVxMath();

    VxThread(const VxThread &);

    VxThread &operator=(const VxThread &);

    void SetPriority();

    static VxMutex &GetMutex();

    static XHashTable<VxThread *, GENERIC_HANDLE> &GetHashThread();

    static XULONG __stdcall ThreadFunc(void *args);

    GENERIC_HANDLE m_Thread;

    unsigned int m_ThreadID;

    unsigned int m_State;

    unsigned int m_Priority;

    VxThreadFunction *m_Func;

    void *m_Args;

    static VxThread *m_MainThread;
};

#endif // VXTHREAD_H
