---
title: "Compiling psqlODBC on Windows"
weight: 4
aliases:
  - /docs/win32-compilation.html
---

psqlODBC is built on Windows with the PowerShell scripts in `winbuild\` and `installer\`, which drive MSBuild and WiX. The official releases are built this way, by the [MSBuild workflow](https://github.com/postgresql-interfaces/psqlodbc/blob/main/.github/workflows/main.yml) in the repository.

| Script | What it does |
| --- | --- |
| `winbuild\editConfiguration.ps1` | Opens a window for editing the build configuration, `winbuild\configuration.xml` |
| `winbuild\BuildAll.ps1` | Builds the drivers |
| `winbuild\regress.ps1` | Builds the regression tests and runs them against the drivers you built |
| `installer\buildInstallers.ps1` | Builds the `.msi` installers and merge modules |

Each script has a batch file of the same name at the top of the source tree, such as `BuildAll.bat`, for use from Command Prompt.

## Requirements

- **Visual Studio 2015 or later, up to Visual Studio 2026**, with the "Desktop development with C++" workload. Install the MSVC build tools for each platform you build: x86 and x64, and ARM64 for ARM64 builds. The build tools without the IDE are enough.
- **libpq for each platform:** the headers (`include`), the import library (`lib`) and the DLLs (`bin`). The PostgreSQL installers for Windows include 64-bit libpq. For x86 and ARM64 you build libpq from the PostgreSQL source; the release workflow builds it with Meson, with OpenSSL for SSL support.
- **WiX 6 with the UI extension**, to build installers:

  ```powershell
  dotnet tool install --global wix --version "6.*"
  wix extension add --global WixToolset.UI.wixext/6.0.0
  ```

- **The `mimalloc` submodule**, to build with the mimalloc allocator. Clone with `git clone --recurse-submodules`, or run `git submodule update --init` in an existing clone.
- **For the regression tests:** a running PostgreSQL server, and GNU `diff` on `PATH`, for example from [GnuWin32 DiffUtils](https://gnuwin32.sourceforge.net/packages/diffutils.htm), which the release workflow uses.

PowerShell must be allowed to run local scripts. If `Get-ExecutionPolicy` reports `Restricted` or `AllSigned`, run:

```powershell
Set-ExecutionPolicy RemoteSigned
```

## Configuring the build

The scripts read the location of libpq for each platform from `winbuild\configuration.xml`. To create or edit it, run:

```powershell
.\winbuild\editConfiguration.ps1
```

or copy `winbuild\configuration_template.xml` to `winbuild\configuration.xml` and edit it by hand. For example:

```xml
<Configuration version="" formatVersion="0.4" vcversion="" toolset="">
  <x86 disabled="no">
    <libpq version="y">
      <include>D:\postgresql86\include</include>
      <lib>D:\postgresql86\lib</lib>
      <bin>D:\postgresql86\bin</bin>
    </libpq>
  </x86>
  <x64>
    <libpq version="y">
      <include>D:\postgresql\include</include>
      <lib>D:\postgresql\lib</lib>
      <bin>D:\postgresql\bin</bin>
    </libpq>
  </x64>
  <arm64>
    <libpq version="y">
      <include>C:\postgresql_arm64\include</include>
      <lib>C:\postgresql_arm64\lib</lib>
      <bin>C:\postgresql_arm64\bin</bin>
    </libpq>
  </arm64>
