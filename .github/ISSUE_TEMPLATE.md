## Opening an issue
Please fill out the following form with as much detail as possible. **Failing to do so might get yourself blocked.**

- ![#f03c15](https://placehold.it/15/f03c15/000000?text=+)**Required**      
Affected source code or LLVM IR. It's best for you to create minimal source code that could reproduce the issue. Alternatively LLVM IR could be obtained by adding ``-S -emit-llvm`` to your normal CFLAGS or compile a normal binary with Bitcode Enabled.
- ![#f03c15](https://placehold.it/15/f03c15/000000?text=+)**Required**  
Obfuscation Flags used.
- ![#f03c15](https://placehold.it/15/ffa500/000000?text=+)**Strongly Recommend**      
LLVM's Logs in Debug mode if the issue is about crashed compiling process
- ![#f03c15](https://placehold.it/15/ffa500/000000?text=+)**Strongly Recommend**    
Misbehaving passes. Try toggling pass switches to find out what pass(es) is triggering the issue
- ![#f03c15](https://placehold.it/15/ffa500/000000?text=+)**Strongly Recommend**    
Details regarding the target platform

## 创建一个Issue
请按照如下模版尽可能详细的填写资料。**不按照规则提供我解决您的问题所需的信息将很有可能导致您的帐号被我的私人账号和Hikari组织永久黑名单**
- ![#f03c15](https://placehold.it/15/f03c15/000000?text=+)**必须**      
受影响源码或LLVM IR. 源码最好是最小复现问题的代码。IR可通过在CFLAGS末尾加上-S -emit-llvm或编译包含Bitcode的二进制获得。注意后者目前只支持iOS
- ![#f03c15](https://placehold.it/15/f03c15/000000?text=+)**必须**  
使用的混淆参数列表
- ![#f03c15](https://placehold.it/15/ffa500/000000?text=+)**强烈建议提供**  
LLVM的完整日志
- ![#f03c15](https://placehold.it/15/ffa500/000000?text=+)**强烈建议提供**  
有问题的Pass,试着分别打开关闭混淆的pass来找出是哪个(些)Pass的问题
- ![#f03c15](https://placehold.it/15/ffa500/000000?text=+)**强烈建议提供**  
目标平台的细节。例如操作系统，处理器架构等。

Please Refer to the following examples on how to create a issue that I could possibly provide any sort of help:  
请参阅下文的例子来创建有足够的信息来让我提供任何形式帮助的Issue:  

- [#41](https://github.com/HikariObfuscator/Hikari/issues/41) 
- [#56](https://github.com/HikariObfuscator/Hikari/issues/56) 
- [#57](https://github.com/HikariObfuscator/Hikari/issues/57) 
