#include "FirePattern.h"
#include "AssetImportLibrary.h"

void UFirePattern::Rebuild()
{
    FString Error;
    if (!UAssetImportLibrary::ParseDirectionsText(ImportText, CustomDirections, Error))
    {
        UE_LOG(LogTemp, Warning, TEXT("FirePattern Rebuild failed: %s"), *Error);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("FirePattern Rebuild: %d directions parsed"), CustomDirections.Num());

#if WITH_EDITOR
    MarkPackageDirty();
#endif
}

void UFirePattern::Clear()
{
    CustomDirections.Empty();
    ImportText.Empty();

#if WITH_EDITOR
    MarkPackageDirty();
#endif
}
