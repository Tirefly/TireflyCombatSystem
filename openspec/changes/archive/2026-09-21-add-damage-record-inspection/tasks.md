## 1. Implementation

- [ ] 1.1 `Private/TcsDamageSubsystem.cpp`：加 `DumpRecentRecords(int32 Count)` 助手（格式化表格：序号/Flow/源/目标/Base/Final/差额/Executed/Absorbed/暴击/击杀/时刻）+ `Tcs.Damage.DumpRecords` 命令注册（`FAutoConsoleCommandWithWorldAndArgs`——需要 World 取门面）
- [ ] 1.2 参数钳制：`N` 默认 10、钳到 `[1, 128]`；空缓冲打"尚无记录"提示
- [ ] 1.3 输出：`UE_LOG(LogTcsDamage, Display, ...)` + `GEngine->AddOnScreenDebugMessage`（固定行号，重跑覆盖）

## 2. Verification

- [ ] 2.1 UBT Development Editor 编译通过（零警告）
- [ ] 2.2 用户 PIE：先跑 `Tcs.Test.Damage.Primitive` 产生记录 → 敲 `Tcs.Damage.DumpRecords` 看表格（重点：Base / Final / 差额三列）
- [ ] 2.3 文档回写：台账 R8-6 的"R3 临时替代"指向本命令（已写）；README 检查点状态补一行
