## 1. 文件迁移
- [x] 1.1 新建 `Public|Private/Attribute/AttrModOperand/` 目录。
- [x] 1.2 将 Operand 侧 5 个 `.h` + 4 个 `.cpp` 移入并重命名。
- [x] 1.3 保留 `AttrModOperation/` 仅含 `TcsAttributeModifierOperation` 与 `TcsAttributeModifierCustomOperator`。

## 2. 引用修正
- [x] 2.1 更新被移动文件的 `.generated.h` 与内部 include。
- [x] 2.2 更新外部引用（TcsAttributeModifierOperation.h/.cpp、AttrModEvaluation、AttrModHelpers、主模块 ManualTest）。

## 3. 验证
- [x] 3.1 grep 确认无残留旧路径引用。
- [x] 3.2 编译验证 TCS 与 Editor target。
- [x] 3.3 `openspec validate --strict --no-interactive` 通过后归档。
