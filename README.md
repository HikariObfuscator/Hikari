# Hikari

[English Documentation](https://github.com/HikariObfuscator/Hikari/wiki)   
Hikari(Light in Japanese, name stolen from the Nintendo Switch game [Xenoblade Chronicles 2](http://www.nintendo.co.uk/Games/Nintendo-Switch/Xenoblade-Chronicles-2-1233955.html)) is my hackathon-ish toy project for the 2017 Christmas to kill time.It's already stable enough to use in production environment. However, as initially planned, Hikari  has been ported to LLVM 6.0 release version and no longer being actively maintained due to the time and effort it takes. You can find the history of its development at ``developer`` branch. Further enhancements include more features like Code-Intergrity Checking and a full anti-hook implementation. These are not open-source and will probably be released as a commercial product. If you know me close enough we can discuss the license model and pricing issue because I might not be able to provide real-time bug fix and stuff.

# License
Hikari is relicensed from Obfuscator-LLVM and LLVM upstream's permissive NCSA license to GNU Affero General Public License Version 3 with exceptions listed below. tl;dr: The obfuscated LLVM IR and/or obfuscated binary is not restricted in anyway, however **any other project containing code from Hikari needs to be open source and licensed under AGPLV3 as well, even for web-based obfuscation services**.  

## License
- Anyone who has associated with ByteDance in anyway at any past, current, future time point is prohibited from direct using this piece of software or create any derivative from it

# Prerequisites
Prerequisites are exactly the same as LLVM Upstream.
You need fairly new version of the following packages to compile LLVM.
- SWIG
- Python
- CMake
- GCC/Clang
- zlib

```
Additionally, your compilation host is expected to have the usual plethora of Unix utilities. Specifically:

ar — archive library builder
bzip2 — bzip2 command for distribution generation
bunzip2 — bunzip2 command for distribution checking
chmod — change permissions on a file
cat — output concatenation utility
cp — copy files
date — print the current date/time
echo — print to standard output
egrep — extended regular expression search utility
find — find files/dirs in a file system
grep — regular expression search utility
gzip — gzip command for distribution generation
gunzip — gunzip command for distribution checking
install — install directories/files
mkdir — create a directory
mv — move (rename) files
ranlib — symbol table builder for archive libraries
rm — remove (delete) files and directories
sed — stream editor for transforming output
sh — Bourne shell for make build scripts
tar — tape archive for distribution generation
test — test things in file system
unzip — unzip command for distribution checking
zip — zip command for distribution generation
```


# macOS Quick Install
This script assumes current working directory is not the user's home directory(aka ``~/``). ``cd`` to some where else if this is the case.  This script also assumes you have ``cmake`` and ``ninja`` installed, if not, use [Homebrew](https://brew.sh) and similar package managers to install them    

```
git clone -b release_70 https://github.com/HikariObfuscator/Hikari.git Hikari && mkdir Build && cd Build && cmake -G "Ninja" -DLLDB_CODESIGN_IDENTITY='' -DCMAKE_BUILD_TYPE=MinSizeRel -DLLVM_APPEND_VC_REV=on -DLLVM_CREATE_XCODE_TOOLCHAIN=on -DCMAKE_INSTALL_PREFIX=~/Library/Developer/ ../Hikari && ninja &&ninja install-xcode-toolchain && git clone https://github.com/HikariObfuscator/Resources.git ~/Hikari && rsync -a --ignore-existing /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/ ~/Library/Developer/Toolchains/Hikari.xctoolchain/ && rm ~/Library/Developer/Toolchains/Hikari.xctoolchain/ToolchainInfo.plist
```

## Building issues
- ```CMake Error at tools/xcode-toolchain/CMakeLists.txt:80 (message): Could not identify toolchain dir```
 Make sure you have Xcode installed and ``xcode-select -p`` points to ``/Applications/Xcode.app/Contents/Developer`` instead of the standalone macOS Toolchain

## Other Apple Compatibility Issues
Apple's Xcode uses a modified LLVM 6.0(~) to speed up build time and stuff. While Apple has stated they do plan to backport their changes back to the open-source LLVM, so far directly using LLVM upstream in Xcode still has some compatibility issues.

- ``-index-store-path`` ``cannot specify -o when generating multiple output files`` These two issues can be solved by turning off ``Enable Index While Building`` **ACROSS THE WHOLE PROJECT, INCLUDING SUB-PROJECTS** Note that for whatever reason that I have no interest in figuring out, newer Xcode versions seems to be ignoring this setting and still pass that option back nevertheless. This is a **known bug** and I honestly don't have the mood or the interest to even attempt to fix it.

It's probably less troublesome to port Hikari back to Apple's [SwiftLLVM](https://github.com/apple/swift), which you can find a simple tutorial on my [blog](https://mayuyu.io/2018/02/13/Porting-Hikari-to-Swift-Clang/), that being said setting the source up for the port is not for the faint hearted and each compilation takes more than three hours.

Or alternatively, you could try using [HikariObfuscator/NatsukoiHanabi](https://github.com/HikariObfuscator/NatsukoiHanabi) which injects the obfuscation code into Apple Clang, this is the least stable implementation but it at least works straight out of the box

# Building on Unix
Most parts are the same, you just remove all the commands related to Xcode

```
git clone -b release_70 https://github.com/HikariObfuscator/Hikari.git Hikari && mkdir Build && cd Build && cmake -G "Ninja" -DCMAKE_BUILD_TYPE=MinSizeRel -DLLVM_APPEND_VC_REV=on ../Hikari && ninja && ninja install && git clone https://github.com/HikariObfuscator/Resources.git ~/Hikari
```

# Building on Windows
You can either use a UNIX-Like environment like MinGW / Cygwin (Based upon community feedback. I personally have zero success building LLVM on Windows, even with official LLVM release tarballs.), or just edit ``lib/Transforms/Obfuscation/FunctionCallObfuscate.cpp`` ,remove ``#include <dlfcn.h>`` and then clean up all function bodies in this file, add ``return false`` if required. Since that thing isn't supported on Windows anyway and I have no plan to adding support for Windows to that Obfuscation Pass.

Plus, even if you managed to get things working, there is a chance that the LLVM Frontend Clang won't accept your code (Google ``MSVC nonstandard behavior``), so if you are using some non-standard compatible code, you might as well fall back to alternative solutions on Windows

# Extra Features in Commercial Version:
- GlobalVariable Reference Obfuscation
- Target Jump Address in IndirectBranch Obfuscation
- Anti Disassembler on Certain Archs
- Constant Encryption
- Code Integrity Protection(aka Anti InlineHook,Anti Patching)(Currently supports iOS/macOS only)
- Swift 4.1 Support
- Support all terminators in Flattening, open-source version simply skips the whole function if it contains unsupported instruction
- Virtualization
- Other features
- And many bug fixes

# Work In Progress Features in Commercial Version
- C++ RTTI Obfuscation
- Syscall Lowering

# Demo   
**This only demonstrates a limited part of Hikari's capabilities. Download the complete demo and analyze yourself, link in the documentation**  
![AntiClassDump](https://github.com/HikariObfuscator/Hikari/blob/master/Images/AntiClassDump.jpeg?raw=true)  
![FunctionWrapper](https://github.com/HikariObfuscator/Hikari/blob/master/Images/FunctionWrapper.jpeg?raw=true)  
![IndirectBranch](https://github.com/HikariObfuscator/Hikari/blob/master/Images/IndirectBranch.jpeg?raw=true)
![InstructionReplacement](https://github.com/HikariObfuscator/Hikari/blob/master/Images/InstructionReplacement.jpeg?raw=true)
![StringEncryption](https://github.com/HikariObfuscator/Hikari/blob/master/Images/StringEncryption.jpeg?raw=true)
