// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TcsAttributeName.generated.h"



/**
 * 属性名（D2-1）：FName 的显式包装——裸 FName/TEXT 传不进属性 API（token 化保证，
 * 属性读写点不接受裸名）。
 *
 * 稠密 int32 id 缓存（D2-1 后半：注册表世代号校验失效）依赖属性词表注册表，属 M8；
 * 本任务不落该字段——零消费者不预建；R3 以 TMap + GetTypeHash(FName) 键控。
 */
USTRUCT(BlueprintType)
struct TCSATTRIBUTE_API FTcsAttributeName
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	// 默认构造（空名——供反射默认值与容器默认值使用）
	FTcsAttributeName() = default;

	// 显式构造（裸 FName 不做隐式转换——D2-1）
	explicit FTcsAttributeName(FName InName)
		: Name(InName)
	{
	}

public:
	// 属性唯一名（与属性词表行名对应）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Name")
	FName Name = NAME_None;

#pragma endregion


// 查询与比较
#pragma region Query

public:
	// 是否为空名（未配置/无效——属性读写点应拒绝空名）
	bool IsNone() const
	{
		return Name.IsNone();
	}

	// 相等比较（属性实例键控/来源配对清理用）
	friend bool operator==(const FTcsAttributeName& A, const FTcsAttributeName& B)
	{
		return A.Name == B.Name;
	}

	// 哈希（TMap 键控用）
	friend uint32 GetTypeHash(const FTcsAttributeName& AttributeName)
	{
		return GetTypeHash(AttributeName.Name);
	}

#pragma endregion
};
