#-------------------------------------------------------------------------
#
# Makefile.am for psqlodbc30w (PostgreSQL ODBC driver)
#
#-------------------------------------------------------------------------

AUTOMAKE_OPTIONS = 1.8 foreign

ACLOCAL_AMFLAGS = -I config

lib_LTLIBRARIES = psqlodbcw.la psqlodbca.la

AM_LDFLAGS = -module -no-undefined -avoid-version -export-symbols-regex '^SQL'

psqlodbcw_la_CFLAGS = -DUNICODE_SUPPORT # -DUNICODE_SUPPORTXX

psqlodbca_la_SOURCES = \
	info.c bind.c columninfo.c connection.c convert.c drvconn.c \
	environ.c execute.c lobj.c misc.c options.c \
	pgtypes.c psqlodbc.c qresult.c results.c parse.c \
	statement.c tuple.c dlg_specific.c \
	multibyte.c odbcapi.c descriptor.c \
	odbcapi30.c pgapi30.c mylog.c \
\
	bind.h catfunc.h columninfo.h connection.h convert.h \
	descriptor.h dlg_specific.h environ.h unicode_support.h \
	lobj.h misc.h multibyte.h pgapifunc.h pgtypes.h \
	psqlodbc.h qresult.h resource.h statement.h tuple.h \
	version.h pgenlist.h mylog.h xalibname.h

psqlodbcw_la_SOURCES = $(psqlodbca_la_SOURCES) \
	odbcapi30w.c odbcapiw.c win_unicode.c


EXTRA_DIST = license.txt readme.txt readme_winbuild.txt \
	psqlodbc.def psqlodbca.def editConfiguration.bat BuildAll.bat \
	windows-defaults.mak buildInstallers.bat regress.bat \
	pgenlist.def pgenlista.def connexp.h \
	dlg_wingui.c loadlib.c loadlib.h inouealc.c win_setup.h \
        psqlsetup.c setup.c win_unicode.c psqlodbc.rc win64.mak \
        psqlodbc.reg psqlodbc.dsp psqlodbc.vcproj \
        psqlodbc.sln msdtc_enlist.cpp pgxalib.cpp xalibname.c \
        pgxalib.def psqlsetup.def  psqlsetupa.def odbc.sql odbc-drop.sql \
\
	docs/config-opt.html \
	docs/config.html \
	docs/README.txt \
	docs/release-7.3.html \
	docs/release.html \
	docs/unix-compilation.html \
	docs/win32-compilation.html \
	docs/msdtc_pgxalib_tracing_enable.reg \
	docs/msdtc_pgxalib_tracing_disable.reg \
	docs/msdtc_pgxalib_tracing_README.txt \
	docs/editConfiguration.jpg \
\
	installer/background.bmp \
	installer/banner.bmp \
	installer/buildInstallers.ps1 \
	installer/installer.mak \
	installer/lgpl.rtf \
	installer/modify_msi.vbs \
	installer/productcodes.mak \
	installer/psqlodbc-setup \
	installer/README.txt \
	installer/psqlodbcm_cpu.wxs \
	installer/psqlodbc_cpu.wxs \
	installer/psqlodbc-setup/buildBootstrapper.ps1 \
	installer/psqlodbc-setup/Bundle.wxs \
	installer/psqlodbc-setup/Make.bat \
	installer/psqlodbc-setup/psqlodbc-setup.wixproj \
	installer/psqlodbc-setup/vcredist.wxs \
\
	winbuild/BuildAll.ps1 \
	winbuild/platformbuild.vcxproj \
	winbuild/MSProgram-Get.psm1 \
	winbuild/psqlodbc.Cpp.props \
	winbuild/Psqlodbc-config.psm1 \
	winbuild/psqlodbc.vcxproj \
	winbuild/configuration_template.xml \
	winbuild/psqlsetup.vcxproj \
	winbuild/editConfiguration.ps1 \
	winbuild/readme.txt \
	winbuild/pgenlist.vcxproj \
	winbuild/regress.ps1 \
	winbuild/pguser.Cpp.props \
	winbuild/regress_one.vcxproj \
	winbuild/pgxalib.vcxproj \
	winbuild/std_dbmsname.Cpp.props \
\
	test/Makefile.in \
	test/odbcini-gen.sh \
	test/README.txt \
	test/RegisterRegdsn.c \
	test/reset-db.c \
	test/runsuite.c \
	test/sampletables.sql \
	test/tests \
	test/win.mak \
	test/expected \
	test/Makefile \
	test/src

MAINTAINERCLEANFILES = \
	Makefile.in config/config.guess config.h.in config/config.sub configure \
	config/install-sh config/missing aclocal.m4 config/ltmain.sh \
	config/depcomp
