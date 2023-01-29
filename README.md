# Deprecated

As a toy project, Hikari had way too many design issues that makes it irrelavant in modern binary protection.  
You really shouldn't be using it anymore, instead, switch to a commercially supported implementation. 
If you absolutely must use Hikari, switch to a actively maintained fork like [NeHyci/Hikari-LLVM15](https://github.com/NeHyci/Hikari-LLVM15) where I occasionally show up and provide some insights / hints/ help.

I also occasionally analyze existing open-source/commericial LLVM obfuscators in a professional setup, for analysis reports that are not restricted by an NDA, you can see a stripped down version of my reports [here](https://github.com/Naville/Nitpicking-OpenSourceObfuscators)


# Hikari

[English Documentation](https://github.com/HikariObfuscator/Hikari/wiki)   
Hikari(Light in Japanese, name stolen from the Nintendo Switch game [Xenoblade Chronicles 2](http://www.nintendo.co.uk/Games/Nintendo-Switch/Xenoblade-Chronicles-2-1233955.html)) is [Naville](https://github.com/Naville)'s 2017 Christmas Toy Project. 

New features are not expected to be open-sourced and instead the focus would be compatibility with future LLVM versions and Xcode versions.

# License
Please refer to [License](https://github.com/HikariObfuscator/Hikari/wiki/License). 

Note that this linked version of license text overrides any artifact left in source code

# Building
See [Compile & Install](https://github.com/HikariObfuscator/Hikari/wiki/Compile-&-Install)

# Security
All releases prior to and including LLVM8 are signed using [this PGP Key](https://keybase.io/navillezhang/pgp_keys.asc) from [Naville](https://github.com/Naville) . Verifiable on his [Keybase](https://keybase.io/navillezhang).

 
# Demo   
**This only demonstrates a limited part of Hikari's capabilities. Download the complete demo and analyze yourself, link in the documentation**  
![AntiClassDump](https://github.com/HikariObfuscator/Hikari/blob/master/Images/AntiClassDump.jpeg?raw=true)  
![FunctionWrapper](https://github.com/HikariObfuscator/Hikari/blob/master/Images/FunctionWrapper.jpeg?raw=true)  
![IndirectBranch](https://github.com/HikariObfuscator/Hikari/blob/master/Images/IndirectBranch.jpeg?raw=true)
![InstructionReplacement](https://github.com/HikariObfuscator/Hikari/blob/master/Images/InstructionReplacement.jpeg?raw=true)
![StringEncryption](https://github.com/HikariObfuscator/Hikari/blob/master/Images/StringEncryption.jpeg?raw=true)
