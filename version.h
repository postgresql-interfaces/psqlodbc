/* File:			version.h
 *
 * Description:		This file defines the driver version.
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *
 */

#ifndef __VERSION_H__
#define __VERSION_H__

/*
 *	BuildAll may pass POSTGRESDRIVERVERSION and PG_DRVFILE_VERSION
 *	via winbuild/psqlodbc.vcxproj.	
 */
#ifndef POSTGRESDRIVERVERSION
#define POSTGRESDRIVERVERSION		"09.03.0100"
#endif
#define POSTGRES_RESOURCE_VERSION	POSTGRESDRIVERVERSION "\0"
#ifndef PG_DRVFILE_VERSION
#define PG_DRVFILE_VERSION		9,3,01,00
#endif
#define PG_BUILD_VERSION		"201402020001"

#endif
