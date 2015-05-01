// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodySetup2D.h"
#include "PaperTileMap.h"
#include "PaperTileLayer.h"
#include "PaperRuntimeSettings.h"

#define LOCTEXT_NAMESPACE "Paper2D"

//////////////////////////////////////////////////////////////////////////
// UPaperTileMap

UPaperTileMap::UPaperTileMap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MapWidth = 4;
	MapHeight = 4;
	TileWidth = 32;
	TileHeight = 32;
	PixelsPerUnrealUnit = 1.0f;
	SeparationPerTileX = 0.0f;
	SeparationPerTileY = 0.0f;
	SeparationPerLayer = 4.0f;
	CollisionThickness = 50.0f;
	SpriteCollisionDomain = ESpriteCollisionMode::None;

#if WITH_EDITORONLY_DATA
	SelectedLayerIndex = INDEX_NONE;
	BackgroundColor = FColor(55, 55, 55);
#endif

	LayerNameIndex = 0;

	static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterial(TEXT("/Paper2D/DefaultSpriteMaterial"));
	Material = DefaultMaterial.Object;
}

#if WITH_EDITOR

#include "PaperTileMapComponent.h"
#include "ComponentReregisterContext.h"

/** Removes all components that use the specified sprite asset from their scenes for the lifetime of the class. */
class FTileMapReregisterContext
{
public:
	/** Initialization constructor. */
	FTileMapReregisterContext(UPaperTileMap* TargetAsset)
	{
		// Look at tile map components
		for (TObjectIterator<UPaperTileMapComponent> TileMapIt; TileMapIt; ++TileMapIt)
		{
			if (UPaperTileMapComponent* TestComponent = *TileMapIt)
			{
				if (TestComponent->TileMap == TargetAsset)
				{
					AddComponentToRefresh(TestComponent);
				}
			}
		}
	}

protected:
	void AddComponentToRefresh(UActorComponent* Component)
	{
		if (ComponentContexts.Num() == 0)
		{
			// wait until resources are released
			FlushRenderingCommands();
		}

		new (ComponentContexts) FComponentReregisterContext(Component);
	}

private:
	/** The recreate contexts for the individual components. */
	TIndirectArray<FComponentReregisterContext> ComponentContexts;
};

void UPaperTileMap::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	//@TODO: Determine when these are really needed, as they're seriously expensive!
	FTileMapReregisterContext ReregisterTileMapComponents(this);

	ValidateSelectedLayerIndex();

	if (PixelsPerUnrealUnit <= 0.0f)
	{
		PixelsPerUnrealUnit = 1.0f;
	}

	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UPaperTileMap, MapWidth)) || (PropertyName == GET_MEMBER_NAME_CHECKED(UPaperTileMap, MapHeight)))
	{
		ResizeMap(MapWidth, MapHeight, /*bForceResize=*/ true);
	}

	if (!IsTemplate())
	{
		UpdateBodySetup();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UPaperTileMap::CanEditChange(const UProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange(InProperty);
	if (bIsEditable && InProperty)
	{
		const FName PropertyName = InProperty->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPaperTileMap, HexSideLength))
		{
			bIsEditable = ProjectionMode == ETileMapProjectionMode::HexagonalStaggered;
		}
	}

	return bIsEditable;
}

void UPaperTileMap::PostLoad()
{
	Super::PostLoad();
	ValidateSelectedLayerIndex();
}

void UPaperTileMap::ValidateSelectedLayerIndex()
{
	if (!TileLayers.IsValidIndex(SelectedLayerIndex))
	{
		// Select the top-most visible layer
		SelectedLayerIndex = INDEX_NONE;
		for (int32 LayerIndex = 0; (LayerIndex < TileLayers.Num()) && (SelectedLayerIndex == INDEX_NONE); ++LayerIndex)
		{
			if (!TileLayers[LayerIndex]->bHiddenInEditor)
			{
				SelectedLayerIndex = LayerIndex;
			}
		}

		if ((SelectedLayerIndex == INDEX_NONE) && (TileLayers.Num() > 0))
		{
			SelectedLayerIndex = 0;
		}
	}
}

