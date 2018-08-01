@echo off
:: ##########################################################################################
:: Bat-native-ledger-isolation-test environment
:: ##########################################################################################

set "ROOT=%cd%"
set PULLLOCKFILE=.pulllock
if exist %PULLLOCKFILE% (
  echo You have pulled dependencies already. 
  exit /b 0
)

type nul > %PULLLOCKFILE%

:: Commits the solution is bound to
set bat-native-anonize_commit=5e3e8eb137a1837a136a0d364ece01d0cdae6098
set bat-native-ledger_commit=c373c55313edc97b1529b21bd8634e78c51b4777
set bat-native-rapidjson_commit=744b43313525a047eda4f2e2e689aa88b6c596fa
set bat-native-tweetnacl_commit=d61f0cdc88dd2c4320176d2c514b8dd8dd1f22c2
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

cd %ROOT%\bat-native-rapidjson
git checkout  %bat-native-rapidjson_commit%

cd %ROOT%\bat-native-tweetnacl
git checkout  %bat-native-tweetnacl_commit%

cd %ROOT%\bip39wally-core-native
git checkout  %bip39wally-core-native_commit%

cd %ROOT%\boringssl
git checkout  %boringssl_commit%

cd %ROOT%\curl
git checkout  %curl_commit%

cd %ROOT%\leveldb
git checkout  %leveldb_commit%

cd %ROOT%\snappy
git checkout  %snappy_commit%

cd %ROOT%