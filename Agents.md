# Demo_AS 项目说明

本项目用于学习研究 Unreal Engine 中的 Angelscript 脚本编程。

## 引擎

- 引擎目录：`d:\WorkGit\UnrealEngine`（UE 5.8.3 源码构建版）
- `Demo_AS.uproject` 中的 `EngineAssociation` 为 GUID 关联，若失效可在注册表 `HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds` 重建，或右键 uproject 重新切换引擎版本。
- 引擎带 Angelscript 支持（含 `AngelscriptGAS` 插件）。

## Script 目录（Angelscript 代码）

项目脚本根目录：`d:\WorkGit\UEProjs\Demo_AS\Script`

- `IntroductionActor.as` — 入门示例 Actor 脚本
- `Script-Examples/` — 官方脚本示例集，供学习参考