#endif

#if WITH_EDITORONLY_DATA
void UPaperTileMap::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->SourceFilePath, FAssetRegistryTag::TT_Hidden) );
	}

	Super::GetAssetRegistryTags(OutTags);
}
#endif

void UPaperTileMap::UpdateBodySetup()
{
	// Ensure we have the data structure for the desired collision method
	switch (SpriteCollisionDomain)
	{
	case ESpriteCollisionMode::Use3DPhysics:
		BodySetup = NewObject<UBodySetup>(this);
		break;
	case ESpriteCollisionMode::Use2DPhysics:
		BodySetup = NewObject<UBodySetup2D>(this);
		break;
	case ESpriteCollisionMode::None:
		BodySetup = nullptr;
		break;
	}

	if (SpriteCollisionDomain != ESpriteCollisionMode::None)
	{
		BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;

		for (int32 LayerIndex = 0; LayerIndex < TileLayers.Num(); ++LayerIndex)
		{
			TileLayers[LayerIndex]->AugmentBodySetup(BodySetup);
		}

		// Finalize the BodySetup
#if WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR
		BodySetup->InvalidatePhysicsData();
#endif
		BodySetup->CreatePhysicsMeshes();
	}
}

void UPaperTileMap::GetTileToLocalParameters(FVector& OutCornerPosition, FVector& OutStepX, FVector& OutStepY, FVector& OutOffsetYFactor) const
{
	const float UnrealUnitsPerPixel = GetUnrealUnitsPerPixel();
	const float TileWidthInUU = TileWidth * UnrealUnitsPerPixel;
	const float TileHeightInUU = TileHeight * UnrealUnitsPerPixel;

	switch (ProjectionMode)
	{
	case ETileMapProjectionMode::Orthogonal:
	default:
		OutCornerPosition = -(TileWidthInUU * PaperAxisX * 0.5f) + (TileHeightInUU * PaperAxisY * 0.5f);
		OutOffsetYFactor = FVector::ZeroVector;
		OutStepX = PaperAxisX * TileWidthInUU;
		OutStepY = -PaperAxisY * TileHeightInUU;
		break;
	case ETileMapProjectionMode::IsometricDiamond:
		OutCornerPosition = (TileHeightInUU * PaperAxisY * 0.5f);
		OutOffsetYFactor = FVector::ZeroVector;
		OutStepX = (TileWidthInUU * PaperAxisX * 0.5f) - (TileHeightInUU * PaperAxisY * 0.5f);
		OutStepY = (TileWidthInUU * PaperAxisX * -0.5f) - (TileHeightInUU * PaperAxisY * 0.5f);
		break;
	case ETileMapProjectionMode::HexagonalStaggered:
	case ETileMapProjectionMode::IsometricStaggered:
		OutCornerPosition = -(TileWidthInUU * PaperAxisX * 0.5f) + (TileHeightInUU * PaperAxisY * 0.5f);
		OutOffsetYFactor = 0.5f * TileWidthInUU * PaperAxisX;
		OutStepX = PaperAxisX * TileWidthInUU;
		OutStepY = 0.5f * -PaperAxisY * TileHeightInUU;
		break;
	}
}

