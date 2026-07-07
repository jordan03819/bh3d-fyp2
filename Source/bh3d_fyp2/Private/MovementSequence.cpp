#include "MovementSequence.h"
#include "AssetImportLibrary.h"

void UMovementSequence::Rebuild()
{
    FString Error;
    if (!UAssetImportLibrary::ParseMovementText(ImportText, MovementPoints, Error))
    {
        UE_LOG(LogTemp, Warning, TEXT("MovementSequence Rebuild failed: %s"), *Error);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("MovementSequence Rebuild: %d points parsed"), MovementPoints.Num());

#if WITH_EDITOR
    MarkPackageDirty();
#endif
}

void UMovementSequence::Clear()
{
    MovementPoints.Empty();
    ImportText.Empty();

#if WITH_EDITOR
    MarkPackageDirty();
#endif
}
