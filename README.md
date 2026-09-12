\# 模拟设备 TCP 服务端



使用 Qt Console、QTcpServer 和 QTcpSocket 实现。



\## 功能



\- 监听 127.0.0.1:5000。

\- 接收客户端连接，记录连接和断开。

\- 按换行符读取完整命令。

\- 使用 Device 状态机处理启动、停止和复位。

\- 返回执行结果或错误原因。



\## 通信格式



使用 UTF-8，每条命令和回复以换行符结束。



支持命令：

\- START：启动设备。

\- STOP：停止设备。

\- RESET：复位设备。

\- GET\_STATUS：查询状态。



回复示例：

\- OK|running

\- STATE|stopped

\- ERR|INVALID\_STATE|running

\- ERR|UNKNOWN\_COMMAND|idle



\## 运行方法



1\. 使用 Qt Creator 打开 CMakeLists.txt，构建并运行服务端。

2\. 启动 day15\_qt\_window 客户端。

3\. 通过客户端按钮发送命令，在日志中查看回复。



\## 已验证



\- 客户端连接、断开正常。

\- START、STOP、RESET 按状态机规则执行。

\- 非法状态下的命令被拒绝。

\- GET\_STATUS 返回当前状态。

\- 客户端重启后，服务端设备状态仍保留。



\## 当前限制



\- 设备状态只保存在内存中，服务端重启后恢复 idle。

\- 尚未连接真实设备。

\- 客户端目前仅在日志中显示回复，界面状态尚未同步。

