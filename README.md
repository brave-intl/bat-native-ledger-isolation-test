## MS Visual Studio isolated testing/debugging environment for bat-native-ledger project.

It is made to test and debug the bat-native-ledger library in isolated environment excluding interaction with hosting browser application. 
The solution is based on MS Visual Studio solution and projects, and allows using all power of VS IDE.
It includes all bat-native-ledger's dependencies as submodules, bat-native-ledger itself, test driver application and curl library for handling http/https communication. 
The solution has Debug and Release configurations for x86 and x86_64 platforms. 
Some of the dependencies have pre-generated configuration files and minor patches to make code compilable in VS. 
(The config files and patches are located in **'patches'** directory.)

## Branch/commit of each component used in the solution.  

All libraries,including bat-native-ledger are moving forward and their interfaces are changing. The solution is bound to a particular branch/commit of each component.
Switching to a newer commit is possible but will require some work to put it together.

* **bat-native-anonize**:  win-port/5e3e8eb137a1837a136a0d364ece01d0cdae6098
* **bat-native-ledger**: master/387c78b347aa55e96823c4685c26cc650e5384ec
* **bat-native-rapidjson**: master/744b43313525a047eda4f2e2e689aa88b6c596fa
* **bat-native-tweetnacl**: master/05ed8f82faa03609fe5ae0a4c2d454afbe2ff267
* **bip39wally-core-native**: master/e5aba371a56d3e41f7e80e868312446ce7bd434c 
* **boringssl**: master/0080d83b9faf8dd325f5f5f92eb56faa93864e4c 
* **curl**: master/7212c4cd607af889c9adc47030a84b6f8ac3b0f6 
* **leveldb**: windows/ad834a20a651ebcabf7c03a88712e780a965d4e3 
* **snappy**: master/4f7bd2dbfd12bfda77488baf46c2f7648c9f1999 

Switching to a new commit will also require updating this **README.md** and **pull-depends.bat** files.

## Prerequisites:

* The **leveldb** for windows depends on [Boost C++ library](https://www.boost.org/). The easiest way is to install pre-built Windows binaries and source files from [Boost download page](https://www.boost.org/users/download).
Configurations for **x86** or **x86_64** will require the matching set of libraries. The location of Boost include directory and libs can be configured in projects properties files **globals.props**, **x86.props** and **x86_64.props** (in 'msvcpp' directory) in variables **$(Boost)** and **$(BoostLib)**.

## Initial setup:

To bring all the dependencies, to switch to the right commit and to apply patches please run `pull-depends.bat` from the repository root directory. 

## Testing:

The bat-native-ledger library is using threads to run tasks and callbacks. 
Use **BraveRewardsService::TestingJoinAllRunningTasks()** call after each test to wait for all spawned tasks/callbacks.
