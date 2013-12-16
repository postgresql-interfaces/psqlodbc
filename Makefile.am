#-------------------------------------------------------------------------
#
# Makefile.am for psqlodbc30w (PostgreSQL ODBC driver)
#
#-------------------------------------------------------------------------

AUTOMAKE_OPTIONS = 1.8 foreign

ACLOCAL_AMFLAGS = -I config

if enable_unicode
lib_LTLIBRARIES = psqlodbcw.la
else
lib_LTLIBRARIES = psqlodbca.la
endif

AM_LDFLAGS = -module -no-undefined -avoid-version

psqlodbca_la_SOURCES = \
	info.c bind.c columninfo.c connection.c convert.c drvconn.c \
	environ.c execute.c lobj.c md5.c misc.c options.c \
	pgtypes.c psqlodbc.c qresult.c results.c socket.c parse.c \
	statement.c tuple.c dlg_specific.c loadlib.c \
	multibyte.c odbcapi.c descriptor.c \
	odbcapi30.c pgapi30.c info30.c mylog.c \
\
	bind.h catfunc.h columninfo.h connection.h convert.h \
	descriptor.h dlg_specific.h environ.h \
	lobj.h md5.h misc.h multibyte.h pgapifunc.h pgtypes.h \
	psqlodbc.h qresult.h resource.h socket.h statement.h tuple.h \
	version.h loadlib.h pgenlist.h mylog.h

psqlodbcw_la_SOURCES = $(psqlodbca_la_SOURCES) \
	odbcapi30w.c odbcapiw.c win_unicode.c


EXTRA_DIST = license.txt readme.txt \
	psqlodbc.def psqlodbca.def \
	pgenlist.def pgenlista.def connexp.h \
	dlg_wingui.c inouealc.c win_setup.h \
        setup.c win_unicode.c psqlodbc.rc win64.mak \
        win32.mak psqlodbc.reg psqlodbc.dsp psqlodbc.vcproj \
        psqlodbc.sln msdtc_enlist.cpp pgxalib.cpp xalibname.c \
        pgxalib.def odbc.sql odbc-drop.sql odbcapi25w.c \
	sspisvcs.c sspisvcs.h gsssvcs.c gsssvcs.h \
	buildx64.ps1 buildx86.ps1 \
\
	docs/config-opt.html \
	docs/config.html \
	docs/README.txt \
	docs/release-7.3.html \
	docs/release.html \
	docs/unix-compilation.html \
	docs/win32-compilation.html \
\
	installer/background.bmp \
	installer/banner.bmp \
	installer/buildX64-installer.ps1 \
	installer/buildX86-installer.ps1 \
	installer/lgpl.rtf \
	installer/Make.bat \
	installer/MakeX64.bat \
	installer/modify_msi.vbs \
	installer/psqlodbc-setup \
	installer/psqlodbc-setup/Bundle.wxs \
	installer/psqlodbc-setup/Make.bat \
	installer/psqlodbc-setup/psqlodbc-setup.wixproj \
	installer/psqlodbc-setup/vcredist.wxs \
	installer/psqlodbc.wxs \
	installer/psqlodbcm.wxs \
	installer/psqlodbcm_cpu.wxs \
	installer/psqlodbc_cpu.wxs \
	installer/README.txt \
	installer/upgrade.bat \
	installer/upgrade_x64.bat \
\
	winbuild/configuration.ps1 \
	winbuild/configuration_template.xml \
	winbuild/editConfiguration.bat \
	winbuild/editConfiguration.ps1 \
\
	test/expected/alter.out \
	test/expected/arraybinding.out \
	test/expected/boolsaschar.out \
	test/expected/catalogfunctions.out \
	test/expected/commands.out \
	test/expected/connect.out \
	test/expected/cursors.out \
	test/expected/cursors_1.out \
	test/expected/cvtnulldate.out \
	test/expected/dataatexecution.out \
	test/expected/getresult.out \
	test/expected/insertreturning.out \
	test/expected/multistmt.out \
	test/expected/notice.out \
	test/expected/params.out \
	test/expected/positioned-update.out \
	test/expected/prepare.out \
	test/expected/quotes.out \
	test/expected/sampletables.out \
	test/expected/select.out \
	test/expected/stmthandles.out \
	test/launcher \
	test/Makefile \
	test/odbc.ini \
	test/odbcinst.ini \
	test/README.txt \
	test/sql/sampletables.sql \
	test/src/alter-test.c \
	test/src/arraybinding-test.c \
	test/src/boolsaschar-test.c \
	test/src/catalogfunctions-test.c \
	test/src/commands-test.c \
	test/src/common.c \
	test/src/common.h \
	test/src/connect-test.c \
	test/src/cursors-test.c \
	test/src/cvtnulldate-test.c \
	test/src/dataatexecution-test.c \
	test/src/getresult-test.c \
	test/src/insertreturning-test.c \
	test/src/multistmt-test.c \
	test/src/notice-test.c \
	test/src/params-test.c \
	test/src/positioned-update-test.c \
	test/src/prepare-test.c \
	test/src/quotes-test.c \
	test/src/select-test.c \
	test/src/stmthandles-test.c 

MAINTAINERCLEANFILES = \
	Makefile.in config/config.guess config.h.in config/config.sub configure \
	config/install-sh config/missing aclocal.m4 config/ltmain.sh \
	config/depcomp