void UPaperTileMap::GetLocalToTileParameters(FVector& OutCornerPosition, FVector& OutStepX, FVector& OutStepY, FVector& OutOffsetYFactor) const
{
	const float UnrealUnitsPerPixel = GetUnrealUnitsPerPixel();
	const float TileWidthInUU = TileWidth * UnrealUnitsPerPixel;
	const float TileHeightInUU = TileHeight * UnrealUnitsPerPixel;

	switch (ProjectionMode)
	{
	case ETileMapProjectionMode::Orthogonal:
	default:
		OutCornerPosition = -(TileWidthInUU * PaperAxisX * 0.5f) + (TileHeightInUU * PaperAxisY * 0.5f);
		OutOffsetYFactor = FVector::ZeroVector;
		OutStepX = PaperAxisX / TileWidthInUU;
		OutStepY = -PaperAxisY / TileHeightInUU;
		break;
	case ETileMapProjectionMode::IsometricDiamond:
		OutCornerPosition = (TileHeightInUU * PaperAxisY * 0.5f);
		OutOffsetYFactor = FVector::ZeroVector;
		OutStepX = (PaperAxisX / TileWidthInUU) - (PaperAxisY / TileHeightInUU);
		OutStepY = (-PaperAxisX / TileWidthInUU) - (PaperAxisY / TileHeightInUU);
		break;
	case ETileMapProjectionMode::HexagonalStaggered:
	case ETileMapProjectionMode::IsometricStaggered:
		OutCornerPosition = -(TileWidthInUU * PaperAxisX * 0.5f) + (TileHeightInUU * PaperAxisY * 0.5f);
		OutOffsetYFactor = 0.5f * TileWidthInUU * PaperAxisX;
		OutStepX = PaperAxisX / TileWidthInUU;
		OutStepY = -PaperAxisY / TileHeightInUU;
		break;
	}
}

void UPaperTileMap::GetTileCoordinatesFromLocalSpacePosition(const FVector& Position, int32& OutTileX, int32& OutTileY) const
{
	FVector CornerPosition;
	FVector OffsetYFactor;
	FVector ParameterAxisX;
	FVector ParameterAxisY;

	// position is in pixels
	// axis is tiles to pixels

	GetLocalToTileParameters(/*out*/ CornerPosition, /*out*/ ParameterAxisX, /*out*/ ParameterAxisY, /*out*/ OffsetYFactor);

	const FVector RelativePosition = Position - CornerPosition;
	const float ProjectionSpaceXInTiles = FVector::DotProduct(RelativePosition, ParameterAxisX);
	const float ProjectionSpaceYInTiles = FVector::DotProduct(RelativePosition, ParameterAxisY);

	float X2 = ProjectionSpaceXInTiles;
	float Y2 = ProjectionSpaceYInTiles;

	if ((ProjectionMode == ETileMapProjectionMode::IsometricStaggered) || (ProjectionMode == ETileMapProjectionMode::HexagonalStaggered))
	{
		const float px = FMath::Frac(ProjectionSpaceXInTiles);
		const float py = FMath::Frac(ProjectionSpaceYInTiles);

		// Determine if the point is inside of the diamond or outside
		const float h = 0.5f;
		const float Det1 = -((px - h)*h - py*h);
		const float Det2 = -(px - 1.0f)*h - (py - h)*h;
		const float Det3 = -(-(px - h)*h + (py - 1.0f)*h);
		const float Det4 = px*h + (py - h)*h;

		const bool bOutsideTile = (Det1 < 0.0f) || (Det2 < 0.0f) || (Det3 < 0.0f) || (Det4 < 0.0f);

		if (bOutsideTile)
		{
			X2 = ProjectionSpaceXInTiles - ((px < 0.5f) ? 1.0f : 0.0f);
			Y2 = FMath::FloorToFloat(ProjectionSpaceYInTiles)*2.0f + py + ((py < 0.5f) ? -1.0f : 1.0f);
		}
 		else
 		{
 			X2 = ProjectionSpaceXInTiles;
			Y2 = FMath::FloorToFloat(ProjectionSpaceYInTiles)*2.0f + py;
 		}
	}

	OutTileX = FMath::FloorToInt(X2);
	OutTileY = FMath::FloorToInt(Y2);
}

