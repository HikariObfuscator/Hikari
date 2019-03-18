# Hikari

[English Documentation](https://github.com/HikariObfuscator/Hikari/wiki)   
Hikari(Light in Japanese, name stolen from the Nintendo Switch game [Xenoblade Chronicles 2](http://www.nintendo.co.uk/Games/Nintendo-Switch/Xenoblade-Chronicles-2-1233955.html)) is my hackathon-ish toy project for the 2017 Christmas to kill time.It's already stable enough to use in production environment. However, as initially planned, Hikari  has been ported to LLVM 6.0 release version and no longer being actively maintained due to the time and effort it takes. You can find the history of its development at ``developer`` branch. Note that updates are not backported so you should probably always stick to the latest branch (``release_70`` at the time of writing). When newer LLVM versions got released, a new branch will be created containing the latest complete toolchain assembled from the release tarballs with Obfuscation Passes injected in.
# License
Hikari is relicensed from Obfuscator-LLVM and LLVM upstream's permissive NCSA license to GNU Affero General Public License Version 3 with exceptions listed below. tl;dr: The obfuscated LLVM IR and/or obfuscated binary is not restricted in anyway, however **any other project containing code from Hikari needs to be open source and licensed under AGPLV3 as well, even for web-based obfuscation services**. __**Even if your project doesn't contain any code from Hikari but rather linked to Libraries/Object Files containing code from Hikari, you'll still need to open-source your project under AGPLV3**__ , according to AGPLV3 License Text which you should have known in the beginning but I didn't explain carefully before

## Exceptions
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

## Compilation Issues
``fatal error: error in backend: Section too large, can't encode r_address (0x100006a) into 24 bits of scattered relocation entry.`` The obfuscated LLVM IR is way too complicated for proper relocation encoding in MachO, try adjust the obfuscation arguments a bit. Reference: [lib/Target/X86/MCTargetDesc/X86MachObjectWriter.cpp](https://github.com/llvm/llvm-project/blob/2946cd701067404b99c39fb29dc9c74bd7193eb3/llvm/lib/Target/X86/MCTargetDesc/X86MachObjectWriter.cpp#L418)

## Other Apple Compatibility Issues
Apple's Xcode uses a modified LLVM 6.0(~) to speed up build time and stuff. While Apple has stated they do plan to backport their changes back to the open-source LLVM, so far directly using LLVM upstream in Xcode still has some compatibility issues.

- ``-index-store-path`` ``cannot specify -o when generating multiple output files`` These two issues can be solved by turning off ``Enable Index While Building`` **ACROSS THE WHOLE PROJECT, INCLUDING SUB-PROJECTS** Note that for whatever reason that I have no interest in figuring out, newer Xcode versions seems to be ignoring this setting and still pass that option back nevertheless. This is a **known bug** and I honestly don't have the mood or the interest to even attempt to fix it.

It's probably less troublesome to port Hikari back to Apple's [SwiftLLVM](https://github.com/apple/swift), which you can find a simple tutorial on my [blog](https://mayuyu.io/2018/02/13/Porting-Hikari-to-Swift-Clang/), that being said setting the source up for the port is not for the faint hearted and each compilation takes more than three hours.

Or alternatively, you could try using [HikariObfuscator/NatsukoiHanabi](https://github.com/HikariObfuscator/NatsukoiHanabi) which injects the obfuscation code into Apple Clang, this is the least stable implementation but it at least works straight out of the box

## CocoaPods Integration
If you are using CocoaPods in your project, the following CocoaPods script by [@chenxk-j](https://github.com/chenxk-j) solves the aforementioned issues once and for all. For any queries regarding this script please contact him instead.

```
clang_rt_search_path = '/Library/Developer/Toolchains/Hikari.xctoolchain/usr/lib/clang/10.0.0/lib/darwin/'
# This path is different depending on how you are installing Hikari
# The 10.0.0 part depends on Xcode version and will change to 11.0.0 when Xcode enters 11.0.0 era
# Below is the alternative path if you are not using the pkg installer and are building and installing from source
# clang_rt_search_path = '~/Library/Developer/Toolchains/Hikari.xctoolchain/usr/lib/clang/10.0.0/lib/darwin/'

post_install do |installer|
    generator_group = installer.pods_project.main_group.find_subpath("Frameworks", true)
    files_references_hash = Hash.new
        clang_rt_reference = files_references_hash[target.platform_name.to_s]
        if clang_rt_reference == nil
            clang_rt_path = clang_rt_search_path + 'libclang_rt.'+target.platform_name.to_s+'.a'
            clang_rt_reference = generator_group.new_reference(clang_rt_path)
            files_references_hash[target.platform_name.to_s] = clang_rt_reference
        end
        target.add_file_references([clang_rt_reference])
        target.frameworks_build_phase.add_file_reference(clang_rt_reference, true)
        target.build_configurations.each do |config|
            origin_build_config = config.base_configuration_reference
            origin_build_config_parser = Xcodeproj::Config.new(origin_build_config.real_path)
            lib_search_path = origin_build_config_parser.attributes()['LIBRARY_SEARCH_PATHS']
            if lib_search_path != nil
                config.build_settings['LIBRARY_SEARCH_PATHS'] = lib_search_path + ' ' + clang_rt_search_path
            else
                config.build_settings['LIBRARY_SEARCH_PATHS'] = clang_rt_search_path
            end
            #ld: loaded libLTO doesn't support -bitcode_hide_symbols: LLVM version 7.0.0 for architecture armv7
            config.build_settings['HIDE_BITCODE_SYMBOLS'] = 'NO'
            #Issues:-index-store-path cannot specify -o when generating multiple output files
            config.build_settings['COMPILER_INDEX_STORE_ENABLE'] = 'NO'
        end
    end
end
```

# Building on Unix
Most parts are the same, you just remove all the commands related to Xcode

```
git clone -b release_70 https://github.com/HikariObfuscator/Hikari.git Hikari && mkdir Build && cd Build && cmake -G "Ninja" -DCMAKE_BUILD_TYPE=MinSizeRel -DLLVM_APPEND_VC_REV=on ../Hikari && ninja && ninja install && git clone https://github.com/HikariObfuscator/Resources.git ~/Hikari
```

# Building on Windows
You can either use a UNIX-Like environment like MinGW / Cygwin (Based upon community feedback. I personally have zero success building LLVM on Windows, even with official LLVM release tarballs.), or just edit ``lib/Transforms/Obfuscation/FunctionCallObfuscate.cpp`` ,remove ``#include <dlfcn.h>`` and then clean up all function bodies in this file, add ``return false`` if required. Since that thing isn't supported on Windows anyway and I have no plan to adding support for Windows to that Obfuscation Pass.

Plus, even if you managed to get things working, there is a chance that the LLVM Frontend Clang won't accept your code (Google ``MSVC nonstandard behavior``), so if you are using some non-standard compatible code, you might as well fall back to alternative solutions on Windows

# Commercial Version:
## Extra Features
- GlobalVariable Reference Obfuscation
- Target Jump Address in IndirectBranch Obfuscation
- Anti Disassembler on Certain Archs
- Constant Encryption
- Code Integrity Protection(aka Anti InlineHook,Anti Patching)(Currently supports iOS/macOS only)
- Swift 4.1 Support
- Support all terminators in Flattening, open-source version simply skips the whole function if it contains unsupported instruction
- Virtualization
- Syscall Lowering
- Other features
- And many bug fixes

##  Work In Progress Features in Commercial Version
- C++ RTTI Obfuscation

## But where do I buy it
As much as I'd love to profit a few thousands dollars a year from each user, you probably shouldn't. The open-source version is more than enough to defeat 99% of the script kiddies and hackers out there. The only cases you actually need the private version are:

- I'm too rich and I simply want the best
- My product contains very sensitive algorithms that needs to be well-protected.

Other than that, for usage scenarios like a DRM verification for your software, proper algorithm level implementation plus the open-source version of Hikari is probably a better choice.

## Still want it?
Send me an email after July 2019 so we can discuss further

# Demo   
**This only demonstrates a limited part of Hikari's capabilities. Download the complete demo and analyze yourself, link in the documentation**  
![AntiClassDump](https://github.com/HikariObfuscator/Hikari/blob/master/Images/AntiClassDump.jpeg?raw=true)  
![FunctionWrapper](https://github.com/HikariObfuscator/Hikari/blob/master/Images/FunctionWrapper.jpeg?raw=true)  
![IndirectBranch](https://github.com/HikariObfuscator/Hikari/blob/master/Images/IndirectBranch.jpeg?raw=true)
![InstructionReplacement](https://github.com/HikariObfuscator/Hikari/blob/master/Images/InstructionReplacement.jpeg?raw=true)
![StringEncryption](https://github.com/HikariObfuscator/Hikari/blob/master/Images/StringEncryption.jpeg?raw=true)
