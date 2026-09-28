# Demo_AS 项目说明

本项目用于学习研究 Unreal Engine 中的 Angelscript 脚本编程。

## 引擎

- 引擎目录：`d:\WorkGit\UnrealEngine`（UE 5.8.3 源码构建版）
- Angelscript 主插件目录：`D:\WorkGit\UnrealEngine\Engine\Plugins\Angelscript`（见 `Angelscript.uplugin`）
- 引擎带 Angelscript 支持（含 `AngelscriptGAS` 插件）。

## Script 目录（Angelscript 代码）

项目脚本根目录：`d:\WorkGit\UEProjs\Demo_AS\Script`

- `IntroductionActor.as` — 入门示例 Actor 脚本
- `Script-Examples/` — 官方脚本示例集，供学习参考
- `Binds.Cache` — C++ → Angelscript 绑定数据库缓存（cook/commandlet 时自动生成，勿手改）

## Plugins/AngelscriptTypeExporter（类型声明导出插件）

项目级 Editor 插件：把 Angelscript 引擎注册的全部 C++ 绑定类型导出为 `.d.as` 声明文件（无函数体），供 my-angel-script-lsp 离线消费（hover/跳转/引用不需要启动引擎）。数据源与引擎侧 `FAngelscriptDebugServer::SendDebugDatabase` 完全一致，但额外过滤脚本类（`asOBJ_SCRIPT_OBJECT`）。

- 详细实现说明：`my_docs/P1-导出插件实现说明.md`（输出格式、与 DebugServer 的逐段对照表）
- 核心源码：`Plugins/AngelscriptTypeExporter/Source/AngelscriptTypeExporter/Private/TypeDeclarationExporter.cpp`
- 依赖引擎插件 `AngelscriptCode`（其 Build.cs 公开暴露 ThirdParty angelscript 头文件）

### 导出方式

```powershell
# 推荐：Commandlet（无头，秒级完成）
& "d:\WorkGit\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "d:\WorkGit\UEProjs\Demo_AS\Demo_AS.uproject" `
    -run=AngelscriptTypeExporter -unattended -nopause -nosplash
```

或编辑器内 Tools 菜单 → "Export Angelscript Type Declarations (.d.as)"。

### 输出

- 目录：`Saved/AS-Cache/`（先删后建；`Saved/` 不入 git，导出物是缓存）
- `_manifest.dctx`：版本清单（`cache_format` / `engine_version` / `float_is_float64` / `files=`），LSP 端据此发现文件并校验版本
- 分组规则：UClass/UScriptStruct/UEnum 按 `UPackage` 命名空间分组（如 `/Script/EnhancedInput`）；namespace 按名分组（空 → `Global`）；无法归组的落入 `Core.d.as`
- **文件名规则**：包分组与 namespace 分组短名冲突时，包分组用全限定名（`/Script/EnhancedInput` → `Script.EnhancedInput.d.as`），namespace 保留短名——两者绝不可互相同名，否则后写会覆盖先写（曾因此丢失整个 EnhancedInput 包的类型声明）
- ScriptMixin 载体类（如 `UEnhancedInputLocalPlayerSubsystemScriptMixinLibrary`）是空壳，不导出；其函数以方法形式绑定在目标类型上

### 修改后验证

```powershell
# 增量编译
& "d:\WorkGit\UnrealEngine\Engine\Build\BatchFiles\Build.bat" Demo_ASEditor Win64 Development -Project="d:\WorkGit\UEProjs\Demo_AS\Demo_AS.uproject" -WaitMutex
# 重新导出后检查：_manifest.dctx 的 files= 无重复项；抽查目标类（如 Script.EnhancedInput.d.as 里的 UEnhancedInputLocalPlayerSubsystem）
```

