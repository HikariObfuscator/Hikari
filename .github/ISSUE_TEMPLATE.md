## Opening an issue
If you provide one or more of the stuff listed below, it'll be a lot faster for me and future fellow contributors to help you

- Affected source code. It's best for you to create minimal source code that could reproduce the issue
- LLVM's Logs in Debug mode if the issue is about crashed compiling process
- Misbehaving passes. Try toggling pass switches to find out what pass(es) is triggering the issue
- Details regarding the target platform

## 创建一个Issue
请尽可能多的提供下述的信息。否则恕无能为力
- 受影响源码或LLVM IR. 源码最好是最小复现问题的代码。IR可通过在CFLAGS末尾加上-S -emit-llvm获得
- LLVM的完整日志
- 有问题的Pass,试着分别打开关闭混淆的pass来找出是哪个(些)Pass的问题
- 目标平台的细节。例如操作系统，处理器架构等。
