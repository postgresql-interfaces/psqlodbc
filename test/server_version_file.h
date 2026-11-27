/*
 * File:			server_version_file.h
 *
 * Description:		The file that carries the server's major version from
 *			reset-db, which connects to the server, to runsuite,
 *			which picks the expected output and links no ODBC
 *			library of its own. It holds the major version as
 *			decimal digits, so 19 for PostgreSQL 19.
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *
 */

#ifndef __SERVER_VERSION_FILE_H__
#define __SERVER_VERSION_FILE_H__

#ifdef WIN32
#define SERVER_VERSION_FILE	"results\\server_version"
#else
#define SERVER_VERSION_FILE	"results/server_version"
#endif

#endif
