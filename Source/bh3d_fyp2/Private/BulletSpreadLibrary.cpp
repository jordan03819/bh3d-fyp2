#include "BulletSpreadLibrary.h"

// ── Dispatch ──────────────────────────────────────────────────────────────

TArray<FVector> UBulletSpreadLibrary::GenerateSpread(
    ESpreadMode Mode, int32 BulletsPerBeam, float ConeHalfAngle, float RollOffset)
{
    switch (Mode)
    {
        case ESpreadMode::Rim:
            return GenerateRim(BulletsPerBeam, ConeHalfAngle, RollOffset);

        case ESpreadMode::Filled:
            return GenerateFilled(BulletsPerBeam, ConeHalfAngle);

        case ESpreadMode::Cross:
            return GenerateCross(BulletsPerBeam, ConeHalfAngle);

        case ESpreadMode::Custom:
            // Custom dirs are provided directly by the caller — nothing to generate.
            return TArray<FVector>();

        default:
            return TArray<FVector>();
    }
}

// ── Rim ───────────────────────────────────────────────────────────────────

TArray<FVector> UBulletSpreadLibrary::GenerateRim(
    int32 Count, float HalfAngleDeg, float RollOffset)
{
    return MakeRim(Count,
                   FMath::DegreesToRadians(HalfAngleDeg),
                   FMath::DegreesToRadians(RollOffset));
}

// ── Filled ────────────────────────────────────────────────────────────────

TArray<FVector> UBulletSpreadLibrary::GenerateFilled(int32 Count, float HalfAngleDeg)
{
    TArray<FVector> Dirs;
    if (Count <= 0) return Dirs;

    // Degenerate cases: collapse to a single forward bullet.
    if (Count == 1 || FMath::IsNearlyZero(HalfAngleDeg))
    {
        Dirs.Add(FVector::ForwardVector);
        return Dirs;
    }

    // Fibonacci spherical cap spiral.
    //
    // We distribute Count points over a spherical cap of half-angle HalfAngleDeg.
    // cos(theta) is sampled uniformly in [cos(HalfAngle), 1] — this gives uniform
    // area density on the sphere cap (equal solid angle per point).
    //
    // The golden angle (≈ 137.5°) is used as the azimuth step so no two
    // consecutive points share a radial line, producing a visually even spiral.

    const float CosMax    = FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg));
    const float GoldenAng = PI * (3.f - FMath::Sqrt(5.f)); // ≈ 2.3999 rad ≈ 137.5°

    for (int32 i = 0; i < Count; i++)
    {
        // Uniform in [CosMax, 1]: t=0 → centre (no deflection), t=1 → rim.
        const float t        = (Count > 1) ? static_cast<float>(i) / (Count - 1) : 0.f;
        const float CosTheta = FMath::Lerp(1.f, CosMax, t);
        const float SinTheta = FMath::Sqrt(FMath::Max(0.f, 1.f - CosTheta * CosTheta));

        const float Phi = i * GoldenAng;

        // Local space: X = forward, Y = right, Z = up.
        Dirs.Add(FVector(CosTheta,
                         SinTheta * FMath::Cos(Phi),
                         SinTheta * FMath::Sin(Phi)));
    }

    return Dirs;
}

// ── Cross ─────────────────────────────────────────────────────────────────

TArray<FVector> UBulletSpreadLibrary::GenerateCross(int32 Count, float HalfAngleDeg)
{
    // Cross = Rim with the first bullet aligned to local +Z (up).
    // A 90° roll offset achieves this because MakeRim starts the first
    // bullet at roll = 0 which aligns to local +Y (right).
    return MakeRim(Count,
                   FMath::DegreesToRadians(HalfAngleDeg),
                   FMath::DegreesToRadians(90.f));
}

// ── Shared rim helper ─────────────────────────────────────────────────────

TArray<FVector> UBulletSpreadLibrary::MakeRim(
    int32 Count, float HalfAngleRad, float RollOffsetRad)
{
    TArray<FVector> Dirs;
    if (Count <= 0) return Dirs;

    const float CosH = FMath::Cos(HalfAngleRad);
    const float SinH = FMath::Sin(HalfAngleRad);

    // Special case: ConeHalfAngle == 0 → all bullets along beam axis.
    if (FMath::IsNearlyZero(SinH))
    {
        for (int32 i = 0; i < Count; i++)
            Dirs.Add(FVector::ForwardVector);
        return Dirs;
    }

    const float AngleStep = (Count > 1) ? (2.f * PI / Count) : 0.f;

    for (int32 i = 0; i < Count; i++)
    {
        // Phi = angular position of this bullet around the cone rim.
        const float Phi = RollOffsetRad + i * AngleStep;

        // Local space: X = forward (= CosH = no deflection component),
        //              Y = right  (= SinH * cos(phi)),
        //              Z = up     (= SinH * sin(phi)).
        Dirs.Add(FVector(CosH,
                         SinH * FMath::Cos(Phi),
                         SinH * FMath::Sin(Phi)));
    }

    return Dirs;
}

