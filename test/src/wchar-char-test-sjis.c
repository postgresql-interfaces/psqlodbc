
static int sjis_test(HSTMT hstmt)
{
	int rc;

	SQLLEN		ind, cbParam, cbParam2;
	unsigned char	lovedt[100] = {0x95, 0x4e, 0x0a, 0x4e, 0x5a, 0x53, 0xf2, 0x53, 0x0, 0x0};
	SQLWCHAR	wchar[100];
	SQLCHAR		str[100];
	SQLCHAR		chardt[100];
	SQLTCHAR	query[] = _T("select 'éÑÇÕ' || ?::text || 'Ç≈Ç∑ÅBãMï˚ÇÕ' || ?::text || 'Ç≥ÇÒÇ≈Ç∑ÇÀÅH'");

	rc = SQLBindCol(hstmt, 1, SQL_C_CHAR, (SQLPOINTER) chardt, sizeof(chardt), &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol to SQL_C_CHAR failed", hstmt);

	cbParam = SQL_NTS;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
		SQL_C_WCHAR,	/* value type */
		SQL_WCHAR,			/* param type */
		sizeof(lovedt) / sizeof(lovedt[0]),			/* column size */
		0,			/* dec digits */
		lovedt,		// param1,		/* param value ptr */
		sizeof(lovedt),			/* buffer len */
		&cbParam	/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter 1 failed", hstmt);
	cbParam2 = SQL_NTS;
	rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, sizeof(str), 0, str, sizeof(str), &cbParam2);
	CHECK_STMT_RESULT(rc, "SQLBindParameter 2 failed", hstmt);
	strncpy((char *) str, "êƒì°ç_", sizeof(str));
	rc = SQLExecDirect(hstmt, query, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed to return SQL_C_CHAR", hstmt);
	while (SQL_SUCCEEDED(SQLFetch(hstmt)))
		printf("ANSI=%s\n", chardt);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	rc = SQLBindCol(hstmt, 1, SQL_C_WCHAR, (SQLPOINTER) wchar, sizeof(wchar) / sizeof(wchar[0]), &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol to SQL_C_WCHAR failed", hstmt);


	rc = SQLExecDirect(hstmt, query, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed to return SQL_C_WCHAR", hstmt);
	while (SQL_SUCCEEDED(rc = SQLFetch(hstmt)))
	{
		int	i;
		for (i = 0; wchar[i]; i++)
			printf("U+%02X%02X", (unsigned short) wchar[i] / 256, wchar[i] % 256);
		printf("\n");
		// wprintf(L"Unicode=%ls\n", wchar);
	}

	return rc;
}
