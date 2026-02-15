# TCS 代码规范检查清单

在提交代码前，请确保所有项目都已检查：

## 文件头部规范

- [ ] 所有文件以 `// Copyright Tirefly. All Rights Reserved.` 开头
- [ ] Copyright 声明后有 1 个空行
- [ ] 使用 `#pragma once` (头文件)
- [ ] `#pragma once` 后有 1 个空行
- [ ] Include 块后有 3 个空行

## 代码组织规范

- [ ] **使用 `#pragma region-endregion` 组织代码** (NOT `// ========== Section ==========`)
- [ ] region 声明后有 1 个空行
- [ ] endregion 前有 1 个空行
- [ ] 区域之间有 2 个空行

示例：
```cpp
#pragma region RegionName

public:
	// 区域内容

#pragma endregion


#pragma region AnotherRegion

public:
	// 另一个区域内容

#pragma endregion
```

## 声明间距规范

- [ ] 枚举/结构体/类声明之间: 3 个空行
- [ ] 前置声明块和枚举/结构体之间: 3 个空行
- [ ] 委托声明之间: 1 个空行
- [ ] 委托声明块和类声明之间: 3 个空行

## 函数间距规范 (.cpp 文件)

- [ ] 函数实现之间: 0 个空行 (函数直接相邻)
- [ ] 函数体内部: 根据逻辑分组适当使用空行

## 注释规范

- [ ] 类、结构体、枚举声明需要注释
- [ ] 成员变量和成员函数需要注释
- [ ] 函数参数超过2个时，需要为每个参数添加注释
- [ ] 有返回值的函数需要说明返回值含义
- [ ] 委托声明需要注释

## 命名规范

- [ ] 类名: `UTcs` 或 `FTcs` 前缀
- [ ] 枚举: `ETcs` 前缀
- [ ] 接口: `ITcs` 前缀
- [ ] 使用驼峰命名法

## 编译验证

- [ ] 代码编译通过，无错误
- [ ] 代码编译通过，无警告
- [ ] 已测试基本功能

## OpenSpec 规范 (如适用)

- [ ] 已阅读 `openspec/project.md`
- [ ] 已阅读相关的 `proposal.md` 和 `design.md`
- [ ] 已按照 `tasks.md` 完成所有任务
- [ ] 已更新 `tasks.md` 中的完成状态

---

**重要提示**: 这些规范不是建议，而是**强制要求**。不符合规范的代码将被要求修改。
