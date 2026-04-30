#include "CPP_Spawner.h"
#include "CPP_Bullet.h"
#include "BulletSpreadLibrary.h"
#include "Engine/World.h"

ACPP_Spawner::ACPP_Spawner()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ACPP_Spawner::BeginPlay()
{
    Super::BeginPlay();

    if (Sequence)
        StartSequence();
}

// ── Runtime control ───────────────────────────────────────────────────────

void ACPP_Spawner::SetSequence(UAttackSequence* NewSequence)
{
    Sequence = NewSequence;
    StartSequence();
}

void ACPP_Spawner::SetPaused(bool bPause)
{
    bPaused = bPause;
}

// ── Tick ──────────────────────────────────────────────────────────────────

void ACPP_Spawner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bPaused || !Sequence) return;
    if (!Sequence->Phases.IsValidIndex(CurrentPhaseIndex)) return;

    const FAttackPhase& Phase = Sequence->Phases[CurrentPhaseIndex];

    // Accumulate spin even during the start delay so the pattern is already
    // at the correct angle when firing begins.
    if (Phase.Pattern)
        AccumSpinAngle += Phase.Pattern->SpinRate * DeltaTime;

    PhaseTimer += DeltaTime;

    // PhaseTimer starts at -StartDelay and counts up to 0 before firing begins.
    if (PhaseTimer < 0.f) return;

    // While loop catches up if DeltaTime crossed multiple intervals in one
    // frame (e.g. after a hitch or the first frame of a new phase).
    while (PhaseTimer >= Phase.Interval)
    {
        PhaseTimer -= Phase.Interval;

        bool bShotsRemaining = (Phase.ShotCount < 0) ||
                               (ShotsThisPhase < Phase.ShotCount);

        if (bShotsRemaining && Phase.Pattern)
        {
            SpawnVolley(Phase.Pattern);
            ShotsThisPhase++;
        }

        bool bPhaseComplete = (Phase.ShotCount >= 0) &&
                              (ShotsThisPhase >= Phase.ShotCount);

        if (bPhaseComplete)
        {
            if (Phase.bLoop)
            {
                // Keep firing this phase from the top.
                ShotsThisPhase = 0;
            }
            else
            {
                AdvancePhase();
                break; // Re-evaluate with the new phase next tick.
            }
        }
    }
}

// ── Sequence state ────────────────────────────────────────────────────────

void ACPP_Spawner::StartSequence()
{
    CurrentPhaseIndex = 0;
    ShotsThisPhase    = 0;
    AccumSpinAngle    = 0.f;

    if (Sequence && Sequence->Phases.IsValidIndex(0))
    {
        PhaseTimer = -Sequence->Phases[0].StartDelay;
        OnPhaseStarted(0);
    }
    else
    {
        PhaseTimer = 0.f;
    }
}

void ACPP_Spawner::AdvancePhase()
{
    CurrentPhaseIndex++;
    ShotsThisPhase = 0;
    AccumSpinAngle = 0.f;

    if (Sequence->Phases.IsValidIndex(CurrentPhaseIndex))
    {
        const FAttackPhase& Next = Sequence->Phases[CurrentPhaseIndex];
        PhaseTimer = -Next.StartDelay;
        OnPhaseStarted(CurrentPhaseIndex);
    }
    else
    {
        // All phases exhausted — sequence is complete.
        OnSequenceFinished();
    }
}

// ── SpawnVolley ───────────────────────────────────────────────────────────