FVector UPaperTileMap::GetTilePositionInLocalSpace(float TileX, float TileY, int32 LayerIndex) const
{
	FVector CornerPosition;
	FVector OffsetYFactor;
	FVector StepX;
	FVector StepY;

	GetTileToLocalParameters(/*out*/ CornerPosition, /*out*/ StepX, /*out*/ StepY, /*out*/ OffsetYFactor);

	FVector TotalOffset;
	switch (ProjectionMode)
	{
	case ETileMapProjectionMode::Orthogonal:
	default:
		TotalOffset = CornerPosition;
		break;
	case ETileMapProjectionMode::IsometricDiamond:
		TotalOffset = CornerPosition;
		break;
	case ETileMapProjectionMode::HexagonalStaggered:
	case ETileMapProjectionMode::IsometricStaggered:
		TotalOffset = CornerPosition + ((int32)TileY & 1) * OffsetYFactor;
		break;
	}

	const FVector PartialX = TileX * StepX;
	const FVector PartialY = TileY * StepY;

	const float TotalSeparation = (SeparationPerLayer * LayerIndex) + (SeparationPerTileX * TileX) + (SeparationPerTileY * TileY);
	const FVector PartialZ = TotalSeparation * PaperAxisZ;

	const FVector LocalPos(PartialX + PartialY + PartialZ + TotalOffset);

	return LocalPos;
}

void UPaperTileMap::GetTilePolygon(int32 TileX, int32 TileY, int32 LayerIndex, TArray<FVector>& LocalSpacePoints) const
{
	switch (ProjectionMode)
	{
	case ETileMapProjectionMode::Orthogonal:
	case ETileMapProjectionMode::IsometricDiamond:
	default:
		LocalSpacePoints.Add(GetTilePositionInLocalSpace(TileX, TileY, LayerIndex));
		LocalSpacePoints.Add(GetTilePositionInLocalSpace(TileX + 1, TileY, LayerIndex));
		LocalSpacePoints.Add(GetTilePositionInLocalSpace(TileX + 1, TileY + 1, LayerIndex));
		LocalSpacePoints.Add(GetTilePositionInLocalSpace(TileX, TileY + 1, LayerIndex));
		break;

	case ETileMapProjectionMode::IsometricStaggered:
		{
			const float UnrealUnitsPerPixel = GetUnrealUnitsPerPixel();
			const float TileWidthInUU = TileWidth * UnrealUnitsPerPixel;
			const float TileHeightInUU = TileHeight * UnrealUnitsPerPixel;

			const FVector RecenterOffset = PaperAxisX*TileWidthInUU*0.5f;
			const FVector LSTM = GetTilePositionInLocalSpace(TileX, TileY, LayerIndex) + RecenterOffset;

			LocalSpacePoints.Add(LSTM);
			LocalSpacePoints.Add(LSTM + PaperAxisX*TileWidthInUU*0.5f - PaperAxisY*TileHeightInUU*0.5f);
			LocalSpacePoints.Add(LSTM - PaperAxisY*TileHeightInUU*1.0f);
			LocalSpacePoints.Add(LSTM - PaperAxisX*TileWidthInUU*0.5f - PaperAxisY*TileHeightInUU*0.5f);
		}
		break;

	case ETileMapProjectionMode::HexagonalStaggered:
		{
			const float UnrealUnitsPerPixel = GetUnrealUnitsPerPixel();
			const float TileWidthInUU = TileWidth * UnrealUnitsPerPixel;
			const float TileHeightInUU = TileHeight * UnrealUnitsPerPixel;

			const FVector HalfWidth = PaperAxisX*TileWidthInUU*0.5f;
			const FVector LSTM = GetTilePositionInLocalSpace(TileX, TileY, LayerIndex) + HalfWidth;

			const float HexSideLengthInUU = HexSideLength * UnrealUnitsPerPixel;
			const float HalfHexLength = HexSideLengthInUU*0.5f;
			const FVector Top(LSTM - PaperAxisY*HalfHexLength);

			const FVector StepTopSides = PaperAxisY*(TileHeightInUU*0.5f - HalfHexLength);
			const FVector RightTop(LSTM + HalfWidth - StepTopSides);
			const FVector LeftTop(LSTM - HalfWidth - StepTopSides);

			const FVector StepBottomSides = PaperAxisY*(TileHeightInUU*0.5f + HalfHexLength);
			const FVector RightBottom(LSTM + HalfWidth - StepBottomSides);
			const FVector LeftBottom(LSTM - HalfWidth - StepBottomSides);

			const FVector Bottom(LSTM - PaperAxisY*(TileHeightInUU - HalfHexLength));

			LocalSpacePoints.Add(Top);
			LocalSpacePoints.Add(RightTop);
			LocalSpacePoints.Add(RightBottom);
			LocalSpacePoints.Add(Bottom);
			LocalSpacePoints.Add(LeftBottom);
			LocalSpacePoints.Add(LeftTop);
		}
		break;
	}
}

