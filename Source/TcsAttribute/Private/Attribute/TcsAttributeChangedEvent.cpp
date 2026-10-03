// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeChangedEvent.h"



// 属性变更事件 Tag（原生 Tag 注册——随模块加载生效，无需项目 Tag 表配置）
// **`DevComment`**：语义 / 谁声明谁消费（编辑器 tag 树 tooltip 的载体）
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_TcsEvent_Attribute_ValueChanged, "TcsEvent.Attribute.ValueChanged",
	"属性当前值变更结果事件（重算产生实质变化时经总线立即通道派发）；由插件 TcsAttribute 原生声明并广播·宿主与跨模块消费者订阅过滤");
