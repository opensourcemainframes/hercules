@echo off

rem $Id:


  ::  Uncomment the below to ALWAYS generate assembly (.cod) listings, or
  ::  define it yourself via 'set' command before invoking this batch file.

  :: set ASSEMBLY_LISTINGS=1


::*****************************************************************************
::*****************************************************************************
::*****************************************************************************
::***                                                                       ***
::***                         MAKEFILE.BAT                                  ***
::***                                                                       ***
::*****************************************************************************
::*****************************************************************************
::*****************************************************************************
::*                                                                           *
::*      Designed to be called either from the command line or by a           *
::*      Visual Studio makefile project with the "Build command line"         *
::*      set to: "makefile.bat <arguments...>". See 'HELP' section            *
::*      further below for details regarding use.                             *
::*                                                                           *
::*      If this batch file works okay then it was written by Fish.           *
::*      If it doesn't work then I don't know who the heck wrote it.          *
::*                                                                           *
::*****************************************************************************

  set rc=0
  goto :begin

::-----------------------------------------------------------------------------
::                                HELP
::-----------------------------------------------------------------------------
:help

  ::***************************************************************************
  ::
  ::  All error messages *MUST* be issued in the following strict format:
  ::
  ::         echo %~nx0^(1^) : error C9999 : error-message-text...
  ::
  ::  in order for the Visual Studio IDE to detect it as a build error since
  ::  exiting with a non-zero return code doesn't do the trick. Visual Studio
  ::  apparently examines the message-text looking for error/warning strings.
  ::
  ::***************************************************************************
  echo.
  echo %~nx0^(1^) : error C9999 : Help information is as follows:
  echo.
  echo.
  echo.
  echo                             %~nx0
  echo.
  echo.
  echo  Initializes the Windows software development build envionment and invokes
  echo  nmake to build the desired 32 or 64-bit version of the Hercules emulator.
  echo.
  echo.
  echo  Format:
  echo.
  echo.
  echo    %~nx0 {build-type} {makefile-name} {num-cpu-engines} [-a^|clean]
  echo.
  echo.
  echo  Where:
  echo.
  echo    {build-type}        The desired build configuration. Valid values are
  echo                        DEBUG / RETAIL for building a 32-bit Hercules, or
  echo                        DEBUG-X64 / RETAIL-X64 to build a 64-bit version
  echo                        of Hercules targeting (favoring) AMD64 processors,
  echo                        or DEBUG-IA64 / RETAIL-IA64 for a 64-bit version
  echo                        of Hercules favoring Intel Itanium-64 processors.
  echo.
  echo    {makefile-name}     The name of our makefile:  'makefile.msvc'
  echo.
  echo    {num-cpu-engines}   The maximum number of emulated CPUs (NUMCPU=) you
  echo                        want this build of Hercules to support: 1 to 32.
  echo.
  echo    [-a^|clean]          Use '-a' to perform a full rebuild of all Hercules
  echo                        binaries, or 'clean' to delete all temporary work
  echo                        files from all work/output directories, including
  echo                        any/all previously built binaries. If not specified
  echo                        then only those modules that need to be rebuilt are
  echo                        actually rebuilt, usually resulting in much quicker
  echo                        build. However, when doing a 'RETAIL' build it is
  echo                        HIGHLY RECOMMENDED that you always specify the '-a'
  echo                        option to ensure that a complete rebuild is done.
  echo.

  exit /b 1

::-----------------------------------------------------------------------------
::                                BEGIN
::-----------------------------------------------------------------------------
:begin


  set build_type=%~1
  set makefile_name=%~2
  set num_cpus=%~3
  set extra_nmake_arg=%~4
  set SolutionDir=%~5


  if "%build_type%" == ""        goto :help
  if "%build_type%" == "?"       goto :help
  if "%build_type%" == "/?"      goto :help
  if "%build_type%" == "-?"      goto :help
  if "%build_type%" == "--help"  goto :help


  call :set_version %VERSION%
  if %rc% NEQ 0 exit /b 1


  call :set_build_env
  if %rc% NEQ 0 exit /b 1


  call :validate_makefile
  if %rc% NEQ 0 exit /b 1


  call :validate_num_cpus
  if %rc% NEQ 0 exit /b 1


  call :set_cfg
  if %rc% NEQ 0 exit /b 1


  call :set_targ_arch
  if %rc% NEQ 0 exit /b 1


  goto :%build_env%