FVector UPaperTileMap::GetTileCenterInLocalSpace(float TileX, float TileY, int32 LayerIndex) const
{
	switch (ProjectionMode)
	{
	case ETileMapProjectionMode::Orthogonal:
	default:
		return GetTilePositionInLocalSpace(TileX + 0.5f, TileY + 0.5f, LayerIndex);

	case ETileMapProjectionMode::IsometricDiamond:
		return GetTilePositionInLocalSpace(TileX + 0.5f, TileY + 0.5f, LayerIndex);

	case ETileMapProjectionMode::IsometricStaggered:
		{
			const float UnrealUnitsPerPixel = GetUnrealUnitsPerPixel();
			const float TileWidthInUU = TileWidth * UnrealUnitsPerPixel;
			const float TileHeightInUU = TileHeight * UnrealUnitsPerPixel;

			const FVector RecenterOffset = PaperAxisX*TileWidthInUU*0.5f - PaperAxisY*TileHeightInUU*0.5f;
			return GetTilePositionInLocalSpace(TileX, TileY, LayerIndex) + RecenterOffset;
		}

	case ETileMapProjectionMode::HexagonalStaggered:
		{
			const float UnrealUnitsPerPixel = GetUnrealUnitsPerPixel();
			const float TileWidthInUU = TileWidth * UnrealUnitsPerPixel;
			const float TileHeightInUU = TileHeight * UnrealUnitsPerPixel;
			const float HexSideLengthInUU = HexSideLength * UnrealUnitsPerPixel;

			const FVector RecenterOffset = PaperAxisX*TileWidthInUU*0.5f - PaperAxisY*(TileHeightInUU)*0.5f;
			return GetTilePositionInLocalSpace(TileX, TileY, LayerIndex) + RecenterOffset;
		}
	}
}

FBoxSphereBounds UPaperTileMap::GetRenderBounds() const
{
	const float Depth = SeparationPerLayer * (TileLayers.Num() - 1);
	const float HalfThickness = 2.0f;

	const float UnrealUnitsPerPixel = GetUnrealUnitsPerPixel();
	const float TileWidthInUU = TileWidth * UnrealUnitsPerPixel;
	const float TileHeightInUU = TileHeight * UnrealUnitsPerPixel;

	switch (ProjectionMode)
	{
		case ETileMapProjectionMode::Orthogonal:
		default:
		{
			const FVector BottomLeft((-0.5f) * TileWidthInUU, -HalfThickness - Depth, -(MapHeight - 0.5f) * TileHeightInUU);
			const FVector Dimensions(MapWidth * TileWidthInUU, Depth + 2 * HalfThickness, MapHeight * TileHeightInUU);

			const FBox Box(BottomLeft, BottomLeft + Dimensions);
			return FBoxSphereBounds(Box);
		}
		case ETileMapProjectionMode::IsometricDiamond:
		{
			 const FVector BottomLeft((-0.5f) * TileWidthInUU * MapWidth, -HalfThickness - Depth, -MapHeight * TileHeightInUU);
			 const FVector Dimensions(MapWidth * TileWidthInUU, Depth + 2 * HalfThickness, (MapHeight + 1) * TileHeightInUU);

			 const FBox Box(BottomLeft, BottomLeft + Dimensions);
			 return FBoxSphereBounds(Box);
		}
		case ETileMapProjectionMode::HexagonalStaggered:
		case ETileMapProjectionMode::IsometricStaggered:
		{
			const int32 RoundedHalfHeight = (MapHeight + 1) / 2;
			const FVector BottomLeft((-0.5f) * TileWidthInUU, -HalfThickness - Depth, -(RoundedHalfHeight) * TileHeightInUU);
			const FVector Dimensions((MapWidth + 0.5f) * TileWidthInUU, Depth + 2 * HalfThickness, (RoundedHalfHeight + 1.0f) * TileHeightInUU);

			const FBox Box(BottomLeft, BottomLeft + Dimensions);
			return FBoxSphereBounds(Box);
		}
	}
}

