/**
 * @file Mdl.cpp
 * @author created by: Peter Hlavaty
 */

#include "StdAfx.h"
#include "MemoryMapping.h"

//************* virtual to virtual *************

_IRQL_requires_max_(DISPATCH_LEVEL)
CMdl::CMdl(
	__in void* virtualAddress, 
	__in size_t size
	) : m_locked(false),
		m_mem(NULL),
		m_mdl(NULL)
{
	m_lockOperation = IoWriteAccess;
	m_mdl = IoAllocateMdl(virtualAddress, (ULONG)size, FALSE, FALSE, NULL);
}

CMdl::CMdl( 
	__in const void* virtualAddress, 
	__in size_t size 
	) : m_locked(false),
		m_mem(NULL),
		m_mdl(NULL)
{
	m_lockOperation = IoReadAccess;
	m_mdl = IoAllocateMdl(const_cast<void*>(virtualAddress), (ULONG)size, FALSE, FALSE, NULL);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
CMdl::~CMdl()
{
	if (m_mdl)
	{
		if (m_locked)
			MmUnlockPages(m_mdl);

		//call also CMdl::Unmap() ?
		IoFreeMdl(m_mdl);
	}
}

//Callers of MmProbeAndLockPages must be running at IRQL <= APC_LEVEL for pageable addresses, or at IRQL <= DISPATCH_LEVEL for nonpageable addresses.
_IRQL_requires_max_(APC_LEVEL)
__checkReturn
bool CMdl::Lock()
{
	if (!m_mdl)
		return false;

	__try 
	{
		MmProbeAndLockPages(m_mdl, KernelMode, m_lockOperation);
		m_locked = true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
	return true;
}

//If AccessMode is UserMode, the caller must be running at IRQL <= APC_LEVEL. If AccessMode is KernelMode, the caller must be running at IRQL <= DISPATCH_LEVEL.
_IRQL_requires_max_(APC_LEVEL)
__checkReturn
const void* CMdl::ReadPtr(
	__in_opt MEMORY_CACHING_TYPE cacheType /*= MmCached*/ 
	)
{
	return Map(cacheType, false);
}

_IRQL_requires_max_(APC_LEVEL)
__checkReturn
void* CMdl::WritePtr( 
	__in_opt MEMORY_CACHING_TYPE cacheType /*= MmCached */
	)
{
	if (m_lockOperation == IoWriteAccess)
		return Map(cacheType, false);
	return NULL;
}

_IRQL_requires_max_(APC_LEVEL)
const void* CMdl::ReadPtrUser( 
	__in_opt MEMORY_CACHING_TYPE cacheType /*= MmCached */ 
	)
{
	return Map(cacheType, true);
}

_IRQL_requires_max_(APC_LEVEL)
__checkReturn
void* CMdl::WritePtrUser( 
	__in_opt MEMORY_CACHING_TYPE cacheType /*= MmCached */ 
	)
{
	if (m_lockOperation == IoWriteAccess)
		return Map(cacheType, true);
	return NULL;
}


//Callers of MmUnmapLockedPages must be running at IRQL <= DISPATCH_LEVEL if the pages were mapped to system space. Otherwise, the caller must be running at IRQL <= APC_LEVEL.
_IRQL_requires_max_(APC_LEVEL)
void CMdl::Unmap()
{
	if (m_mem && m_mdl)
	{
		MmUnmapLockedPages(m_mem, m_mdl);
		m_mem = NULL;
	}
}

void* CMdl::Map( 
	__in MEMORY_CACHING_TYPE cacheType,
	__in bool user
	)
{

	if (!m_mdl)//mdl fail
		return NULL;

	if (!m_locked && !Lock())
		return NULL;

	if (m_mem)//need to unmap first
		return NULL;

	__try 
	{
		m_mem = MmMapLockedPagesSpecifyCache(m_mdl, user ? UserMode : KernelMode, cacheType, NULL, FALSE, user ? NormalPagePriority : HighPagePriority);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		DbgPrint("\n LOCK exception occured\n");
	}

	return m_mem;
}

//************* physical to virtual *************

_IRQL_requires_max_(DISPATCH_LEVEL)
CMmMap::CMmMap( 
	__in ULONG_PTR address,
	__in size_t size
	)
{
	Init(address, size);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
CMmMap::CMmMap( 
	__in const void* address, 
	__in size_t size 
	)
{
	Init((ULONG_PTR)address, size);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
CMmMap::CMmMap( 
	__in const PHYSICAL_ADDRESS& address,
	__in size_t size
	)
{
	Init(address.QuadPart, size);
}

void CMmMap::Init(
	__in ULONG_PTR address,
	__in size_t size
	)
{
	m_size = size;
	RtlZeroMemory(&m_addrPhysical, sizeof(m_addrPhysical));
	m_addrPhysical.QuadPart = address;

	m_addrVirtual = MapPhysicalToVirtual(m_addrPhysical, m_size);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
CMmMap::~CMmMap()
{
	if (m_addrVirtual)//int nt!kipagefault getnexttable IRQL_NOT_LESS_OR_EQUAL
		MmUnmapIoSpace(m_addrVirtual, m_size);
}

void* CMmMap::GetVirtualAddress()
{
	return m_addrVirtual;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void* CMmMap::MapPhysicalToVirtual(
	__in const PHYSICAL_ADDRESS& address, 
	__in size_t size 
	)
{
	return MmMapIoSpace(address, size, MmNonCached);
}
