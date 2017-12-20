# Hikari
Hikari is a port of Obfuscator-LLVM with a few custom built passes.
## Source
Available at master branch
## Install

For macOS,Download from Releases page,Extract Hikari.xctoolchain to ``~/Library/Developer/``.  
For other platforms, compile and install accordingly. See [BuildScript](https://gist.github.com/Naville/f1d8ea43ffde61f57497492d599b32fb)

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
Note that LTO Obfuscations requires passing arguments seperately.Example of enable everything from command line would be:
``
-mllvm -enable-allobf -mllvm -enable-fco -flto
``  
# TODO
- StringEncryption
- Generate unobfuscated dSym
- Improved CFG Obfuscation Algorithm
- Complete Anti-Debuging

Zhang
