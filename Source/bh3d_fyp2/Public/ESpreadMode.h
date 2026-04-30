#pragma once

#include "CoreMinimal.h"
#include "ESpreadMode.generated.h"

/**
 * ESpreadMode
 *
 * Selects how bullets are distributed within a beam's spread cone.
 * All modes produce a TArray<FVector> of local directions at runtime.
 * Custom lets you supply that array directly.
 *
 * Local frame convention (same for all modes and for CustomSpreadDirections):
 *   X = forward (along beam axis). A direction of (1,0,0) = no deflection.
 *   Y = cone right (perpendicular to beam, computed at spawn time).
 *   Z = cone up   (perpendicular to beam and right).
 *
 * The spawner transforms local dirs → world dirs as:
 *   worldDir = BeamDir*local.X + ConeRight*local.Y + ConeUp*local.Z
 */
UENUM(BlueprintType)
enum class ESpreadMode : uint8
{
    /**
     * Bullets placed evenly around the RIM of the cone at exactly ConeHalfAngle
     * from the beam axis. Produces a hollow ring of bullets.
     *
     * BulletsPerBeam controls how many bullets are on the rim.
     * ConeRollOffset rotates the ring around the beam axis.
     *
     *   1 bullet  → single bullet offset ConeHalfAngle from axis
     *   3 bullets → equilateral triangle
     *   8 bullets → octagonal ring
     */
    Rim         UMETA(DisplayName="Rim"),

    /**
     * Bullets distributed across the INTERIOR of the cone up to ConeHalfAngle,
     * using a Fibonacci spherical cap spiral for uniform coverage.
     *
     * BulletsPerBeam controls density. ConeHalfAngle=0 collapses to a
     * single bullet on the axis regardless of BulletsPerBeam.
     *
     * Use for shotgun/scatter patterns where density matters more than
     * precise arrangement.
     */
    Filled      UMETA(DisplayName="Filled"),

    /**
     * Bullets placed on the rim of the cone in a CROSS pattern:
     * up, down, left, right (and diagonals if BulletsPerBeam > 4).
     *
     * Equivalent to Rim with ConeRollOffset chosen so arms align to
     * the cone's local axes. BulletsPerBeam must be a multiple of 4
     * for perfect symmetry.
     */
    Cross       UMETA(DisplayName="Cross"),

    /**
     * User-supplied directions via FBeamDefinition.CustomSpreadDirections.
     * All other spread parameters are ignored.
     * Directions must follow the local frame convention above.
     */
    Custom      UMETA(DisplayName="Custom"),
};
