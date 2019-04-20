# Hikari [![](https://img.shields.io/badge/chat-on--discord-7289da.svg?style=flat-square&longCache=true&logo=discord)](https://discord.gg/ebrfvfm)

[English Documentation](https://github.com/HikariObfuscator/Hikari/wiki)   
Hikari(Light in Japanese, name stolen from the Nintendo Switch game [Xenoblade Chronicles 2](http://www.nintendo.co.uk/Games/Nintendo-Switch/Xenoblade-Chronicles-2-1233955.html)) is my hackathon-ish toy project for the 2017 Christmas to kill time.It's already stable enough to use in production environment. However, as initially planned, Hikari  has been ported to LLVM 6.0 release version and no longer being actively maintained due to the time and effort it takes. You can find the history of its development at ``developer`` branch. Note that updates are not backported so you should probably always stick to the latest branch (``release_70`` at the time of writing). When newer LLVM versions got released, a new branch will be created containing the latest complete toolchain assembled from the release tarballs with Obfuscation Passes injected in.
# License
Hikari is relicensed from Obfuscator-LLVM and LLVM upstream's permissive NCSA license to GNU Affero General Public License Version 3 with exceptions listed below. tl;dr: The obfuscated LLVM IR and/or obfuscated binary is not restricted in anyway, however **any other project containing code from Hikari needs to be open source and licensed under AGPLV3 as well, even for web-based obfuscation services**. __**Even if your project doesn't contain any code from Hikari but rather linked to Libraries/Object Files containing code from Hikari, you'll still need to open-source your project under AGPLV3**__ , according to AGPLV3 License Text which you should have known in the beginning but I didn't explain carefully before

## Exceptions
- Anyone who has associated with ByteDance in anyway at any past, current, future time point is prohibited from direct using this piece of software or create any derivative from it

# Building
See [Compile & Install](https://github.com/HikariObfuscator/Hikari/wiki/Compile-&-Install)

 
# Demo   
**This only demonstrates a limited part of Hikari's capabilities. Download the complete demo and analyze yourself, link in the documentation**  
![AntiClassDump](https://github.com/HikariObfuscator/Hikari/blob/master/Images/AntiClassDump.jpeg?raw=true)  
![FunctionWrapper](https://github.com/HikariObfuscator/Hikari/blob/master/Images/FunctionWrapper.jpeg?raw=true)  
![IndirectBranch](https://github.com/HikariObfuscator/Hikari/blob/master/Images/IndirectBranch.jpeg?raw=true)
![InstructionReplacement](https://github.com/HikariObfuscator/Hikari/blob/master/Images/InstructionReplacement.jpeg?raw=true)
![StringEncryption](https://github.com/HikariObfuscator/Hikari/blob/master/Images/StringEncryption.jpeg?raw=true)
