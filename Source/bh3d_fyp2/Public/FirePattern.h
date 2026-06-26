#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BulletMotionParams.h"
#include "BeamDefinition.h"
#include "FirePattern.generated.h"

/**
 * UFirePattern
 *
 * A DataAsset that describes a single volley — the complete geometric
 * arrangement of bullets produced by one call to SpawnVolley().
 *
 * ── Two layout modes ──────────────────────────────────────────────────────
 *
 * PARAMETRIC (default, CustomDirections is empty):
 *   Beams are distributed across a sphere using two axes:
 *
 *   AZIMUTH   — rotation around SpinAxis. BeamCount and AngleBetweenBeams
 *               produce a ring of beams radiating outward.
 *
 *   ELEVATION — tilt above/below the equatorial plane. ElevationSteps stacks
 *               multiple azimuth rings vertically. AngleBetweenElevations sets
 *               the gap. DefaultElevationAngle offsets the whole stack.
 *
 *   Total beams = BeamCount × ElevationSteps.
 *
 * CUSTOM (CustomDirections is non-empty):
 *   The parametric layout is skipped entirely. Each entry in CustomDirections
 *   is used as a beam direction directly (world-space unit vectors). Use this
 *   for arbitrary geometry — e.g. the 60 vertices of a truncated icosahedron
 *   from UBulletSpreadLibrary::GetTruncatedIcosahedronDirections().
 *
 *   Total beams = CustomDirections.Num().
 *
 * ── BeamOverrides indexing ────────────────────────────────────────────────
 *
 * In parametric mode:
 *   index = elevationStep * BeamCount + azimuthStep
 *
 * In custom mode:
 *   index = same index as CustomDirections entry
 *
 * You only need entries for beams you wish to customise.
 *
 * ── Text Import (CustomDirections only) ──────────────────────────────────
 *
 * ImportText format (one direction per line, X Y Z):
 *
 *   1 0 0
 *   0 1 0
 *   0 0 1
 *   -1 0 0
 *
 * Lines beginning with # are ignored. Empty lines are ignored.
 * Values are normalized at parse time, so actual magnitudes don't matter.
 *
 * Click Rebuild to parse ImportText → CustomDirections.
 *
 * ── Common setups ─────────────────────────────────────────────────────────
 *
 *   Flat 8-way ring:
 *     BeamCount=8, AngleBetweenBeams=45, ElevationSteps=1
 *
 *   Spiral:
 *     BeamCount=1, SpinRate=120, ElevationSteps=1
 *
 *   Spherical burst (3 rings):
 *     BeamCount=8, AngleBetweenBeams=45, ElevationSteps=3,
 *     AngleBetweenElevations=30, DefaultElevationAngle=0
 *     → rings at -30°, 0°, +30°, 8 beams each = 24 total
 *
 *   Truncated icosahedron (60 beams):
 *     CustomDirections = UBulletSpreadLibrary::GetTruncatedIcosahedronDirections()
 */
UCLASS(BlueprintType)
class BH3D_FYP2_API UFirePattern : public UDataAsset
{
    GENERATED_BODY()

public:

    // ── Custom geometry (overrides parametric layout when non-empty) ──────