</Configuration>
```

Leave `vcversion` and `toolset` empty to use the newest Visual Studio installed. After each build, `BuildAll.ps1` records the Visual Studio version and toolset it used in the `BuildResult` element of this file.

## Building the drivers

```powershell
.\winbuild\BuildAll.ps1
```

builds the Release drivers for x86 and x64. The parameters are:

| Parameter | Values | Default |
| --- | --- | --- |
| `-Platform` | `Win32`, `x64`, `ARM64`, or `both` for Win32 and x64. ARM64 is built only when you ask for it. | `both` |
| `-Configuration` | `Release` or `Debug` | `Release` |
| `-Target` | `Build`, `Rebuild` or `Clean` | `Build` |
| `-VCVersion` | The Visual Studio version, from `14.0` (Visual Studio 2015) to `18.0` (Visual Studio 2026) | the newest installed |
| `-Toolset` | The MSBuild platform toolset, such as `v143` | chosen from the Visual Studio version |
| `-UseMimalloc` | Build with the mimalloc allocator. Needs the `v141` toolset or later, and isn't available for ARM64. | off |
| `-AlongWithInstallers` | Build the installers after the drivers | off |
| `-BuildConfigPath` | A configuration file other than `winbuild\configuration.xml` | |

For example, to build the ARM64 drivers:

```powershell
.\winbuild\BuildAll.ps1 -Platform ARM64
```

Each platform, driver type and configuration gets its own output directory, named `<cpu>_<type>_<configuration>`, for example `x64_Unicode_Release` or `arm64_ANSI_Debug`. The directories are created at the top of the source tree, or under the directory in the `PSQLODBC_OBJBASE` environment variable if it is set. Each holds:

| File | What it is |
| --- | --- |
| `psqlodbc35w.dll` | the Unicode driver (in the `Unicode` directories) |
| `psqlodbc30a.dll` | the ANSI driver (in the `ANSI` directories) |
| `pgenlist.dll`, `pgenlista.dll` | MSDTC support for the Unicode and the ANSI driver |
| `pgxalib.dll` | the MSDTC XA library, built with the Unicode driver and used by both |

## Running the regression tests

```powershell
.\winbuild\regress.ps1 -DsnInfo "SERVER=localhost|DATABASE=contrib_regression|PORT=5432|UID=postgres|PWD=password"
```

builds the test programs and runs them against the drivers that `BuildAll.ps1` built, with no installer needed. The first run registers a test driver and a System DSN, which asks for administrator rights. Without `-DsnInfo`, the script prompts for the server, port, database, user and password.

| Parameter | Values | Default |
| --- | --- | --- |
| `-Platform` | `Win32`, `x64`, `ARM64` or `both` | `both` |
| `-Ansi` | Test the ANSI drivers instead of the Unicode drivers | off |
| `-DeclareFetch` | Run with Use Declare/Fetch `on`, `off` or `both` | `on` |
| `-TestList` | Run only these tests, named without `-test`, for example `-TestList connect, select` | all tests |
| `-ReinstallDriver` | Register the test driver again | off |

The test programs and results go in `winbuild\test_x86`, `winbuild\test_x64` or `winbuild\test_arm64`. Differences from the expected output are written to `regression.diffs` there.

## Building the installers

```powershell
.\installer\buildInstallers.ps1
```

builds `psqlodbc_x86.msi` and `psqlodbc_x64.msi`, with the matching merge modules (`.msm`), into `installer\x86` and `installer\x64`, or under `PSQLODBC_OBJBASE` if it is set. When it builds both, it also builds the `psqlodbc-setup.exe` bootstrapper. Use `-cpu x86`, `-cpu x64` or `-cpu arm64` to build one platform.

The installers bundle libpq and the DLLs it depends on, such as the OpenSSL libraries, from the libpq `bin` directory in the configuration, together with the Visual C++ runtime. `-ExcludeRuntime` leaves out the Visual C++ runtime, `-RedistUCRT` bundles the Visual C++ Redistributable, and `-NoPDB` leaves out the debug symbol files. See `installer\README.txt` for more.

## Building with NMAKE

`win64.mak` builds the drivers from a Visual Studio Developer Command Prompt, without the PowerShell scripts. It builds for the architecture of the prompt, despite its name. It does not build installers.

```bat
nmake /f win64.mak
nmake /f win64.mak ANSI_VERSION=yes
nmake /f win64.mak world
```

The first builds the Unicode driver and the second the ANSI driver. `world` builds both drivers for x64 and for x86, but it switches architecture with the `setenv` command of the Windows SDK 7.1 command prompt, which Visual Studio's Developer Command Prompt doesn't have. The defaults are in `windows-defaults.mak`. To change them, create `windows-local.mak` in the same directory, or set them on the command line:

| Variable | Values | Default |
| --- | --- | --- |
| `CFG` | `Release` or `Debug` | `Release` |
| `PG_INC` | The libpq `include` directory | `$(PROGRAMFILES)\PostgreSQL\9.6\include` |
| `PG_LIB` | The libpq `lib` directory | `$(PROGRAMFILES)\PostgreSQL\9.6\lib` |
| `ANSI_VERSION` | `yes` builds the ANSI driver, `psqlodbc30a.dll`, instead of the Unicode driver, `psqlodbc35w.dll` | `no` |
| `MSDTC` | `no` leaves out MSDTC support | `yes` |
