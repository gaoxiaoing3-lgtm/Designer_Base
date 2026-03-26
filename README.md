# Designer_Base

Unreal在线包装软件。

## 简介

`Designer_Base` 是一个基于 Unreal Engine 的在线包装原型项目，包含：

- Lower third 在线控制与播出
- 网页端控制台
- Unreal 运行时联机接收与状态回传
- 本地 Node 控制服务

## 目录

- `Source/`：Unreal C++ 源码
- `Content/`：项目资源与蓝图
- `Tools/`：本地控制服务与启动脚本
- `Docs/`：协议与联调说明

## 启动

1. 启动 `Tools/StartMockControlServer.bat`
2. 打开 `Designer_Base.uproject`
3. 运行项目后通过网页端进行在线控制
