// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/AssetPtr.h"
#include "PersonaPreviewSceneDescription.generated.h"

class UAnimationAsset;
class UPreviewMeshCollection;
class USkeletalMesh;

UENUM()
enum class EPreviewAnimationMode : uint8
{
	/** Animate the preview based on the current editor (e.g. Skeleton = Reference Pose, Animation = Animation, Animation Blueprint = Animation Blueprint) */
	Default,

	/** Use the reference pose from the skeleton */
	ReferencePose,

	/** Use a specified animation */
	UseSpecificAnimation,
};

UCLASS()
class UPersonaPreviewSceneDescription : public UObject
{
public:
	GENERATED_BODY()

	/** The method by which the preview is animated */
	UPROPERTY(EditAnywhere, Category = "Animation")
	EPreviewAnimationMode AnimationMode;

	/** The preview animation to use */
	UPROPERTY(EditAnywhere, Category = "Animation", meta=(DisplayThumbnail=true))
	TAssetPtr<UAnimationAsset> Animation;

	/** The preview mesh to use */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta=(DisplayThumbnail=true))
	TAssetPtr<USkeletalMesh> PreviewMesh;

	UPROPERTY(EditAnywhere, Category = "Additional Meshes")
	TAssetPtr<UPreviewMeshCollection> AdditionalMeshes;
};
