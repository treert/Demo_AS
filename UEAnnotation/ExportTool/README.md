# ExportTool 依赖说明

本目录的 `GenerateUEAnnotationForLua.py` 依赖第三方库 **PyYAML**，新电脑（或新拉取的机器）
没有装这个库时脚本会直接报错退出。首次使用请按下面的步骤先装依赖。

脚本本身不做任何依赖检测/自动安装，依赖问题统一在本文件中说明。

## 一、环境要求

- Python 3.7 及以上（脚本使用了 `from __future__ import annotations` 与 `list[...]` 泛型写法）
- PyYAML（见 `requirements.txt`）

注意：请确认你用的是**哪一个 Python**。如果是在编辑器 / UE 内置终端里运行，
默认解释器可能是 UE 自带的 Python，而不是你自己装的系统 Python。
装依赖时要对**同一个解释器**安装，否则仍然会提示找不到 `yaml`。

查看当前解释器路径：

```bash
python -c "import sys; print(sys.executable)"
```

## 二、安装依赖（首次使用）

在仓库根目录（即 `UEAnnotation` 目录）执行，任选其一：

```bash
# 方式 1：按依赖清单安装（推荐）
python -m pip install -r ExportTool/requirements.txt

# 方式 2：直接安装
python -m pip install pyyaml
```

安装完成后验证一下：

```bash
python -c "import yaml; print(yaml.__version__)"
```

能打印出版本号（如 `6.0.3`）即安装成功。

## 三、运行

```bash
python ExportTool/GenerateUEAnnotationForLua.py
```

脚本通过 `__file__` 定位仓库根目录，所以在哪个目录下执行都可以。

运行前请确认 `Misc/ue-class-info.yaml` 已存在：该文件是 UE 侧导出的中间产物，
需先执行 UE 对应的导出步骤生成，本脚本只负责把它转换成 Lua 注释文件。
