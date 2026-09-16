## 1. Implementation（补录——已于 2026-09-11 实现并编译通过）

- [x] 1.1 `Public/FTcsValueConvention.h`：`ETcsValueConventionFlag`（VCF_None=0 / VCF_Percent / VCF_OneMinus / VCF_Negate，UENUM Bitflags + UseEnumValuesAsMaskValuesInEditor）+ 静态无状态 `FTcsValueConvention::ConvertToCanonical(RawValue, ConventionFlags)`（header-only，FORCEINLINE）

## 2. Verification

- [x] 2.1 Development Editor 编译通过（2026-09-11）
- [x] 2.2 人工核对：固定组合顺序实现与 D5-18 语义一致（Percent ÷100 → OneMinus 1−v → Negate 取负；VCF_None 恒等）
