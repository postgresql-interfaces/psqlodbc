/*
 * Test UTF-16 surrogate-pair handling in wide-character ODBC APIs.
 *
 * SQLPrepareW() converts the SQLWCHAR input with ucs2_to_utf8().  A dangling
 * high surrogate at the end of the caller-specified length must not make the
 * driver read the following SQLWCHAR.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#ifdef WIN32
static SQLWCHAR *
alloc_guarded_high_surrogate(void **mapping)
{
	SYSTEM_INFO info;
	DWORD page_size;
	DWORD old_protect;
	char *mem;

	GetSystemInfo(&info);
	page_size = info.dwPageSize;
	mem = VirtualAlloc(NULL, page_size * 2, MEM_RESERVE | MEM_COMMIT,
					   PAGE_READWRITE);
	if (!mem)
		return NULL;
	if (!VirtualProtect(mem + page_size, page_size, PAGE_NOACCESS,
						&old_protect))
	{
		VirtualFree(mem, 0, MEM_RELEASE);
		return NULL;
	}
	*mapping = mem;
	return (SQLWCHAR *) (mem + page_size - sizeof(SQLWCHAR));
}

static void
free_guarded_high_surrogate(void *mapping)
{
	VirtualFree(mapping, 0, MEM_RELEASE);
}
#else
#include <sys/mman.h>
#include <unistd.h>

static SQLWCHAR *
alloc_guarded_high_surrogate(void **mapping)
{
	long page_size;
	char *mem;

	page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0)
		return NULL;

	mem = mmap(NULL, (size_t) page_size * 2, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (MAP_FAILED == mem)
		return NULL;
	if (mprotect(mem + page_size, (size_t) page_size, PROT_NONE) != 0)
	{
		munmap(mem, (size_t) page_size * 2);
		return NULL;
	}
	*mapping = mem;
	return (SQLWCHAR *) (mem + page_size - sizeof(SQLWCHAR));
}

static void
free_guarded_high_surrogate(void *mapping)
{
	long page_size = sysconf(_SC_PAGESIZE);

	if (page_size > 0)
		munmap(mapping, (size_t) page_size * 2);
}
#endif

int
main(void)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLWCHAR *sql;
	void *mapping = NULL;

	test_connect_ext("");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate statement handle", SQL_HANDLE_DBC, conn);
		test_disconnect();
		return 1;
	}

	sql = alloc_guarded_high_surrogate(&mapping);
	if (!sql)
	{
		fprintf(stderr, "failed to allocate guarded SQLWCHAR buffer\n");
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		test_disconnect();
		return 1;
	}

	sql[0] = 0xd800;
	printf("preparing SQL text ending with a dangling high surrogate\n");
	rc = SQLPrepareW(hstmt, sql, 1);
	printf("SQLPrepareW returned without reading past the supplied length\n");

	free_guarded_high_surrogate(mapping);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	printf("ok\n");

	return 0;
}
