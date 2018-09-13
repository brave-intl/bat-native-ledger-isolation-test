@echo off
:: ##########################################################################################
:: Bat-native-ledger-isolation-test environment
:: ##########################################################################################

set "ROOT=%cd%"
set PATCHES=%ROOT%\patches
set PULLLOCKFILE=.pulllock

if exist %PULLLOCKFILE% (
  echo You have pulled dependencies already. To force it: remove .pulllock file and re-run.
  exit /b 0
)

type nul > %PULLLOCKFILE%

git submodule update --init --recursive


cd %ROOT%\bat-native-ledger
git apply %PATCHES%\bat-native-ledger\bat-native-ledger.patch

cd %ROOT%\bip39wally-core-native
git apply %PATCHES%\bip39wally-core-native\bip39wally-core-native.patch

cd %ROOT%\boringssl
copy /Y %PATCHES%\boringssl\err_data.c .

cd %ROOT%\curl
copy /Y %PATCHES%\curl\curl_config.h .

cd %ROOT%\leveldb
git apply %PATCHES%\leveldb\leveldb.patch

cd %ROOT%\snappy
copy /Y %PATCHES%\snappy\config.h .
copy /Y %PATCHES%\snappy\snappy-stubs-public.h .

cd %ROOT%