void ACPP_Spawner::SpawnVolley(UFirePattern* Pattern)
{
    if (!Pattern || !GetWorld()) return;

    const FVector Origin   = GetActorLocation();
    const FVector SpinAxis = Pattern->SpinAxis.GetSafeNormal();

    // Total azimuth/spin rotation applied to the whole pattern this volley.
    // Applies to both parametric and custom layout modes.
    const float TotalAzimuth = Pattern->OffsetAngle + AccumSpinAngle;
    const FQuat PatternRot(SpinAxis, FMath::DegreesToRadians(TotalAzimuth));

    // ── Collect beam directions ───────────────────────────────────────────
    //
    // Two modes:
    //   Custom:     use Pattern->CustomDirections directly (arbitrary geometry)
    //   Parametric: compute directions from azimuth + elevation parameters
    //
    // Both modes produce a flat TArray<FVector> of world-space unit vectors.
    // Everything below this block is identical for both modes.

    TArray<FVector> BeamDirs;

    const bool bCustomLayout = Pattern->CustomDirections.Num() > 0;

    if (bCustomLayout)
    {
        // ── Custom layout ─────────────────────────────────────────────────
        //
        // Rotate each custom direction by the accumulated spin.
        // OffsetAngle and SpinRate still apply — the whole set of directions
        // rotates as a rigid body around SpinAxis.

        BeamDirs.Reserve(Pattern->CustomDirections.Num());

        for (const FVector& Dir : Pattern->CustomDirections)
        {
            BeamDirs.Add(PatternRot.RotateVector(Dir).GetSafeNormal());
        }
    }
    else
    {
        // ── Parametric layout ─────────────────────────────────────────────
        //
        // Beams are arranged in a grid of elevation rings × azimuth steps.
        //
        // AZIMUTH: rotate the spawner's forward vector around SpinAxis by
        //   (azimuthIndex × AngleBetweenBeams), then apply PatternRot on top.
        //
        // ELEVATION: tilt each beam away from the equatorial plane by rotating
        //   around ElevAxis — the axis perpendicular to both SpinAxis and the
        //   current azimuth direction. This preserves azimuth angle exactly.

        const FVector ForwardDir = GetActorForwardVector();

        const int32 ElevSteps  = FMath::Max(1, Pattern->ElevationSteps);
        const float ElevCentre = Pattern->DefaultElevationAngle;
        const float ElevGap    = Pattern->AngleBetweenElevations;
        const float ElevSpan   = (ElevSteps - 1) * ElevGap;

        BeamDirs.Reserve(ElevSteps * Pattern->BeamCount);

        for (int32 ElevIdx = 0; ElevIdx < ElevSteps; ElevIdx++)
        {
            // Elevation of this ring, centred around ElevCentre.
            const float RingElevation = (ElevSteps > 1)
                ? ElevCentre - ElevSpan * 0.5f + ElevIdx * ElevGap
                : ElevCentre;

            for (int32 AzimIdx = 0; AzimIdx < Pattern->BeamCount; AzimIdx++)
            {
                // Step 1: Azimuth — place the beam around the ring.
                const float AzimDeg = AzimIdx * Pattern->AngleBetweenBeams;
                const FQuat BeamAzimRot(SpinAxis, FMath::DegreesToRadians(AzimDeg));
                FVector BeamDir = PatternRot.RotateVector(
                                      BeamAzimRot.RotateVector(ForwardDir)).GetSafeNormal();

                // Step 2: Elevation — tilt the beam up or down.
                // ElevAxis is perpendicular to both SpinAxis and BeamDir, so
                // tilting around it does not disturb the azimuth angle.
                const FVector ElevAxis = FVector::CrossProduct(SpinAxis, BeamDir).GetSafeNormal();

                if (!ElevAxis.IsNearlyZero())
                {
                    // Fetch per-beam elevation offset (if any).
                    const int32 BeamIdx = ElevIdx * Pattern->BeamCount + AzimIdx;
                    const FBeamDefinition* BeamDef =
                        Pattern->BeamOverrides.IsValidIndex(BeamIdx)
                        ? &Pattern->BeamOverrides[BeamIdx]
                        : nullptr;

                    const float TotalElev = RingElevation +
                                            (BeamDef ? BeamDef->ElevationOffset : 0.f);

                    const FQuat ElevRot(ElevAxis, FMath::DegreesToRadians(TotalElev));
                    BeamDir = ElevRot.RotateVector(BeamDir).GetSafeNormal();
                }

                BeamDirs.Add(BeamDir);
            }
        }
    }

    // ── Fire each beam ────────────────────────────────────────────────────
    //
    // For each beam direction we:
    //   1. Build a cone local frame (ConeRight, ConeUp) perpendicular to BeamDir.
    //   2. Resolve spread directions (preset or custom) in beam-local space.
    //   3. Transform each local spread dir to world space.
    //   4. Spawn a bullet, set its rotation, and hand off motion params.

    for (int32 BeamIdx = 0; BeamIdx < BeamDirs.Num(); BeamIdx++)
    {
        const FVector BeamDir = BeamDirs[BeamIdx];

        // Fetch beam override (same index as BeamDirs, works for both modes).
        const FBeamDefinition* BeamDef =
            Pattern->BeamOverrides.IsValidIndex(BeamIdx)
            ? &Pattern->BeamOverrides[BeamIdx]
            : nullptr;

        TSubclassOf<AActor>        BulletClass  = ResolveBulletClass(Pattern, BeamDef);
        const FBulletMotionParams& MotionParams = ResolveMotionParams(Pattern, BeamDef);

        if (!BulletClass) continue;

        // ── Build cone local frame ────────────────────────────────────────
        //
        // ConeRight and ConeUp are two vectors perpendicular to BeamDir that
        // form the cross-section disc of the spread cone.
        //
        // We cross BeamDir with world Up to find ConeRight. If BeamDir is
        // nearly parallel to world Up (i.e. pointing straight up or down),
        // we fall back to world Forward to avoid a zero-length cross product.

        FVector ConeRight = FVector::CrossProduct(BeamDir, FVector::UpVector);
        if (ConeRight.IsNearlyZero())
            ConeRight = FVector::CrossProduct(BeamDir, FVector::ForwardVector);
        ConeRight.Normalize();

        const FVector ConeUp = FVector::CrossProduct(BeamDir, ConeRight).GetSafeNormal();

        // ── Resolve spread directions ─────────────────────────────────────
        //
        // Local convention (defined in ESpreadMode.h and BulletSpreadLibrary.h):
        //   X = forward (along beam axis)
        //   Y = cone right
        //   Z = cone up
        //
        // Custom mode: use CustomSpreadDirections from the beam definition.
        // Preset modes: call GenerateSpread to produce the array at spawn time.

        TArray<FVector> LocalSpreadDirs;

        if (BeamDef && BeamDef->SpreadMode == ESpreadMode::Custom)
        {
            LocalSpreadDirs = BeamDef->CustomSpreadDirections;

            // If no custom directions were provided, fall back to a single
            // forward bullet rather than spawning nothing.
            if (LocalSpreadDirs.Num() == 0)
                LocalSpreadDirs.Add(FVector::ForwardVector);
        }
        else
        {
            const ESpreadMode Mode        = BeamDef ? BeamDef->SpreadMode    : ESpreadMode::Rim;
            const int32       Count       = BeamDef ? BeamDef->BulletsPerBeam : 1;
            const float       HalfAngle   = BeamDef ? BeamDef->ConeHalfAngle : 0.f;
            const float       RollOffset  = BeamDef ? BeamDef->ConeRollOffset : 0.f;

            LocalSpreadDirs = UBulletSpreadLibrary::GenerateSpread(
                Mode, Count, HalfAngle, RollOffset);

            // GenerateSpread returns empty only for Custom mode (handled above),
            // but guard anyway.
            if (LocalSpreadDirs.Num() == 0)
                LocalSpreadDirs.Add(FVector::ForwardVector);
        }

        const float Spacing = BeamDef ? BeamDef->SpacingBetweenBullets : 0.f;

        // ── Spawn one bullet per spread direction ─────────────────────────

        for (int32 BulletIdx = 0; BulletIdx < LocalSpreadDirs.Num(); BulletIdx++)
        {
            const FVector& Local = LocalSpreadDirs[BulletIdx];

            // Transform local spread direction to world space.
            // BeamDir  maps to local X (forward — no deflection).
            // ConeRight maps to local Y.
            // ConeUp   maps to local Z.
            const FVector BulletDir = (BeamDir   * Local.X +
                                       ConeRight * Local.Y +
                                       ConeUp    * Local.Z).GetSafeNormal();

            // Stagger spawn positions along the beam direction (optional).
            const FVector SpawnPos = Origin + BeamDir * (BulletIdx * Spacing);

            AActor* Spawned = GetWorld()->SpawnActor<AActor>(
                BulletClass, SpawnPos, BulletDir.Rotation());

            // Pass motion params to the bullet. Its forward vector is already
            // set correctly by the rotation passed to SpawnActor, so the movement
            // component will build the initial velocity from GetActorForwardVector().
            if (ACPP_Bullet* Bullet = Cast<ACPP_Bullet>(Spawned))
            {
                Bullet->InitializeBullet(MotionParams);
            }
        }
    }
}

// ── Resolution helpers ────────────────────────────────────────────────────

TSubclassOf<AActor> ACPP_Spawner::ResolveBulletClass(
    const UFirePattern* Pattern, const FBeamDefinition* BeamDef) const
{
    // Priority: beam override → pattern default → spawner fallback.
    if (BeamDef && BeamDef->BulletClassOverride)
        return BeamDef->BulletClassOverride;

    if (Pattern && Pattern->DefaultBulletClass)
        return Pattern->DefaultBulletClass;

    return DefaultBulletClass;
}

const FBulletMotionParams& ACPP_Spawner::ResolveMotionParams(
    const UFirePattern* Pattern, const FBeamDefinition* BeamDef) const
{
    // Priority: beam override (only when explicitly enabled) → pattern default.
    if (BeamDef && BeamDef->bOverrideMotionParams)
        return BeamDef->MotionParamsOverride;

    // Pattern is guaranteed non-null at this call site (checked in SpawnVolley).
    return Pattern->DefaultMotionParams;
}
