#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ESpreadMode.h"
#include "BulletSpreadLibrary.generated.h"

/**
 * UBulletSpreadLibrary
 *
 * Static functions that generate TArray<FVector> of local spread directions
 * for use in FBeamDefinition.CustomSpreadDirections, or to populate
 * UFirePattern.CustomDirections for arbitrary geometry patterns.
 *
 * ── Local frame convention ────────────────────────────────────────────────
 *
 * All functions that produce spread directions (for bullets within a beam)
 * return vectors in beam-local space:
 *
 *   X = forward (along beam axis)      — (1,0,0) = no deflection
 *   Y = cone right (perpendicular)
 *   Z = cone up   (perpendicular)
 *
 * The spawner transforms them to world space as:
 *   worldDir = BeamDir*local.X + ConeRight*local.Y + ConeUp*local.Z
 *
 * Functions that produce beam directions for CustomDirections
 * (e.g. GetTruncatedIcosahedronDirections) return world-space unit vectors
 * that you place directly in UFirePattern.CustomDirections.
 */
UCLASS()
class BH3D_API UBulletSpreadLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    // ── Preset generators (beam-local space) ─────────────────────────────

    /**
     * Generate directions for the given SpreadMode.
     * This is the main dispatch function — the spawner calls this.
     *
     * @param Mode              Which preset to use.
     * @param BulletsPerBeam    How many bullets (i.e. directions) to produce.
     * @param ConeHalfAngle     Angle in degrees from beam axis to cone rim.
     * @param RollOffset        Rotates the pattern around the beam axis, degrees.
     * @return                  Array of unit vectors in beam-local space.
     */
    UFUNCTION(BlueprintCallable, Category="BulletSystem|Spread")
    static TArray<FVector> GenerateSpread(ESpreadMode Mode, int32 BulletsPerBeam,
                                          float ConeHalfAngle, float RollOffset = 0.f);

    /**
     * Rim preset: bullets evenly spaced around the cone rim at ConeHalfAngle.
     * Hollow centre — all bullets travel at the same angle from the axis.
     *
     * @param Count         Number of bullets on the rim.
     * @param HalfAngleDeg  Angle from beam axis to rim, in degrees.
     * @param RollOffset    Starting roll angle, in degrees.
     */
    UFUNCTION(BlueprintCallable, Category="BulletSystem|Spread")
    static TArray<FVector> GenerateRim(int32 Count, float HalfAngleDeg,
                                       float RollOffset = 0.f);

    /**
     * Filled preset: bullets distributed uniformly inside the cone
     * using a Fibonacci spherical cap spiral.
     *
     * Produces the most visually uniform coverage for scatter/shotgun effects.
     * With Count=1 or HalfAngleDeg=0, returns a single forward direction.
     *
     * @param Count         Number of bullets to distribute.
     * @param HalfAngleDeg  Maximum angle from beam axis, in degrees.
     */
    UFUNCTION(BlueprintCallable, Category="BulletSystem|Spread")
    static TArray<FVector> GenerateFilled(int32 Count, float HalfAngleDeg);

    /**
     * Cross preset: bullets arranged on the cone rim aligned to the cone's
     * local axes (up, right, down, left, and diagonals for counts > 4).
     *
     * Identical to Rim with RollOffset chosen to align the first bullet
     * to the cone's local +Z (up) axis.
     *
     * @param Count         Number of bullets. Multiples of 4 give perfect symmetry.
     * @param HalfAngleDeg  Angle from beam axis, in degrees.
     */
    UFUNCTION(BlueprintCallable, Category="BulletSystem|Spread")
    static TArray<FVector> GenerateCross(int32 Count, float HalfAngleDeg);

    // ── Arbitrary geometry (world/spawner-local space) ────────────────────

    /**
     * Returns the 60 vertex directions of a truncated icosahedron
     * (a soccer ball / Buckminster fullerene) as unit vectors.
     *
     * These are in world space and intended for UFirePattern.CustomDirections.
     * The pattern will fire one beam per vertex — 60 beams total.
     *
     * The shape is rotationally arbitrary at rest. Assign it to a spinning
     * spawner (SpinRate != 0) or rotate the spawner actor for orientation control.
     *
     * Example usage:
     *   UFirePattern* P = ...;
     *   P->CustomDirections = UBulletSpreadLibrary::GetTruncatedIcosahedronDirections();
     */
    UFUNCTION(BlueprintCallable, Category="BulletSystem|Geometry")
    static TArray<FVector> GetTruncatedIcosahedronDirections();

    /**
     * Returns N directions uniformly distributed over the full sphere
     * using a Fibonacci spiral.
     *
     * Useful for omnidirectional bursts or custom spherical patterns.
     *
     * @param Count  Number of directions to generate.
     */
    UFUNCTION(BlueprintCallable, Category="BulletSystem|Geometry")
    static TArray<FVector> GetFibonacciSphereDirections(int32 Count);

private:
    /**
     * Shared helper: place Count points on a cone rim.
     * HalfAngleRad in radians. RollOffsetRad in radians.
     */
    static TArray<FVector> MakeRim(int32 Count, float HalfAngleRad,
                                   float RollOffsetRad);
};
