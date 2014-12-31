

# All the driver files that will be included in the installer
DRIVER_FILES = ../$(TARGET_CPU)_Unicode_$(CFG)/psqlodbc35w.dll \
	../$(TARGET_CPU)_Unicode_$(CFG)/pgxalib.dll \
	../$(TARGET_CPU)_Unicode_$(CFG)/pgenlist.dll \
	../$(TARGET_CPU)_ANSI_$(CFG)/psqlodbc30a.dll \
	../$(TARGET_CPU)_ANSI_$(CFG)/pgxalib.dll \
	../$(TARGET_CPU)_ANSI_$(CFG)/pgenlista.dll

ALL: $(TARGET_CPU)\psqlodbc_$(TARGET_CPU).msm $(TARGET_CPU)\psqlodbc_$(TARGET_CPU).msi

CANDLE="$(WIX)bin\candle.exe"
LIGHT="$(WIX)bin\light"

!INCLUDE ..\windows-defaults.mak
!IF EXISTS(..\windows-local.mak)
!INCLUDE ..\windows-local.mak
!ENDIF

!MESSAGE determining product code

!INCLUDE productcodes.mak

!MESSAGE Got product code $(PRODUCTCODE)

MSM_OPTS = -dLIBPQBINDIR="$(LIBPQ_BIN)"

# Merge module
$(TARGET_CPU)\psqlodbc_$(TARGET_CPU).msm: psqlodbcm_cpu.wxs $(DRIVER_FILES)
	echo Building Installer Merge Module
	$(CANDLE) -nologo -dPlatform="$(TARGET_CPU)" -dVERSION=$(POSTGRESDRIVERVERSION) -dSUBLOC=$(SUBLOC) $(MSM_OPTS) -o $(TARGET_CPU)\psqlodbcm.wixobj psqlodbcm_cpu.wxs
	$(LIGHT) -nologo -o $(TARGET_CPU)\psqlodbc_$(TARGET_CPU).msm $(TARGET_CPU)\psqlodbcm.wixobj

$(TARGET_CPU)\psqlodbc_$(TARGET_CPU).msi: psqlodbc_cpu.wxs $(DRIVER_FILES)
	echo Building Installer
	$(CANDLE) -nologo -dPlatform="$(TARGET_CPU)" -dVERSION=$(POSTGRESDRIVERVERSION) -dSUBLOC=$(SUBLOC) -dPRODUCTCODE=$(PRODUCTCODE) -o $(TARGET_CPU)\psqlodbc.wixobj psqlodbc_cpu.wxs
	$(LIGHT) -nologo -ext WixUIExtension -cultures:en-us -o $(TARGET_CPU)\psqlodbc_$(TARGET_CPU).msi $(TARGET_CPU)\psqlodbc.wixobj
	cscript modify_msi.vbs $(TARGET_CPU)\psqlodbc_$(TARGET_CPU).msi

clean:
	-rd /Q /S x64 x86