::-----------------------------------------------------------------------------
::                          PROGRAMMING NOTE
::-----------------------------------------------------------------------------
::
::  It's important to use 'setlocal' before calling the Microsoft batch file
::  that sets up the build environment and to call 'endlocal' before exiting.
::
::  Doing this ensures the build environment is freshly initialized each time
::  this batch file is invoked, thereby ensuing a valid build environment is
::  created each time a build is performed. Failure to do so runs the risk of
::  not only an incompatible (invalid) build environment being constructed as
::  a result of subsequent build setting being created on top of the previous
::  build settings, but also risks overflowing the environment area since the
::  PATH and LIB variables would thus keep growing ever longer and longer.
::
::  Thus to ensure that never happens and that we always start with a freshly
::  initialized and (hopefully!) valid build environment each time, we use
::  setlocal and endlocal to push/pop the local environment space each time
::  we are called.
::
::  Also note that while it would be simpler to simply create a "front end"
::  batch file to issue the setlocal before invoking this batch file (and
::  then do the endlocal once we return), doing it that way leaves open the
::  possibility that some smart-aleck newbie developer who doesn't know any
::  better from bypassing the font-end batch file altogether and invoking us
::  directly and then asking for support when he has problems because builds
::  their Hercules builds are not working correctly.
::
::  Yes it's more effort to do things this way but as long as you're careful
::  it's worth it in my personal opinion.
::
::                                                -- Fish, March 2009
::
::-----------------------------------------------------------------------------
::                         VS2005  or  VS2008
::-----------------------------------------------------------------------------

:vs90
:vs80

  if /i "%targ_arch%" == "all" goto :multi_build

  call :set_host_arch
  if %rc% NEQ 0 exit /b 1

  :: (see PROGRAMMING NOTE further above!)
  setlocal

  call "%VSTOOLSDIR%..\..\VC\vcvarsall.bat"  %host_arch%%targ_arch%
  goto :nmake


::-----------------------------------------------------------------------------
::                     Platform SDK  or  VCToolkit
::-----------------------------------------------------------------------------

:sdk
:toolkit

  :: (see PROGRAMMING NOTE further above!)
  setlocal

  if /i "%build_env%" == "toolkit" call "%bat_dir%vcvars32.bat"
  call "%bat_dir%setenv" /%BLD%  /%CFG%
  goto :nmake


::-----------------------------------------------------------------------------
::                        CALLED SUBROUTINES
::-----------------------------------------------------------------------------
:set_version

  if "%~1" == "" (

    echo.
    echo %~nx0^(1^) : error C9999 : VERSION= is undefined.
    set rc=1
    goto :EOF
  )

  :: NOTE: in the below "(set V1=%%a&&set V2=%%b&&set V3=%%c&&set V4=%%d)"
  :: clause, there is NO SPACE between the '%%a' and the '&&' ampersands
  :: and the 'set' commands. This is done on purpose so that there are no
  :: trailing spaces in the value being set.

  for /f "tokens=1,2,3,4 delims=." %%a in ('echo %~1') do (set V1=%%a&&set V2=%%b&&set V3=%%c&&set V4=%%d)

  if     "%V1%" == "" set V1=1
  if     "%V2%" == "" set V2=0
  if     "%V3%" == "" set V3=0
  if not "%V4%" == "" goto :version_done

  call :auto_build_count
  set V4=%BUILDCOUNT_NUM%

:version_done

  set VERSION=\"%V1%.%V2%.%V3%.%V4%\"
  goto :EOF


::-----------------------------------------------------------------------------
:auto_build_count

  set abc_filename=AutoBuildCount.h

  if exist "%abc_filename%" goto :increment

:create

  set BUILDCOUNT_NUM=1
  goto :writefile

:increment

  for /f "tokens=1,2,3" %%a in (%abc_filename%) do (
    if "%%a" == "#define" (
      if "%%b" == "BUILDCOUNT_NUM" (
        set /a BUILDCOUNT_NUM=%%c+1
      )
    )
  )

