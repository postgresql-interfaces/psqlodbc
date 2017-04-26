/* File:			convert.h
 *
 * Description:		See "convert.c"
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *
 */

#ifndef __CONVERT_H__
#define __CONVERT_H__

#include "psqlodbc.h"

#ifdef	__cplusplus
extern "C" {
#endif
/* copy_and_convert results */
#define COPY_OK							0
#define COPY_UNSUPPORTED_TYPE					1
#define COPY_UNSUPPORTED_CONVERSION				2
#define COPY_RESULT_TRUNCATED					3
#define COPY_GENERAL_ERROR						4
#define COPY_NO_DATA_FOUND						5
#define COPY_INVALID_STRING_CONVERSION				6

int	copy_and_convert_field_bindinfo(StatementClass *stmt, OID field_type, int atttypmod, void *value, int col);
int	copy_and_convert_field(StatementClass *stmt,
			OID field_type, int atttypmod,
			void *value,
			SQLSMALLINT fCType, int precision,
			PTR rgbValue, SQLLEN cbValueMax, SQLLEN *pcbValue, SQLLEN *pIndicator);

int		copy_statement_with_parameters(StatementClass *stmt, BOOL);
SQLLEN		pg_hex2bin(const char *in, char *out, SQLLEN len);
size_t		findTag(const char *str, int ccsc);

BOOL build_libpq_bind_params(StatementClass *stmt,
						int *nParams, OID **paramTypes,
						char ***paramValues,
						int **paramLengths,
						int **paramFormats,
						int *resultFormat);
#ifdef	__cplusplus
}
#endif
#endif