UPaperTileLayer* UPaperTileMap::AddNewLayer(int32 InsertionIndex)
{
	// Create the new layer
	UPaperTileLayer* NewLayer = NewObject<UPaperTileLayer>(this);
	NewLayer->SetFlags(RF_Transactional);

	NewLayer->LayerWidth = MapWidth;
	NewLayer->LayerHeight = MapHeight;
	NewLayer->DestructiveAllocateMap(NewLayer->LayerWidth, NewLayer->LayerHeight);
	NewLayer->LayerName = GenerateNewLayerName(this);

	// Insert the new layer
	if (TileLayers.IsValidIndex(InsertionIndex))
	{
		TileLayers.Insert(NewLayer, InsertionIndex);
	}
	else
	{
		TileLayers.Add(NewLayer);
	}

	return NewLayer;
}

FText UPaperTileMap::GenerateNewLayerName(UPaperTileMap* TileMap)
{
	// Create a set of existing names
	TSet<FString> ExistingNames;
	for (UPaperTileLayer* ExistingLayer : TileMap->TileLayers)
	{
		ExistingNames.Add(ExistingLayer->LayerName.ToString());
	}

	// Find a good name
	FText TestLayerName;
	do
	{
		TileMap->LayerNameIndex++;
		TestLayerName = FText::Format(LOCTEXT("NewLayerNameFormatString", "Layer {0}"), FText::AsNumber(TileMap->LayerNameIndex, &FNumberFormattingOptions::DefaultNoGrouping()));
	} while (ExistingNames.Contains(TestLayerName.ToString()));

	return TestLayerName;
}

void UPaperTileMap::ResizeMap(int32 NewWidth, int32 NewHeight, bool bForceResize)
{
	if (bForceResize || (NewWidth != MapWidth) || (NewHeight != MapHeight))
	{
		MapWidth = FMath::Max(NewWidth, 1);
		MapHeight = FMath::Max(NewHeight, 1);

		// Resize all of the existing layers
		for (int32 LayerIndex = 0; LayerIndex < TileLayers.Num(); ++LayerIndex)
		{
			UPaperTileLayer* TileLayer = TileLayers[LayerIndex];
			TileLayer->Modify();
			TileLayer->ResizeMap(MapWidth, MapHeight);
		}
	}
}

void UPaperTileMap::InitializeNewEmptyTileMap()
{
	//@TODO: Consider removing this function / making it a 'prepare' method just like the sprite one on the importer settings
	AddNewLayer();
}

UPaperTileMap* UPaperTileMap::CloneTileMap(UObject* OuterForClone)
{
	return CastChecked<UPaperTileMap>(StaticDuplicateObject(this, OuterForClone, nullptr));
}

bool UPaperTileMap::UsesTileSet(UPaperTileSet* TileSet) const
{
	for (UPaperTileLayer* Layer : TileLayers)
	{
		if (Layer->UsesTileSet(TileSet))
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE