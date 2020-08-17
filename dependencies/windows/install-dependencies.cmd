@echo off

setlocal

set PATH=%CD%\tools;%PATH%

set WGET_ARGS=-c --no-check-certificate
set UNZIP_ARGS=-q

set BASEURL=https://s3.amazonaws.com/rstudio-buildtools/
set GIN_FILE=gin-2.1.2.zip
set GWT_FILE=gwt-2.8.1.zip
set JUNIT_FILE=junit-4.9b3.jar
set GNUDIFF_FILE=gnudiff.zip
set GNUGREP_FILE=gnugrep-2.5.4.zip
set MSYS_SSH_FILE=msys-ssh-1000-18.zip
set SUMATRA_PDF_FILE=SumatraPDF-3.1.1.zip
set WINUTILS_FILE=winutils-1.0.zip
set WINPTY_FILES=winpty-0.4.3-msys2-2.7.0.zip
set OPENSSL_FILES=openssl-1.0.2p.zip
set BOOST_FILES=boost-1.65.1-win-msvc141.zip

set PANDOC_VERSION=2.6
set PANDOC_NAME=pandoc-%PANDOC_VERSION%-windows-x86_64
set PANDOC_FILE=%PANDOC_NAME%.zip

set LIBCLANG_VERSION=5.0.2
set LIBCLANG_NAME=libclang-windows-%LIBCLANG_VERSION%
set LIBCLANG_FILE=%LIBCLANG_NAME%.zip

if not "%CONDA_BUILD%" == "1" (
  if not exist gnudiff (
    wget %WGET_ARGS% "%BASEURL%%GNUDIFF_FILE%"
    mkdir gnudiff
    echo Unzipping %GNUDIFF_FILE%
    unzip %UNZIP_ARGS% "%GNUDIFF_FILE%" -d gnudiff
    del "%GNUDIFF_FILE%"
  )
)

if not "%CONDA_BUILD%" == "1" (
  if not exist gnugrep (
    wget %WGET_ARGS% "%BASEURL%%GNUGREP_FILE%"
    mkdir gnugrep
    echo Unzipping %GNUGREP_FILE%
    unzip %UNZIP_ARGS% "%GNUGREP_FILE%" -d gnugrep
    del "%GNUGREP_FILE%"
  )
)

if not "%CONDA_BUILD%" == "1" (
  if not exist msys-ssh-1000-18 (
    wget %WGET_ARGS% "%BASEURL%%MSYS_SSH_FILE%"
    mkdir msys-ssh-1000-18
    echo Unzipping %MSYS_SSH_FILE%
    unzip %UNZIP_ARGS% "%MSYS_SSH_FILE%" -d msys-ssh-1000-18
    del "%MSYS_SSH_FILE%"
  )
)

if not exist sumatra\3.1.1 (
  wget %WGET_ARGS% "%BASEURL%%SUMATRA_PDF_FILE%"
  mkdir sumatra\3.1.1
  echo Unzipping %SUMATRA_PDF_FILE%
  unzip %UNZIP_ARGS% "%SUMATRA_PDF_FILE%" -d sumatra\3.1.1
  del "%SUMATRA_PDF_FILE%"
)

if not exist winutils\1.0 (
  wget %WGET_ARGS% "%BASEURL%%WINUTILS_FILE%"
  mkdir winutils\1.0
  echo Unzipping %WINUTILS_FILE%
  unzip %UNZIP_ARGS% "%WINUTILS_FILE%" -d winutils\1.0
  del "%WINUTILS_FILE%"
)

if not exist winpty-0.4.3-msys2-2.7.0 (
  wget %WGET_ARGS% "%BASEURL%%WINPTY_FILES%"
  echo Unzipping %WINPTY_FILES%
  unzip %UNZIP_ARGS% "%WINPTY_FILES%"
  del %WINPTY_FILES%
)

if not exist %OPENSSL_FILES:~0,-4% (
  wget %WGET_ARGS% "%BASEURL%%OPENSSL_FILES%"
  echo Unzipping %OPENSSL_FILES%
  unzip %UNZIP_ARGS% "%OPENSSL_FILES%"
  del %OPENSSL_FILES%
)

if not exist %BOOST_FILES:~0,-4%* (
  wget %WGET_ARGS% "%BASEURL%%BOOST_FILES%"
  echo Unzipping %BOOST_FILES%
  unzip %UNZIP_ARGS% "%BOOST_FILES%"
  del %BOOST_FILES%
)

if not exist ..\..\src\gwt\lib (
  mkdir ..\..\src\gwt\lib
)
pushd ..\..\src\gwt\lib

if not exist gin\2.1.2 (
  wget %WGET_ARGS% "%BASEURL%%GIN_FILE%"
  mkdir gin\2.1.2
  echo Unzipping %GIN_FILE%
  unzip %UNZIP_ARGS% "%GIN_FILE%" -d gin\2.1.2
  del "%GIN_FILE%"
)

if not exist gwt\2.8.1 (
  wget %WGET_ARGS% "%BASEURL%%GWT_FILE%"
  echo Unzipping %GWT_FILE%
  unzip %UNZIP_ARGS% "%GWT_FILE%"
  mkdir gwt
  move gwt-2.8.1 gwt\2.8.1
  del "%GWT_FILE%"
)

if not exist %JUNIT_FILE% (
  wget %WGET_ARGS% "%BASEURL%%JUNIT_FILE%"
)

popd

pushd ..\common
set CORE_DICTIONARIES=core-dictionaries.zip
if not exist "dictionaries\en_US.dic" (
  wget %WGET_ARGS% "https://s3.amazonaws.com/rstudio-buildtools/dictionaries/%CORE_DICTIONARIES%"
  if exist "%CORE_DICTIONARIES%" (
     mkdir dictionaries
     echo Unzipping %CORE_DICTIONARIES%
     unzip %UNZIP_ARGS% "%CORE_DICTIONARIES%" -d dictionaries
     del "%CORE_DICTIONARIES%"
  )
)

set MATHJAX=mathjax-26.zip
if not exist "mathjax-26" (
  wget %WGET_ARGS% "https://s3.amazonaws.com/rstudio-buildtools/%MATHJAX%"
  if exist "%MATHJAX%" (
     mkdir mathjax-26
     echo Unzipping %MATHJAX%
     unzip %UNZIP_ARGS% "%MATHJAX%"
     del "%MATHJAX%"
  )
)

if not exist pandoc\%PANDOC_VERSION% (
  wget %WGET_ARGS% "%BASEURL%pandoc/%PANDOC_VERSION%/%PANDOC_FILE%"
  echo Unzipping %PANDOC_FILE%
  unzip %UNZIP_ARGS% "%PANDOC_FILE%"
  mkdir pandoc\%PANDOC_VERSION%
  copy "%PANDOC_NAME%\pandoc*" "pandoc\%PANDOC_VERSION%""
  del %PANDOC_FILE%
  rmdir /s /q %PANDOC_NAME%
)

if not exist libclang\%LIBCLANG_VERSION% (
  wget %WGET_ARGS% "%BASEURL%%LIBCLANG_FILE%"
  echo Unzipping %LIBCLANG_FILE%
  unzip %UNZIP_ARGS% "%LIBCLANG_FILE%"
  del %LIBCLANG_FILE%
)

if not "%CONDA_BUILD%" == "1" (
  call install-packages.cmd
)

popd

if not defined RSTUDIO_SKIP_QT (
  call install-qt-sdk-win.cmd
)