:writefile

  (echo #ifndef AUTOBUILDCOUNT)                     >   "%abc_filename%"
  (echo #define AUTOBUILDCOUNT)                     >>  "%abc_filename%"
  (echo #define BUILDCOUNT_NUM  ^%BUILDCOUNT_NUM%)  >>  "%abc_filename%"
  (echo #define BUILDCOUNT_STR "%BUILDCOUNT_NUM%")  >>  "%abc_filename%"
  (echo #endif)                                     >>  "%abc_filename%"

:touch

  :: The following is done to prevent the build system from detecting
  :: that our AutoBuildCount.h header has been updated. This stops the
  :: build system from mistakenly thinking our build is out-of-date...

  touch "--date=2000-01-01" "%abc_filename%"

:done

  if /i "%1" == "/q" goto :EOF
  if /i "%1" == "-q" goto :EOF

  echo AutoBuildCount = %BUILDCOUNT_NUM%
  goto :EOF


::-----------------------------------------------------------------------------
:set_build_env

  set rc=0
  set build_env=

  if "%VS90COMNTOOLS%" == "" goto :try_vs80

  set   build_env=vs90
  set VSTOOLSDIR=%VS90COMNTOOLS%
  goto :EOF

:try_vs80

  if "%VS80COMNTOOLS%" == "" goto :try_sdk

  set   build_env=vs80
  set VSTOOLSDIR=%VS80COMNTOOLS%
  goto :EOF

:try_sdk

  if "%MSSdk%" == "" goto :try_toolkit

  set build_env=sdk
  set bat_dir=%MSSdk%
  goto :EOF

:try_toolkit

  if "%VCToolkitInstallDir%" == "" goto :try_xxxx

  set build_env=toolkit
  set bat_dir=%VCToolkitInstallDir%
  goto :EOF

:try_xxxx

  echo.
  echo %~nx0^(1^) : error C9999 : No suitable Windows development product is installed
  set rc=1
  goto :EOF


::-----------------------------------------------------------------------------
:validate_makefile

  set rc=0
  if exist "%makefile_name%" goto :EOF

  echo.
  echo %~nx0^(1^) : error C9999 : makefile "%makefile_name%" not found
  set rc=1
  goto :EOF


::-----------------------------------------------------------------------------
:validate_num_cpus

  set rc=1

  for /f "delims=0123456789" %%i in ("%num_cpus%\") do if "%%i" == "\" set rc=0

  if %rc% NEQ 0        goto :bad_num_cpus
  if %num_cpus% LSS 1  goto :bad_num_cpus
  if %num_cpus% GTR 32 goto :bad_num_cpus
  goto :EOF

:bad_num_cpus

  echo.
  echo %~nx0^(1^) : error C9999 : "%num_cpus%": Invalid number of cpu engines
  set rc=1
  goto :EOF


::-----------------------------------------------------------------------------
:set_cfg

  set rc=0
  set CFG=

  if /i     "%build_type%" == "DEBUG"       set CFG=DEBUG
  if /i     "%build_type%" == "DEBUG-X64"   set CFG=DEBUG
  if /i     "%build_type%" == "DEBUG-IA64"  set CFG=DEBUG

  if /i     "%build_type%" == "RETAIL"      set CFG=RETAIL
  if /i     "%build_type%" == "RETAIL-X64"  set CFG=RETAIL
  if /i     "%build_type%" == "RETAIL-IA64" set CFG=RETAIL

  :: Check for VS2008 multi-configuration multi-platform parallel build...

  if    not "%CFG%"        == ""            goto :EOF
  if /i not "%build_env%"  == "vs90"        goto :bad_cfg

  if /i     "%build_type%" == "DEBUG-ALL"   set CFG=debug
  if /i     "%build_type%" == "RETAIL-ALL"  set CFG=retail
  if /i     "%build_type%" == "ALL-ALL"     set CFG=all
  if    not "%CFG%"        == ""            goto :EOF

:bad_cfg

  echo.
  echo %~nx0^(1^) : error C9999 : "%build_type%": Invalid build-type
  set rc=1
  goto :EOF


::-----------------------------------------------------------------------------
:set_targ_arch

  set rc=0
  set targ_arch=

  if /i     "%build_type%" == "DEBUG"       set targ_arch=x86
  if /i     "%build_type%" == "RETAIL"      set targ_arch=x86

  if /i     "%build_type%" == "DEBUG-X64"   set targ_arch=amd64
  if /i     "%build_type%" == "RETAIL-X64"  set targ_arch=amd64

  if /i     "%build_type%" == "DEBUG-IA64"  set targ_arch=ia64
  if /i     "%build_type%" == "RETAIL-IA64" set targ_arch=ia64

  :: Check for VS2008 multi-configuration multi-platform parallel build...

  if    not "%targ_arch%"  == ""            goto :set_CPU_etc
  if /i not "%build_env%"  == "vs90"        goto :bad_targ_arch

  if /i     "%build_type%" == "DEBUG-ALL"   set targ_arch=all
  if /i     "%build_type%" == "RETAIL-ALL"  set targ_arch=all
  if /i     "%build_type%" == "ALL-ALL"     set targ_arch=all
  if    not "%targ_arch%"  == ""            goto :EOF

:bad_targ_arch

  echo.
  echo %~nx0^(1^) : error C9999 : "%build_type%": Invalid build-type
  set rc=1
  goto :EOF

:set_CPU_etc

  set CPU=

  if /i "%targ_arch%" == "x86"   set CPU=i386
  if /i "%targ_arch%" == "amd64" set CPU=AMD64
  if /i "%targ_arch%" == "ia64"  set CPU=IA64

  set BLD=

  if /i "%targ_arch%" == "x86"   set BLD=XP32
  if /i "%targ_arch%" == "amd64" set BLD=XP64
  if /i "%targ_arch%" == "ia64"  set BLD=IA64

  set _WIN64=

  if /i "%targ_arch%" == "amd64" set _WIN64=1
  if /i "%targ_arch%" == "ia64"  set _WIN64=1

  goto :EOF


::-----------------------------------------------------------------------------
:set_host_arch

  set rc=0
  set host_arch=

  if /i "%PROCESSOR_ARCHITECTURE%" == "x86"   set host_arch=x86
  if /i "%PROCESSOR_ARCHITECTURE%" == "AMD64" set host_arch=amd64
  if /i "%PROCESSOR_ARCHITECTURE%" == "IA64"  set host_arch=ia64

  ::  PROGRAMMING NOTE: there MUST NOT be any spaces before the ')'!!!

  if /i not "%host_arch%" == "%targ_arch%" goto :cross

  set host_arch=
  if exist "%VSTOOLSDIR%..\..\VC\bin\vcvars32.bat" goto :EOF
  goto :missing

:cross

  set host_arch=x86_

  if exist "%VSTOOLSDIR%..\..\VC\bin\%host_arch%%targ_arch%\vcvars%host_arch%%targ_arch%.bat" goto :EOF
  goto :missing

:missing

  echo.
  echo %~nx0^(1^) : error C9999 : Build support for target architecture %targ_arch% is not installed
  set rc=1
  goto :EOF


::-----------------------------------------------------------------------------
:fullpath

  ::  Search the Windows PATH for the file in question and return its
  ::  fully qualified path if found. Otherwise return an empty string.
  ::  Note: the below does not work for directories, only files.

  set fullpath=%~dpnx$PATH:1
  goto :EOF


::-----------------------------------------------------------------------------
:re_set

  :: The following called when set= contains (), which confuses poor windoze

  set %~1=%~2
  goto :EOF


::-----------------------------------------------------------------------------
::                            CALL NMAKE
::-----------------------------------------------------------------------------
:nmake

  set  MAX_CPU_ENGINES=%num_cpus%


  ::  Additional nmake arguments (for reference):
  ::
  ::   -nologo   suppress copyright banner
  ::   -s        silent (suppress commands echoing)
  ::   -k        keep going if error(s)
  ::   -g        display !INCLUDEd files (VS2005 or greater only)


  nmake -nologo -f "%makefile_name%" -s %extra_nmake_arg%
  set rc=%errorlevel%



  :: (see the PROGRAMMING NOTE at the beginning of this batch file!)
  endlocal  &  set rc=%rc%


  exit /b %rc%


::-----------------------------------------------------------------------------
::     VS2008 multi-configuration multi-platform parallel build
::-----------------------------------------------------------------------------
::
::  The following is special logic to leverage Fish's "RunJobs" tool
::  that allows us to build multiple different project configurations
::  in parallel with one another which Microsoft does not yet support.
::
::  Refer to the CodeProject article entitled "RunJobs" at the following
::  URL for more details: http://www.codeproject.com/KB/cpp/runjobs.aspx
::
::-----------------------------------------------------------------------------
:multi_build

  ::-------------------------------------------------
  ::  Make sure the below defined command exists...
  ::-------------------------------------------------

  set runjobs_cmd=runjobs.exe

  call :fullpath "%runjobs_cmd%"
  if "%fullpath%" == "" goto :no_runjobs
  set runjobs_cmd=%fullpath%


  ::-------------------------------------------------------------------
  ::  VCBUILD requires that both $(SolutionDir) and $(SolutionPath)
  ::  are defined properly. Note however that while $(SolutionDir)
  ::  *must* be accurate (so that VCBUILD can find the Solution and
  ::  Projects you wish to build), the actual Solution file defined
  ::  in the $(SolutionPath) variable does not need to exist since
  ::  it is never used. (But the directory portion of its path must
  ::  match $(SolutionDir) of course). Also note that we must call
  ::  vsvars32.bat in order to setup the proper environment so that
  ::  VCBUILD can work properly (and so that RunJobs can find it!)
  ::-------------------------------------------------------------------

  if "%SolutionDir%" == "" (
    call :re_set SolutionDir "%extra_nmake_arg%"
    set extra_nmake_arg=
  )
  call :re_set SolutionPath "%SolutionDir%notused.sln"
  call "%VSTOOLSDIR%vsvars32.bat"

  if /i "%extra_nmake_arg%" == ""      set VCBUILDOPT=/rebuild
  if /i "%extra_nmake_arg%" == "-a"    set VCBUILDOPT=/rebuild
  if /i "%extra_nmake_arg%" == "clean" set VCBUILDOPT=/clean

  "%runjobs_cmd%" %CFG%-%targ_arch%.jobs
  set rc=%errorlevel%

  exit /b %rc%


:no_runjobs

  echo.
  echo %~nx0^(1^) : error C9999 : Could not find "%runjobs_cmd%" anywhere on your system
  set rc=1
  exit /b %rc%

::-----------------------------------------------------------------------------
::                            ((( EOF )))
::-----------------------------------------------------------------------------
