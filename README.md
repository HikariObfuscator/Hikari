# Hikari [![](https://img.shields.io/badge/chat-on--discord-7289da.svg?style=flat-square&longCache=true&logo=discord)](https://discord.gg/ebrfvfm)

[English Documentation](https://github.com/HikariObfuscator/Hikari/wiki)   
Hikari(Light in Japanese, name stolen from the Nintendo Switch game [Xenoblade Chronicles 2](http://www.nintendo.co.uk/Games/Nintendo-Switch/Xenoblade-Chronicles-2-1233955.html)) is my hackathon-ish toy project for the 2017 Christmas to kill time.It's already stable enough to use in production environment. However, as initially planned, Hikari  has been ported to LLVM 6.0 release version and no longer being actively maintained due to the time and effort it takes. You can find the history of its development at ``developer`` branch. Note that updates are not backported so you should probably always stick to the latest branch (``release_70`` at the time of writing). When newer LLVM versions got released, a new branch will be created containing the latest complete toolchain assembled from the release tarballs with Obfuscation Passes injected in.
# License
Please refer to [License](https://github.com/HikariObfuscator/Hikari/wiki/License). 

Note that this linked version of license text overrides any artifact left in source code

# Building
See [Compile & Install](https://github.com/HikariObfuscator/Hikari/wiki/Compile-&-Install)

# Security
All releases are signed using the following PGP public key. You can also use this key to verify any other important discussion/message is signed by me. You can verify the key indeed belongs to me by visiting my [Keybase](https://keybase.io/navillezhang).

```
-----BEGIN PGP PUBLIC KEY BLOCK-----
Comment: GPGTools - https://gpgtools.org

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
q3pA5N/3Lw4q1QoP2vDe+ZcChFuT2A==
=UqHX
-----END PGP PUBLIC KEY BLOCK-----
```
 
# Demo   
**This only demonstrates a limited part of Hikari's capabilities. Download the complete demo and analyze yourself, link in the documentation**  
![AntiClassDump](https://github.com/HikariObfuscator/Hikari/blob/master/Images/AntiClassDump.jpeg?raw=true)  
![FunctionWrapper](https://github.com/HikariObfuscator/Hikari/blob/master/Images/FunctionWrapper.jpeg?raw=true)  
![IndirectBranch](https://github.com/HikariObfuscator/Hikari/blob/master/Images/IndirectBranch.jpeg?raw=true)
![InstructionReplacement](https://github.com/HikariObfuscator/Hikari/blob/master/Images/InstructionReplacement.jpeg?raw=true)
![StringEncryption](https://github.com/HikariObfuscator/Hikari/blob/master/Images/StringEncryption.jpeg?raw=true)
