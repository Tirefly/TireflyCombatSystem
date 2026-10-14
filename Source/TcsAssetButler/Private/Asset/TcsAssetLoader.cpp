// Copyright Tirefly. All Rights Reserved.

#include "Asset/TcsAssetLoader.h"

#include "Engine/AssetManager.h"

#include "TcsAssetButlerLogChannel.h"



void FTcsAssetLoadHandle::Release()
{
	if (!AssetId.IsValid())
	{
		return;
	}

	FTcsAssetLoader::Get().ReleaseRequest(AssetId);
	AssetId = FPrimaryAssetId();
}



bool FTcsAssetLoadHandle::IsValid() const
{
	return AssetId.IsValid();
}



FTcsAssetLoader& FTcsAssetLoader::Get()
{
	// 函数局部静态：加载状态不属于任何世界，进程内唯一
	static FTcsAssetLoader Instance;
	return Instance;
}



FTcsAssetLoadHandle FTcsAssetLoader::LoadAsync(
	const FPrimaryAssetId& AssetId,
	const TArray<FName>& BundleNames,
	FStreamableDelegate OnComplete)
{
	if (!AssetId.IsValid())
	{
		UE_LOG(LogTcsAssetButler, Warning, TEXT("FTcsAssetLoader::LoadAsync: 主资产身份无效——拒绝加载"));
		return FTcsAssetLoadHandle();
	}

	AcquireHandle(AssetId, BundleNames);

	// 完成回调挂在本**次**请求上：引擎对已加载完成者亦在下一帧回调（其文档明示），
	// 故调用方不必自行判"是不是已经在内存里"
	if (OnComplete.IsBound())
	{
		if (const FLoadRecord* Found = Loads.Find(AssetId))
		{
			if (Found->Handle.IsValid())
			{
				Found->Handle->BindCompleteDelegate(OnComplete);
			}
		}
	}

	FTcsAssetLoadHandle Handle;
	Handle.AssetId = AssetId;
	return Handle;
}



UObject* FTcsAssetLoader::LoadSync(const FPrimaryAssetId& AssetId, const TArray<FName>& BundleNames)
{
	if (!AssetId.IsValid())
	{
		UE_LOG(LogTcsAssetButler, Warning, TEXT("FTcsAssetLoader::LoadSync: 主资产身份无效——拒绝加载"));
		return nullptr;
	}

	const TSharedPtr<FStreamableHandle> Handle = AcquireHandle(AssetId, BundleNames);
	if (!Handle.IsValid())
	{
		return nullptr;
	}

	// 逃生口：阻塞当帧
	Handle->WaitUntilComplete();

	return Handle->GetLoadedAsset();
}



int32 FTcsAssetLoader::NumTrackedLoads() const
{
	return Loads.Num();
}



TSharedPtr<FStreamableHandle> FTcsAssetLoader::AcquireHandle(const FPrimaryAssetId& AssetId, const TArray<FName>& BundleNames)
{
	// **防重的唯一落点**：同 id 已在册则复用（计数 +1），不发起第二次加载
	if (FLoadRecord* Existing = Loads.Find(AssetId))
	{
		++Existing->RefCount;
		return Existing->Handle;
	}

	UAssetManager& Manager = UAssetManager::Get();

	FStreamableDelegate Delegate;	// 引擎侧完成回调留空：完成通知由各调用点的 OnComplete 各自绑定
	const TSharedPtr<FStreamableHandle> Handle = Manager.LoadPrimaryAsset(AssetId, BundleNames, MoveTemp(Delegate));

	FLoadRecord& Record = Loads.Add(AssetId);
	Record.Handle = Handle;
	Record.RefCount = 1;

	UE_LOG(LogTcsAssetButler, Log,
		TEXT("FTcsAssetLoader: 发起主资产加载 %s（bundle %d 个）——在册 %d 项"),
		*AssetId.ToString(), BundleNames.Num(), Loads.Num());

	return Handle;
}



void FTcsAssetLoader::ReleaseRequest(const FPrimaryAssetId& AssetId)
{
	FLoadRecord* Record = Loads.Find(AssetId);
	if (!Record)
	{
		return;
	}

	--Record->RefCount;

	if (Record->RefCount > 0)
	{
		return;
	}

	// 计数归零：释放底层句柄（引擎据此决定是否真正卸载）
	Loads.Remove(AssetId);

	UE_LOG(LogTcsAssetButler, Log,
		TEXT("FTcsAssetLoader: 主资产 %s 的请求全部释放——在册 %d 项"),
		*AssetId.ToString(), Loads.Num());
}