    /**
     * Arbitrary beam directions in world space (unit vectors).
     * When this array is non-empty the entire parametric layout
     * (BeamCount, AngleBetweenBeams, ElevationSteps, etc.) is ignored.
     *
     * Populate this from UBulletSpreadLibrary (e.g. GetTruncatedIcosahedronDirections,
     * GetFibonacciSphereDirections) or author directions by hand.
     *
     * SpinRate and OffsetAngle still apply — the whole set of directions
     * is rotated around SpinAxis at runtime.
     *
     * To import from text: paste into ImportText, then call Rebuild().
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|CustomGeometry")
    TArray<FVector> CustomDirections;

    /**
     * Plain text import format for CustomDirections.
     * Format: X Y Z (one direction per line, values are normalized)
     *
     * Example:
     *   1 0 0
     *   0 1 0
     *   0 0 1
     *   -1 0 0
     *
     * Lines beginning with # are comments. Empty lines are ignored.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|Import",
              meta=(MultiLine="true", EditCondition="true"))
    FString ImportText;

    // ── Azimuth (parametric ring of beams) ────────────────────────────────

    /**
     * Number of beams distributed around the azimuth ring, per elevation step.
     * Ignored when CustomDirections is non-empty.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|Azimuth",
              meta=(ClampMin="1"))
    int32 BeamCount = 1;

    /**
     * Angle between consecutive beams around the azimuth ring, in degrees.
     * For a full even ring: 360 / BeamCount.
     * Ignored when CustomDirections is non-empty.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|Azimuth",
              meta=(ClampMin="0", ClampMax="360"))
    float AngleBetweenBeams = 45.f;

    /**
     * Static azimuth offset applied before SpinRate, in degrees.
     * Useful for interleaving two patterns from different spawners.
     * Ignored when CustomDirections is non-empty.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|Azimuth")
    float OffsetAngle = 0.f;

    // ── Elevation (parametric vertical distribution) ──────────────────────

    /**
     * Number of elevation rings stacked vertically.
     * Set to 1 for a flat pattern (classic shmup behaviour).
     * Ignored when CustomDirections is non-empty.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|Elevation",
              meta=(ClampMin="1"))
    int32 ElevationSteps = 1;

    /**
     * Angle between consecutive elevation rings, in degrees.
     * Rings are centred on DefaultElevationAngle.
     * Has no effect when ElevationSteps == 1.
     * Ignored when CustomDirections is non-empty.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|Elevation",
              meta=(ClampMin="0", ClampMax="180",
                    EditCondition="ElevationSteps > 1"))
    float AngleBetweenElevations = 30.f;

    /**
     * Elevation of the centre of the ring stack, in degrees.
     *   0  = equatorial (horizontal)
     *   90 = straight up
     *  -90 = straight down
     * Ignored when CustomDirections is non-empty.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|Elevation",
              meta=(ClampMin="-90", ClampMax="90"))
    float DefaultElevationAngle = 0.f;

    // ── Spin ──────────────────────────────────────────────────────────────

    /**
     * Degrees per second the whole pattern rotates around SpinAxis.
     * Accumulated by the spawner over time, producing spiral effects.
     * Applies to both parametric and custom layout modes.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|Spin")
    float SpinRate = 0.f;

    /**
     * Axis around which SpinRate and azimuth layout rotate.
     * Default = world Up (Z), giving classic top-down Touhou spin.
     * Change to spawner forward for a 3D pinwheel.
     * Normalised at runtime.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|Spin")
    FVector SpinAxis = FVector::UpVector;

    // ── Bullet defaults ───────────────────────────────────────────────────

    /**
     * Bullet class used by all beams that do not specify a BulletClassOverride.
     * Must be a subclass of ACPP_Bullet.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|Bullets")
    TSubclassOf<AActor> DefaultBulletClass;

    /** Motion parameters applied to every bullet unless a beam overrides them. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|Bullets")
    FBulletMotionParams DefaultMotionParams;

    // ── Per-beam overrides ────────────────────────────────────────────────

    /**
     * Optional per-beam customisation. Index matches beam order:
     *   Parametric: index = elevationStep * BeamCount + azimuthStep
     *   Custom:     index = CustomDirections index
     *
     * Beams with no entry use DefaultBulletClass, DefaultMotionParams,
     * and single bullet on beam axis (Rim, BulletsPerBeam=1, ConeHalfAngle=0).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern|Beams")
    TArray<FBeamDefinition> BeamOverrides;

    // ── Text import interface ─────────────────────────────────────────────

    /**
     * Parse ImportText and populate CustomDirections.
     * Call this after pasting text into ImportText.
     * Clears CustomDirections before parsing.
     */
    UFUNCTION(CallInEditor, Category="Import")
    void Rebuild();

    /**
     * Clear all custom directions.
     */
    UFUNCTION(CallInEditor, Category="Import")
    void Clear();
};