// ── Truncated icosahedron ─────────────────────────────────────────────────

TArray<FVector> UBulletSpreadLibrary::GetTruncatedIcosahedronDirections()
{
    // A truncated icosahedron has 60 vertices. They are all even permutations
    // of the following coordinate families (scaled to the unit sphere):
    //
    //   Group 1: (0, ±1, ±3φ)        — 3 cyclic permutations × 4 signs = 12
    //   Group 2: (±1, ±(2+φ), ±2φ)   — 3 cyclic permutations × 8 signs = 24
    //   Group 3: (±2, ±(1+2φ), ±φ)   — 3 cyclic permutations × 8 signs = 24
    //
    // where φ = (1 + √5) / 2 (golden ratio ≈ 1.618).
    //
    // We use lambdas to iterate over all cyclic permutations and sign combos,
    // then normalise all 60 vectors to the unit sphere.

    TArray<FVector> Dirs;
    Dirs.Reserve(60);

    const float Phi = (1.f + FMath::Sqrt(5.f)) / 2.f;

    // Group 1: one coordinate is zero → 4 sign combinations per cyclic perm.
    // Adds 3 cyclic perms × 4 = 12 vertices.
    auto AddCyclic4Signs = [&](float A, float B, float C)
    {
        for (int32 S1 : {-1, 1})
        for (int32 S2 : {-1, 1})
        {
            Dirs.Add(FVector(A,        S1 * B, S2 * C));  // (A,  ±B, ±C)
            Dirs.Add(FVector(S2 * C,   A,      S1 * B));  // (±C,  A, ±B)
            Dirs.Add(FVector(S1 * B,   S2 * C, A     ));  // (±B, ±C,  A)
        }
    };

    // Groups 2 & 3: all non-zero → 8 sign combinations per cyclic perm.
    // Adds 3 cyclic perms × 8 = 24 vertices each.
    auto AddCyclic8Signs = [&](float A, float B, float C)
    {
        for (int32 S0 : {-1, 1})
        for (int32 S1 : {-1, 1})
        for (int32 S2 : {-1, 1})
        {
            Dirs.Add(FVector(S0 * A, S1 * B, S2 * C));  // (±A, ±B, ±C)
            Dirs.Add(FVector(S2 * C, S0 * A, S1 * B));  // (±C, ±A, ±B)
            Dirs.Add(FVector(S1 * B, S2 * C, S0 * A));  // (±B, ±C, ±A)
        }
    };

    AddCyclic4Signs(0.f, 1.f,           3.f * Phi);        // Group 1  → 12
    AddCyclic8Signs(1.f, 2.f + Phi,     2.f * Phi);        // Group 2  → 24
    AddCyclic8Signs(2.f, 1.f + 2.f*Phi, Phi       );       // Group 3  → 24

    // Normalise to unit sphere.
    for (FVector& V : Dirs)
        V.Normalize();

    // Dirs now contains exactly 60 unit vectors. Verify in debug builds.
    check(Dirs.Num() == 60);

    return Dirs;
}

// ── Fibonacci sphere ──────────────────────────────────────────────────────

TArray<FVector> UBulletSpreadLibrary::GetFibonacciSphereDirections(int32 Count)
{
    TArray<FVector> Dirs;
    if (Count <= 0) return Dirs;

    // Distribute Count points uniformly over the full unit sphere.
    // Uses the Fibonacci / golden angle method.
    //
    // cos(theta) is sampled uniformly in [-1, 1] for full sphere coverage.
    // phi uses the golden angle for the azimuth.

    const float GoldenAng = PI * (3.f - FMath::Sqrt(5.f));

    for (int32 i = 0; i < Count; i++)
    {
        const float CosTheta = 1.f - (2.f * i + 1.f) / Count;
        const float SinTheta = FMath::Sqrt(FMath::Max(0.f, 1.f - CosTheta * CosTheta));
        const float Phi      = i * GoldenAng;

        Dirs.Add(FVector(SinTheta * FMath::Cos(Phi),
                         SinTheta * FMath::Sin(Phi),
                         CosTheta));
    }

    return Dirs;
}
