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

:: Commits the solution is bound to
set bat-native-anonize_commit=5e3e8eb137a1837a136a0d364ece01d0cdae6098
set bat-native-ledger_commit=f08c7b09a2d927fe50860923e523960f702ea492
set bat-native-rapidjson_commit=744b43313525a047eda4f2e2e689aa88b6c596fa
set bat-native-tweetnacl_commit=05ed8f82faa03609fe5ae0a4c2d454afbe2ff267
set bip39wally-core-native_commit=e5aba371a56d3e41f7e80e868312446ce7bd434c 
set boringssl_commit=0080d83b9faf8dd325f5f5f92eb56faa93864e4c 
set curl_commit=7212c4cd607af889c9adc47030a84b6f8ac3b0f6 
set leveldb_commit=ad834a20a651ebcabf7c03a88712e780a965d4e3 
set snappy_commit=4f7bd2dbfd12bfda77488baf46c2f7648c9f1999 

git submodule update --init --recursive


:: checkout the expected commit 
cd %ROOT%\bat-native-anonize
git checkout  %bat-native-anonize_commit%

cd %ROOT%\bat-native-ledger
git checkout  %bat-native-ledger_commit%
git apply %PATCHES%\bat-native-ledger\bat-native-ledger.patch

cd %ROOT%\bat-native-rapidjson
git checkout  %bat-native-rapidjson_commit%

cd %ROOT%\bat-native-tweetnacl
git checkout  %bat-native-tweetnacl_commit%

cd %ROOT%\bip39wally-core-native
git checkout  %bip39wally-core-native_commit%
git apply %PATCHES%\bip39wally-core-native\bip39wally-core-native.patch

cd %ROOT%\boringssl
git checkout  %boringssl_commit%
copy /Y %PATCHES%\boringssl\err_data.c .

cd %ROOT%\curl
git checkout  %curl_commit%
copy /Y %PATCHES%\curl\curl_config.h .

cd %ROOT%\leveldb
git checkout  %leveldb_commit%
git apply %PATCHES%\leveldb\leveldb.patch

cd %ROOT%\snappy
git checkout  %snappy_commit%
copy /Y %PATCHES%\snappy\config.h .
copy /Y %PATCHES%\snappy\snappy-stubs-public.h .

cd %ROOT%
