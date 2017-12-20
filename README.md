# Hikari
Hikari is a port of Obfuscator-LLVM with a few custom built passes.
## Source
Available at master branch
## Install

For macOS,Download from Releases page,Extract Hikari.xctoolchain to ``~/Library/Developer/``.  Note that I do not personally use Xcode as such I'm not able to help with issues regarding Xcode Intergration  

For other platforms, compile and install accordingly. See [BuildScript](https://gist.github.com/Naville/f1d8ea43ffde61f57497492d599b32fb)
Note that this repo currently contains only the LLVM Core, see [Getting Started with the LLVM System](http://llvm.org/docs/GettingStarted.html) for installing other components like Clang

## Usage
Add one of the following to CFLAGS

```  
enable-fco  Enable Function CallSite Obfuscation.Use with LTO  
enable-symobf Enable Symbol Obfuscation.Use with LTO  
enable-bcfobf Enable BogusControlFlow  
enable-cffobf Enable Flattening  
enable-splitobf Enable BasicBlockSpliting  
enable-subobf Enable Instruction Substitution  
enable-allobf Enable All Non-LTO Obfuscation  
enable-adb Enable AntiDebugging Mechanisms
```
An example of invoking everything from command line would be:  
``/PATH/TO/OUR/clang -mllvm -enable-allobf -Xlinker -mllvm -Xlinker -enable-fco main.m -flto``

# TODO
- StringEncryption
- Generate unobfuscated dSym
- Improved CFG Obfuscation Algorithm
- Complete Anti-Debuging

## Intergrating with Xcode
- Compile From Scratch or Download XcodeToolchain From Releases
- Copy ``utils/Hikari.xcplugin`` to ``/Applications/Xcode.app/Contents/PlugIns/Xcode3Core.ideplugin/Contents/SharedSupport/Developer/Library/Xcode/Plug-ins/``
- Under Project Settings. Search for ``Enable Index-While-Building Functionality`` to NO. As mentioned by [obfuscator-llvm/obfuscator/issues/86](https://github.com/obfuscator-llvm/obfuscator/issues/86)
- EDIT CFLAGS as you wish.
- In ``Xcode->Toolchains``, select Hikari
## Troubleshooting
- You might run into errors like ``ld: file not found: /Library/Developer/Toolchains/Hikari.xctoolchain/usr/lib/arc/libarclite_macosx.a``. this is due to Xcode's Default Toolchain, located at ``/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/`` contains stuff not packed into Hikari.Just copy corresponding directories over
## Known Issues
On Darwin seems like ``__DARWIN_ALIAS_C`` is messing up symbols, results in a failed dlsym() and thus crush the process.
