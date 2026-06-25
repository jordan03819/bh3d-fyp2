#include "CPP_Spawner.h"
#include "CPP_Bullet.h"
#include "CPP_BulletManager.h"
#include "BulletSpreadLibrary.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

ACPP_Spawner::ACPP_Spawner()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
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

    const FVector Origin      = GetActorLocation();
    const FVector ForwardDir  = GetActorForwardVector();
    const FVector AzimuthAxis = GetActorUpVector();  // beam distribution axis

    // SpinAxis in world space — only used for the final rigid-body rotation.
    const FVector SpinAxis   = Pattern->SpinAxis.GetSafeNormal();
    const float   TotalSpin  = Pattern->OffsetAngle + AccumSpinAngle;
    const FQuat   PatternRot(SpinAxis, FMath::DegreesToRadians(TotalSpin));

    const bool bCustomLayout = Pattern->CustomDirections.Num() > 0;

    TArray<FVector> BeamDirs;

    if (bCustomLayout)
    {
        BeamDirs.Reserve(Pattern->CustomDirections.Num());
        for (const FVector& Dir : Pattern->CustomDirections)
        {
            // Custom directions are defined in spawner-local space at rest.
            // Apply spin as the final step.
            BeamDirs.Add(PatternRot.RotateVector(Dir).GetSafeNormal());
        }
    }
    else
    {
        const int32 ElevSteps  = FMath::Max(1, Pattern->ElevationSteps);
        const float ElevCentre = Pattern->DefaultElevationAngle;
        const float ElevGap    = Pattern->AngleBetweenElevations;
        const float ElevSpan   = (ElevSteps - 1) * ElevGap;

        BeamDirs.Reserve(ElevSteps * Pattern->BeamCount);

        for (int32 ElevIdx = 0; ElevIdx < ElevSteps; ElevIdx++)
        {
            const float RingElevation = (ElevSteps > 1)
                ? ElevCentre - ElevSpan * 0.5f + ElevIdx * ElevGap
                : ElevCentre;

            for (int32 AzimIdx = 0; AzimIdx < Pattern->BeamCount; AzimIdx++)
            {
                // ── Step 1: azimuth in spawner-local space ────────────────
                // Rotate forward around the spawner's up axis.
                // No spin applied yet — this is the rest pose.
                const float AzimDeg = AzimIdx * Pattern->AngleBetweenBeams;
                const FQuat AzimRot(AzimuthAxis, FMath::DegreesToRadians(AzimDeg));
                FVector RestDir = AzimRot.RotateVector(ForwardDir).GetSafeNormal();

                // ── Step 2: elevation in spawner-local space ──────────────
                // ElevAxis is computed from the REST pose direction, not the
                // spun direction. This keeps it stable across all spin angles.
                const FVector ElevAxis = FVector::CrossProduct(
                    AzimuthAxis, RestDir).GetSafeNormal();

                if (!ElevAxis.IsNearlyZero())
                {
                    const int32 BeamIdx = ElevIdx * Pattern->BeamCount + AzimIdx;
                    const FBeamDefinition* BeamDef =
                        Pattern->BeamOverrides.IsValidIndex(BeamIdx)
                        ? &Pattern->BeamOverrides[BeamIdx]
                        : nullptr;

                    const float TotalElev = RingElevation +
                                            (BeamDef ? BeamDef->ElevationOffset : 0.f);
                    const FQuat ElevRot(ElevAxis, FMath::DegreesToRadians(TotalElev));
                    RestDir = ElevRot.RotateVector(RestDir).GetSafeNormal();
                }

                // ── Step 3: apply spin as a single rigid-body rotation ────
                // PatternRot rotates around SpinAxis in world space.
                // Applied last so it doesn't disturb the rest-pose geometry.
                BeamDirs.Add(PatternRot.RotateVector(RestDir).GetSafeNormal());
            }
        }
    }

    // ── Fire each beam ────────────────────────────────────────────────────
    //
    // Fetch the manager once per volley.  All bullet acquisitions go through
    // it — SpawnActor is never called at runtime after pool warm-up.
    ACPP_BulletManager* Manager = ACPP_BulletManager::Get(this);

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

            if (bDebugDrawOnly)
            {
                // Draw a line showing bullet direction, length represents speed
                DrawDebugLine(
                    GetWorld(),
                    SpawnPos,
                    SpawnPos + BulletDir * 600.f,
                    FColor::Red,
                    false,    // not persistent — redraws each volley
                    0.15f,    // lifetime in seconds — just long enough to see
                    0,
                    10.f      // thickness
                );

                // Draw a point at the spawn position
                DrawDebugPoint(
                    GetWorld(),
                    SpawnPos,
                    8.f,
                    FColor::Yellow,
                    false,
                    0.1f
                );
            }
            else if (Manager)
            {
                // Pull from pool — no SpawnActor call at runtime.
                // FireBullet positions the bullet, sets its rotation, and calls
                // InitializeBullet(MotionParams), so we don't need to do it here.
                //
                // BulletClass is TSubclassOf<AActor>; FireBullet takes
                // TSubclassOf<ACPP_Bullet>.  The explicit construction is safe
                // because ResolveBulletClass only ever returns ACPP_Bullet subclasses.
                Manager->FireBullet(
                    TSubclassOf<ACPP_Bullet>(BulletClass.Get()),
                    SpawnPos,
                    BulletDir,
                    MotionParams
                );
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
