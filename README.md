# swfrztool
**内核级希沃冰点还原管理工具**

## 概述
> 警告: 请在win10 x64及以上且安装希沃管家的环境中运行本程序 否则无法保证系统安全

本项目放弃使用传统的希沃自己的API 而是逆向`SWFreeze`、`SeewoKeLiteLady`等驱动 并在R0下修改冰点保护状态以及配置文件

实现了 **即时解除冰点还原保护而无需重启** **模拟冰点还原保护状态** **还原白名单控制**

本程序仅提供可使用的接口 日常使用较为复杂\(尤其是白名单控制\) 之后将会为本程序开发GUI程序

## 使用说明
使用具有**管理员权限**的命令行启动程序

### config 修改冰点还原配置
```
swfrztool cfg <drive1> <drive2>...
```
修改冰点还原的配置文件 可以自定义保护卷 修改后重启生效

### mjfunc 设置磁盘驱动分发例程
```
swfrztool mjfunc <0|1>
```
可恢复冰点还原驱动hook的`读` `写` `ioctl`分发例程\(`pnp`用于U盘管理 不做恢复\) 

`0` 使用SWFreeze.sys hook的分发例程

`1` 恢复disk.sys的原生分发例程 此时可以直接读写磁盘 重启后失效

### volume 临时修改卷保护状态
```
swfrztool volume <drive1> <drive2>... <0|1>
```
若开启了冰点还原 针对于受保护卷 可以临时修改卷的受保护状态 重启失效

`0` 禁用保护

`1` 开启保护

### whitelist 白扇区位图控制
>注意: 若设置后文件扇区扩展\(如: 向文件增加大量内容\) 则必须重新设置 所以建议先将白扇区位图全部置1 然后再设置受保护的文件\(即设置黑名单\)

`file` 计算指定文件所占扇区扇区\(数据流、MFT记录、父目录索引\) 修改白扇区位图 实现直接操作指定文件 暂不支持目录
```
swfrztool whitelist file <filepath> <0|1>
```
`0` 扇区重定向

`1` 直接读写

`sec` 设置白扇区位图中指定扇区所在位的值
```
swfrztool whitelist sec <drive> <start> <length> <0|1>
```
扇区数和长度必须是8的倍数

`in` 导入白扇区位图
```
swfrztool whitelist in <drive> <start> <length> <filepath>
```
扇区数和长度必须是8的倍数

`out` 导出白扇区位图
```
swfrztool whitelist out <drive> <start> <length> <filepath>
```
扇区数和长度必须是8的倍数

### flt 模拟冰点还原状态
> 注意: 该功能会自动劫持管家的运行库用于持久化而非开机启动项\(因为开机自启动过早会触发**BSOD**\) 所以开启后请勿移动本程序位置或删除 否则请重新调用flt
```
swfrztool flt <drive1/off> <drive2>...
```
集控会在关闭冰点还原一段时间后自动打开 所以我开发了这个功能

程序通过hook`SeewoKeLiteLady`的文件过滤回调重定向配置文件 所以使用前确保加载该驱动 并且在使用前先关闭冰点还原

使用off选项禁用

### info 获取冰点还原相关信息

### help 获取帮助

## 示例
```
swfrztool cfg C D                              # 修改配置为保护卷C D
swfrztool cfg                                  # 修改配置为不保护
swfrztool mjfunc 1                             # 恢复disk.sys分发例程
swfrztool volume C D 0                         # 禁用卷C D的保护
swfrztool whitelist file C:\file.txt 1         # 使C:\file.txt可以直接读写
swfrztool whitelist sec C 0 1024 1             # 设置卷C的扇区0-1024可以直接读写
swfrztool whitelist in C 0 1024 C:\bmp.bin     # 导入卷C的0-1024的白扇区位图
swfrztool whitelist out C 0 1024 C:\bmp.bin    # 导出卷C的0-1024的白扇区位图
```

## 相关视频
[B站个人主页](https://space.bilibili.com/3461574094228294)

[希沃冰点还原管理工具原理分析](https://www.bilibili.com/video/BV17pEM6NERi/)

[希沃冰点还原逆向驱动+内核级绕过思路](https://www.bilibili.com/video/BV1Kk9MBNEeb/)

[\[彻底逆向\]希沃冰点还原驱动扇区重定向逆向分析](https://www.bilibili.com/video/BV1V6K16NEqh/)

## 关于项目
本项目部分引用了R0提权项目`msrexec`和md5计算项目

除此之外 本项目所有代码 包括逆向和思路均有本人独立完成 与网络上其他有关希沃的项目无关

本项目使用的漏驱为公开的漏驱 若开启了更严格的签名校验可能无法加载 可以自己寻找能修改`msr寄存器`的驱动

## 更新 \(版本控制随缘\)
### V1.0 
1.完成了基础功能

### V1.1
1.更新了flt参数 实现自动持久化

2.flt参数劫持的回调留了后门用于区分敌我 使用config参数时无需禁用flt

### V1.2
1.更新了info参数

2.优化代码

### V1.3
1.增加了白名单控制

2.更改了配置管理的逻辑

3.重构了命令行参数的使用

4.优化了大量代码

### V1.3\(fix\)
1.修复了保护状态模拟的一个低级错误
