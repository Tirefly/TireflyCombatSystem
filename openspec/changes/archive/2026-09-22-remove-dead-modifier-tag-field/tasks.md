## 1. 实现

- [x] 1.1 `FTcsAttrModInstance` 删 `FName Tag` 字段（`TcsAttrModInstance.h`，留删除说明注释）
- [x] 1.2 `FTcsAttrModDefTableRow` 删 `FName Tag` 字段（`TcsAttrModDef.h`，留删除说明注释）
- [x] 1.3 全库自检：确认零读写（`.Tag` / `->Tag` 搜索无命中，排除 `DefTag`/`TemplateTag`/`GetTagName`）

## 2. 验证（禁 TDD 纪律：编译 + 定向人工检查）

- [x] 2.1 UBT Development 编译零警告零错误
- [x] 2.2 UBT Shipping 编译通过
- [ ] 2.3 编辑器内人工检查：模板资产细节面板不再出现"同来源内分组标签"控件 —— **未执行**（需开编辑器；`UPROPERTY` 删除后控件必然消失，无中间态）

## 3. 规格与文档

- [x] 3.1 `openspec validate remove-dead-modifier-tag-field --strict`
- [x] 3.2 提案归档（delta 并入 `attribute-types`）
- [x] 3.3 `02-module-attributes.md` §2.2 权威形状行 + §2.2a 实例形状行同步
- [x] 3.4 README 决策日志追加条目

## 4. 未覆盖（如实记）

- **2.3 未跑**：改动是删除 `UPROPERTY` 字段，编辑器控件消失是编译期确定的（无运行期中间态）；但"没亲眼看过"这一点如实标注。
- **兼容性**：已存在的修正器模板资产若曾填过 `Tag`，反序列化时该列被忽略（引擎对已移除 `UPROPERTY` 的常规行为）——R3 内容资产（`Content/TcsDev/`）中**无任何修正器模板资产**（4 属性 + 2 链 + 1 地图），故实际零影响。
