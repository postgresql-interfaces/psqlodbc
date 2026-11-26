/*
 * Test that SQLGetDescField() on the IRD stores exactly a SQLSMALLINT for
 * the descriptor fields that ODBC defines as SQLSMALLINT, and leaves the
 * bytes beyond it alone.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define GUARD	0x5A5A

static const struct
{
	SQLSMALLINT	field;
	const char *name;
} smallint_fields[] =
{
	{ SQL_DESC_CONCISE_TYPE, "SQL_DESC_CONCISE_TYPE" },
	{ SQL_DESC_FIXED_PREC_SCALE, "SQL_DESC_FIXED_PREC_SCALE" },
	{ SQL_DESC_NULLABLE, "SQL_DESC_NULLABLE" },
	{ SQL_DESC_PRECISION, "SQL_DESC_PRECISION" },
	{ SQL_DESC_SCALE, "SQL_DESC_SCALE" },
	{ SQL_DESC_SEARCHABLE, "SQL_DESC_SEARCHABLE" },
	{ SQL_DESC_TYPE, "SQL_DESC_TYPE" },
	{ SQL_DESC_UNNAMED, "SQL_DESC_UNNAMED" },
	{ SQL_DESC_UNSIGNED, "SQL_DESC_UNSIGNED" },
	{ SQL_DESC_UPDATABLE, "SQL_DESC_UPDATABLE" },
};

/*
 * The field is read into 'value'; 'guard' sits right behind it and must
 * not be touched.
 */
struct guarded_smallint
{
	SQLSMALLINT	value;
	SQLSMALLINT	guard;
};

static void
get_smallint_field(SQLHDESC ird, SQLSMALLINT recno, SQLSMALLINT field,
				   const char *fieldname)
{
	SQLRETURN	rc;
	SQLINTEGER	strlen = -1;
	struct guarded_smallint *buf;

	/* a heap block, so that a store past 'guard' leaves the block */
	buf = malloc(sizeof(*buf));
	buf->value = 0;
	buf->guard = GUARD;

	rc = SQLGetDescField(ird, recno, field, &buf->value, SQL_IS_SMALLINT, &strlen);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLGetDescField failed", SQL_HANDLE_DESC, ird);
		exit(1);
	}
	printf("%s: %d (length %d)\n", fieldname, (int) buf->value, (int) strlen);
	if (buf->guard != GUARD)
	{
		printf("ERROR: SQLGetDescField wrote past the SQLSMALLINT for %s\n", fieldname);
		exit(1);
	}
	free(buf);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLHDESC	ird;
	SQLSMALLINT colcount;
	int			i,
				col;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 1::int4 AS number, 'x'::text AS label", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLNumResultCols(hstmt, &colcount);
	CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_ROW_DESC, &ird, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);

	/* header field */
	printf("\n-- Header --\n");
	get_smallint_field(ird, 0, SQL_DESC_COUNT, "SQL_DESC_COUNT");

	/* record fields */
	for (col = 1; col <= colcount; col++)
	{
		printf("\n-- Column %d --\n", col);
		for (i = 0; i < sizeof(smallint_fields) / sizeof(smallint_fields[0]); i++)
			get_smallint_field(ird, col, smallint_fields[i].field, smallint_fields[i].name);
	}

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
