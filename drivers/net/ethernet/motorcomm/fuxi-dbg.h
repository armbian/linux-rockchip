/*++

Copyright (c) 2021 Motorcomm Corporation. 
Confidential and Proprietary. All rights reserved.

This is Motorcomm Corporation NIC driver relevant files. Please don't copy, modify,
distribute without commercial permission.

--*/

#ifndef _MP_DBG_H
#define _MP_DBG_H
#include "fuxi-os.h"

//
// Message verbosity: lower values indicate higher urgency
//
#define MP_OFF          0
#define MP_ERROR        1
#define MP_WARN         2
#define MP_TRACE        3
#define MP_INFO         4
#define MP_LOUD         5

#if defined(UEFI)

//#pragma warning(disable:4100) //warning C4100: 'xxx': unreferenced formal parameter
//#pragma warning(disable:4101) //warning C4101: 'xxx': unreferenced local variable
//#pragma warning(disable:4189) //warning C4189: 'xxx': local variable is initialized but not referenced
//#pragma warning(disable:4213) //warning C4213: nonstandard extension used: cast on l-value.
//#pragma warning(disable:4201) //warning C4201: nonstandard extension used: nameless struct/union

#define NIC_DBG_STRING                  "FUXI: "
#define STR_FORMAT      "%a"
extern UINT32 MPDebugLevel;
#if DBG
#define DbgPrintF(level, ...)   \
{                                                                   \
    if (level <= MPDebugLevel)                                      \
    {                                                               \
        AsciiPrint(NIC_DBG_STRING  ##__VA_ARGS__);                    \
        AsciiPrint("\n");                                             \
    }                                                               \
}
#define DBGPRINT(Level, Fmt)  \
{ \
    if (Level <= MPDebugLevel) \
    { \
        /*DbgPrint(NIC_DBG_STRING);*/ \
        AsciiPrint  Fmt; \
    } \
}
#else
#define DBGPRINT(Level, Fmt) 
#define DbgPrintF(level, ...)  
#endif
#define DBGPRINT_RAW(Level, Fmt)
#define DBGPRINT_S(Status, Fmt)
#define DBGPRINT_UNICODE(Level, UString)
#define Dump(p,cb,fAddress,ulGroup)

void fxgmac_dump_buffer(unsigned char *skb, unsigned int len, int tx_rx);

#elif defined(_WIN64) || defined(_WIN32)

//#pragma warning(disable:4100) //warning C4100: 'xxx': unreferenced formal parameter
//#pragma warning(disable:4101) //warning C4101: 'xxx': unreferenced local variable
//#pragma warning(disable:4189) //warning C4189: 'xxx': local variable is initialized but not referenced
//#pragma warning(disable:4213) //warning C4213: nonstandard extension used: cast on l-value.
#pragma warning(disable:4201) //warning C4201: nonstandard extension used: nameless struct/union

#define STR_FORMAT      "%s"
// Define a macro so DbgPrint can work on win9x, 32-bit/64-bit NT's
#ifdef _WIN64
#define PTR_FORMAT      "%p"
#else
#define PTR_FORMAT      "%x"
#endif

#define NIC_TAG                         ((ULONG)'FUXI')
#define NIC_DBG_STRING                  "FUXI: "


#if DBG

extern ULONG MPDebugLevel;
extern BOOLEAN          MPInitDone;
extern NDIS_SPIN_LOCK   MPMemoryLock;

#define DbgPrintF(Level, ...)                                       \
{                                                                   \
    if (Level <= MPDebugLevel)                                      \
    {                                                               \
        DbgPrint(NIC_DBG_STRING  ##__VA_ARGS__);                    \
        DbgPrint("\n");                                             \
    }                                                               \
}

#define DBGPRINT(Level, Fmt) \
{ \
    if (Level <= MPDebugLevel) \
    { \
        /*DbgPrint(NIC_DBG_STRING);*/ \
        DbgPrint Fmt; \
    } \
}

#define DBGPRINT_RAW(Level, Fmt) \
{ \
    if (Level <= MPDebugLevel) \
    { \
      DbgPrint Fmt; \
    } \
}

#define DBGPRINT_S(Status, Fmt) \
{ \
    ULONG dbglevel; \
    if(Status == NDIS_STATUS_SUCCESS || Status == NDIS_STATUS_PENDING) dbglevel = MP_TRACE; \
    else dbglevel = MP_ERROR; \
    DBGPRINT(dbglevel, Fmt); \
}

#define DBGPRINT_UNICODE(Level, UString) \
{ \
    if (Level <= MPDebugLevel) \
    { \
        /* DbgPrint(NIC_DBG_STRING); */ \
      mpDbgPrintUnicodeString(UString); \
   } \
}

#undef ASSERT
#define ASSERT(x) if(!(x)) { \
    DBGPRINT(MP_ERROR, ("Assertion failed: %s:%d %s\n", __FILE__, __LINE__, #x)); \
    /*DbgBreakPoint();*/ }

VOID
DbgPrintOidName(
    _In_  NDIS_OID  Oid
);

VOID
DbgPrintAddress(
    _In_reads_bytes_(ETH_LENGTH_OF_ADDRESS) PUCHAR Address)
    ;

#pragma  pack(push)
#pragma  pack(16)
//
// The MP_ALLOCATION structure stores all info about MPAuditAllocMemTag
//
typedef struct _MP_ALLOCATION
{
    LIST_ENTRY              List;
    ULONG                   Signature;
    CHAR*                   FileNumber;
    ULONG                   LineNumber;
    ULONG                   Size;
    ULONGLONG               dummy; // for 64 bit alignment
    union {
        ULONGLONG           Alignment;        
        UCHAR               UserData;
    };
} MP_ALLOCATION, *PMP_ALLOCATION;

#pragma  pack(pop)

PVOID 
MPAuditAllocMemTag(
    UINT        Size,
    CHAR*      FileNumber,
    ULONG       LineNumber,
    NDIS_HANDLE MiniportAdapterHandle
    );

VOID MPAuditFreeMem(
    PVOID       Pointer
    );
    
VOID mpDbgPrintUnicodeString(
    IN  PUNICODE_STRING UnicodeString);


VOID
Dump(
    __in_bcount(cb) CHAR*   p,
    ULONG                   cb,
    BOOLEAN                 fAddress,
    ULONG                   ulGroup );

void fxgmac_dump_buffer(unsigned char* skb, unsigned int len, int tx_rx);

VOID
DumpLine(
    __in_bcount(cb) CHAR* p,
    ULONG                   cb,
    BOOLEAN                 fAddress,
    ULONG                   ulGroup);


#else   // !DBG

#define DbgPrintF(level,  fmt, ...)
#define DBGPRINT(Level, Fmt)
#define DBGPRINT_RAW(Level, Fmt)
#define DBGPRINT_S(Status, Fmt)
#define DBGPRINT_UNICODE(Level, UString)
#define Dump(p,cb,fAddress,ulGroup)

#undef ASSERT
#define ASSERT(x)

#define DbgPrintOidName(_Oid)
#define DbgPrintAddress(_pAddress)

#define fxgmac_dump_buffer(_skb, _len, _tx_rx)
#define DumpLine(_p, _cbLine, _fAddress, _ulGroup )
#endif  // DBG

#elif defined(LINUX)

#define STR_FORMAT      "%s"

#define DbgPrintF(level,  fmt, ...)
#define DBGPRINT(Level, Fmt)
#define DBGPRINT_RAW(Level, Fmt)
#define DBGPRINT_S(Status, Fmt)
#define DBGPRINT_UNICODE(Level, UString)
#define Dump(p,cb,fAddress,ulGroup)

#undef ASSERT
#define ASSERT(x)

#define DbgPrintOidName(_Oid)
#define DbgPrintAddress(_pAddress)

#define fxgmac_dump_buffer(_skb, _len, _tx_rx)
#define DumpLine(_p, _cbLine, _fAddress, _ulGroup )

#elif defined(UBOOT)
#ifdef UBOOT_DEBUG
extern u32   MPDebugLevel;
#define NIC_DBG_STRING                  "YT6801: "
#define STR_FORMAT      "%s"
#define PTR_FORMAT      "%x"
//#define DBGPRINT(Level, Fmt) printf Fmt
#define DbgPrintF(Level, fmt, args...)                                  \
{                                                                   \
    if (Level <= MPDebugLevel)                                      \
    {                                                               \
        printf(NIC_DBG_STRING fmt,##args);                                \
        printf("\n");                                               \
    }                                                               \
}
#else
#define DbgPrintF(Level, ...)                                       
#endif
#else

//#pragma warning(disable:4100) //warning C4100: 'xxx': unreferenced formal parameter
//#pragma warning(disable:4101) //warning C4101: 'xxx': unreferenced local variable
//#pragma warning(disable:4189) //warning C4189: 'xxx': local variable is initialized but not referenced
//#pragma warning(disable:4213) //warning C4213: nonstandard extension used: cast on l-value.
#pragma warning(disable:4201) //warning C4201: nonstandard extension used: nameless struct/union
#pragma warning(disable:4002)  //warning C4002: too many actual parameters for macro


#endif // ifdef UEFI.

#endif  // _MP_DBG_H