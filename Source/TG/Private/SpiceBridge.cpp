// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "SpiceBridge.h"

#include "Misc/Paths.h"
#include "SpiceUsr.h"

namespace
{
    bool bKernelsLoaded = false;
    bool bErrorHandlingConfigured = false;

    bool IsSystemBarycenter(const FString& BodyName)
    {
        SpiceInt BodyCode = 0;
        SpiceBoolean bFound = SPICEFALSE;
        // BODS2C accepts both canonical names and numeric strings such as
        // "2000001", which are used by the asteroid catalog entries.
        bods2c_c(TCHAR_TO_ANSI(*BodyName), &BodyCode, &bFound);
        if (failed_c())
        {
            reset_c();
            return false;
        }
        return bFound && BodyCode >= 1 && BodyCode <= 9;
    }

    void ConfigureSpiceErrorHandling()
    {
        if (bErrorHandlingConfigured) return;
        SpiceChar ReturnAction[] = "RETURN";
        erract_c("SET", static_cast<SpiceInt>(sizeof(ReturnAction)), ReturnAction);
        // CheckSpiceSucceeded converts CSPICE failures into Unreal-facing messages.
        // Disable CSPICE's duplicate direct stderr printer, including expected
        // optional metadata misses.
        SpiceChar NoDirectOutput[] = "NONE";
        errprt_c(
            "SET", static_cast<SpiceInt>(sizeof(NoDirectOutput)),
            NoDirectOutput);
        bErrorHandlingConfigured = true;
    }

    bool CheckSpiceSucceeded(const TCHAR* Operation, FString& OutMessage)
    {
        if (!failed_c()) return true;

        SpiceChar ErrorMessage[1841] = {};
        getmsg_c("LONG", static_cast<SpiceInt>(sizeof(ErrorMessage)), ErrorMessage);
        OutMessage = FString::Printf(
            TEXT("CSPICE %s failed: %s"),
            Operation,
            UTF8_TO_TCHAR(ErrorMessage));
        reset_c();
        return false;
    }

    bool ResolveBodyFixedFrameName(
        const FString& BodyName,
        FString& OutFrameName,
        FString& OutMessage)
    {
        SpiceInt BodyCode = 0;
        SpiceBoolean BodyFound = SPICEFALSE;
        bods2c_c(
            TCHAR_TO_ANSI(*BodyName),
            &BodyCode,
            &BodyFound);
        if (!CheckSpiceSucceeded(
                TEXT("body-code lookup"),
                OutMessage)
            || !BodyFound)
        {
            OutFrameName.Reset();
            OutMessage = FString::Printf(
                TEXT("CSPICE has no body code for %s."),
                *BodyName);
            return false;
        }

        SpiceInt FrameCode = 0;
        SpiceChar FrameName[128] = {};
        SpiceBoolean FrameFound = SPICEFALSE;
        cidfrm_c(
            BodyCode,
            static_cast<SpiceInt>(sizeof(FrameName)),
            &FrameCode,
            FrameName,
            &FrameFound);
        if (!CheckSpiceSucceeded(
                TEXT("body-fixed frame lookup"),
                OutMessage)
            || !FrameFound)
        {
            OutFrameName.Reset();
            OutMessage = FString::Printf(
                TEXT("CSPICE has no body-fixed frame for %s (NAIF ID %d)."),
                *BodyName,
                static_cast<int32>(BodyCode));
            return false;
        }

        OutFrameName = UTF8_TO_TCHAR(FrameName);
        return true;
    }

    FString GetKernelPath(const FString& FileName)
    {
        return FPaths::ConvertRelativePathToFull(
            FPaths::ProjectContentDir() / TEXT("SPICEKernels") / FileName
        );
    }

    bool CheckFileExists(const FString& Path, FString& OutMessage)
    {
        if (!FPaths::FileExists(Path))
        {
            OutMessage = FString::Printf(TEXT("Missing kernel file: %s"), *Path);
            return false;
        }

        return true;
    }
}

bool FSpiceBridge::LoadKernels(FString& OutMessage)
{
    ConfigureSpiceErrorHandling();
    if (bKernelsLoaded)
    {
        OutMessage = TEXT("SPICE kernels already loaded.");
        return true;
    }

    // Text kernels define time, physical constants, body frames, and asteroid
    // names. Later-loaded SPKs have precedence, so DE442 deliberately comes
    // last and remains authoritative for every overlapping barycentric state.
    static const TCHAR* KernelFiles[] =
    {
        TEXT("naif0012.tls"),
        TEXT("pck00011.tpc"),
        TEXT("gm_de440.tpc"),
        TEXT("codes_300ast_20100725.tf"),
        TEXT("mar099s.bsp"),
        TEXT("jup230-short.bsp"),
        TEXT("jup348.bsp"),
        TEXT("sat252s.bsp"),
        TEXT("ura111.bsp"),
        TEXT("nep076.bsp"),
        TEXT("plu058.bsp"),
        TEXT("codes_300ast_20100725.bsp"),
        TEXT("de442.bsp")
    };

    for (const TCHAR* KernelFile : KernelFiles)
    {
        const FString KernelPath = GetKernelPath(KernelFile);
        if (!CheckFileExists(KernelPath, OutMessage)) return false;

        furnsh_c(TCHAR_TO_ANSI(*KernelPath));
        const FString Operation =
            FString::Printf(TEXT("loading kernel %s"), KernelFile);
        if (!CheckSpiceSucceeded(*Operation, OutMessage)) return false;
    }

    bKernelsLoaded = true;
    OutMessage = FString::Printf(
        TEXT("%d SPICE kernels loaded successfully."),
        static_cast<int>(UE_ARRAY_COUNT(KernelFiles)));
    return true;
}

bool FSpiceBridge::ConvertUTCToET(const FString& UTCString, double& OutET, FString& OutMessage)
{
    if (!LoadKernels(OutMessage))
    {
        OutET = 0.0;
        return false;
    }

    SpiceDouble ET = 0.0;
    str2et_c(TCHAR_TO_ANSI(*UTCString), &ET);

    if (!CheckSpiceSucceeded(TEXT("UTC-to-ET conversion"), OutMessage))
    {
        OutET = 0.0;
        return false;
    }

    OutET = ET;
    OutMessage = FString::Printf(TEXT("UTC converted to ET: %.15f"), OutET);
    return true;
}

bool FSpiceBridge::ConvertETToUTC(double ET, FString& OutUTCString, FString& OutMessage)
{
    if (!LoadKernels(OutMessage))
    {
        OutUTCString.Reset();
        return false;
    }

    SpiceChar UTCBuffer[64] = {};
    et2utc_c(ET, "ISOC", 3, static_cast<SpiceInt>(sizeof(UTCBuffer)), UTCBuffer);
    if (!CheckSpiceSucceeded(TEXT("ET-to-UTC conversion"), OutMessage))
    {
        OutUTCString.Reset();
        return false;
    }

    OutUTCString = UTF8_TO_TCHAR(UTCBuffer);
    OutMessage = TEXT("ET converted to UTC successfully.");
    return true;
}

bool FSpiceBridge::GetBodyICRFStateSI(
    const FString& BodyName,
    double ET,
    FVector& OutPositionM,
    FVector& OutVelocityMps,
    FString& OutMessage)
{
    if (!LoadKernels(OutMessage))
    {
        OutPositionM = FVector::ZeroVector;
        OutVelocityMps = FVector::ZeroVector;
        return false;
    }

    SpiceDouble State[6] = {};
    SpiceDouble LightTime = 0.0;
    // NAIF spkezr_c: geometric target state relative to the solar-system
    // barycenter in J2000. ET is TDB seconds past J2000; output is km, km/s.
    spkezr_c(
        TCHAR_TO_ANSI(*BodyName),
        ET,
        "J2000",
        "NONE",
        "SOLAR SYSTEM BARYCENTER",
        State,
        &LightTime);
    if (!CheckSpiceSucceeded(TEXT("state lookup"), OutMessage))
    {
        OutPositionM = FVector::ZeroVector;
        OutVelocityMps = FVector::ZeroVector;
        return false;
    }

    constexpr double KmToM = 1.0e3;
    OutPositionM = FVector(State[0] * KmToM, State[1] * KmToM, State[2] * KmToM);
    OutVelocityMps = FVector(State[3] * KmToM, State[4] * KmToM, State[5] * KmToM);
    OutMessage = FString::Printf(TEXT("%s ICRF state obtained successfully."), *BodyName);
    return true;
}

bool FSpiceBridge::GetBodyFixedToICRF(
    const FString& BodyName,
    double ET,
    tgsim::Mat3d& OutBodyFixedToICRF,
    FString& OutMessage)
{
    if (!LoadKernels(OutMessage))
    {
        OutBodyFixedToICRF = tgsim::Mat3d::Identity();
        return false;
    }

    FString FrameName;
    if (!ResolveBodyFixedFrameName(
            BodyName,
            FrameName,
            OutMessage))
    {
        OutBodyFixedToICRF = tgsim::Mat3d::Identity();
        return false;
    }

    SpiceDouble Rotation[3][3] = {};
    pxform_c(TCHAR_TO_ANSI(*FrameName), "J2000", ET, Rotation);
    if (!CheckSpiceSucceeded(TEXT("body-fixed-to-J2000 transform"), OutMessage))
    {
        OutBodyFixedToICRF = tgsim::Mat3d::Identity();
        return false;
    }

    for (int Row = 0; Row < 3; ++Row)
        for (int Column = 0; Column < 3; ++Column)
            OutBodyFixedToICRF.m[Row][Column] = Rotation[Row][Column];

    OutMessage = FString::Printf(
        TEXT("%s body-fixed orientation obtained successfully."), *BodyName);
    return true;
}

bool FSpiceBridge::GetBodyAngularVelocityICRF(
    const FString& BodyName,
    double ET,
    tgsim::Vec3d& OutAngularVelocityIcrfRadps,
    FString& OutMessage)
{
    if (!LoadKernels(OutMessage))
    {
        OutAngularVelocityIcrfRadps = {};
        return false;
    }

    FString FrameName;
    if (!ResolveBodyFixedFrameName(
            BodyName,
            FrameName,
            OutMessage))
    {
        OutAngularVelocityIcrfRadps = {};
        return false;
    }

    // SXFORM from J2000 to the body frame contains both orientation and its
    // derivative. XF2RAV then returns omega_BF/ICRF expressed in J2000, exactly
    // the vector needed by v_atm = v_body + omega x r_rel.
    SpiceDouble StateTransform[6][6] = {};
    SpiceDouble Rotation[3][3] = {};
    SpiceDouble AngularVelocity[3] = {};
    sxform_c(
        "J2000",
        TCHAR_TO_ANSI(*FrameName),
        ET,
        StateTransform);
    if (!CheckSpiceSucceeded(TEXT("J2000-to-body state transform"), OutMessage))
    {
        OutAngularVelocityIcrfRadps = {};
        return false;
    }
    xf2rav_c(StateTransform, Rotation, AngularVelocity);
    OutAngularVelocityIcrfRadps = {
        AngularVelocity[0], AngularVelocity[1], AngularVelocity[2]};
    OutMessage = FString::Printf(
        TEXT("%s angular velocity obtained successfully."), *BodyName);
    return true;
}

bool FSpiceBridge::GetBodyGravityMetadataSI(
    const FString& BodyName,
    double& OutGravitationalParameterM3ps2,
    double& OutReferenceRadiusM,
    FString& OutMessage)
{
    if (!LoadKernels(OutMessage))
    {
        OutGravitationalParameterM3ps2 = 0.0;
        OutReferenceRadiusM = 0.0;
        return false;
    }

    SpiceInt Dimension = 0;
    SpiceDouble GmKm3ps2[1] = {};
    bodvrd_c(
        TCHAR_TO_ANSI(*BodyName),
        "GM",
        1,
        &Dimension,
        GmKm3ps2);
    if (!CheckSpiceSucceeded(TEXT("body GM lookup"), OutMessage) ||
        Dimension < 1)
    {
        OutGravitationalParameterM3ps2 = 0.0;
        OutReferenceRadiusM = 0.0;
        return false;
    }

    SpiceDouble RadiiKm[3] = {};
    bool bHasReferenceRadius = false;
    if (!IsSystemBarycenter(BodyName))
    {
        bodvrd_c(
            TCHAR_TO_ANSI(*BodyName),
            "RADII",
            3,
            &Dimension,
            RadiiKm);
        bHasReferenceRadius = !failed_c() && Dimension >= 1;
        if (failed_c())
        {
            // A physical catalog target may still lack an optional PCK radius.
            // Retain its valid GM and represent the missing surface by radius zero.
            reset_c();
        }
    }

    // SPICE returns GM in km^3/s^2 and shape radii in km. A zero radius means
    // that this target has no atmosphere or apparent occultation disk. Harmonic
    // models use their own uploaded GM and reference radius instead.
    constexpr double CubicKilometersToCubicMeters = 1.0e9;
    constexpr double KilometersToMeters = 1.0e3;
    OutGravitationalParameterM3ps2 =
        GmKm3ps2[0] * CubicKilometersToCubicMeters;
    OutReferenceRadiusM =
        bHasReferenceRadius ? RadiiKm[0] * KilometersToMeters : 0.0;
    OutMessage = bHasReferenceRadius
        ? FString::Printf(
            TEXT("%s GM and reference radius obtained successfully."),
            *BodyName)
        : FString::Printf(
            TEXT("%s GM obtained; this target has no physical radius."),
            *BodyName);
    return true;
}

bool FSpiceBridge::GetBodyICRFPositionCm(
    const FString& BodyName,
    double ET,
    FVector& OutPositionCm,
    FString& OutMessage
)
{
    FVector PositionM;
    FVector VelocityMps;
    if (!GetBodyICRFStateSI(BodyName, ET, PositionM, VelocityMps, OutMessage))
    {
        OutPositionCm = FVector::ZeroVector;
        return false;
    }
    OutPositionCm = PositionM * 100.0;
    return true;
}

bool FSpiceBridge::GetBodyRelativeLocationUECm(
    const FString& BodyName,
    double ET,
    const FVector& ObserverICRFCm,
    double MaxVisibleDistanceCm,
    FVector& OutRelativeLocationUECm,
    bool& bShouldBeVisible,
    FString& OutMessage
)
{
    FVector BodyICRFCm;

    if (!GetBodyICRFPositionCm(BodyName, ET, BodyICRFCm, OutMessage))
    {
        OutRelativeLocationUECm = FVector::ZeroVector;
        bShouldBeVisible = false;
        return false;
    }

    const FVector RelativeICRFCm = BodyICRFCm - ObserverICRFCm;

    OutRelativeLocationUECm = FVector(
        RelativeICRFCm.X,
        -RelativeICRFCm.Y,
        RelativeICRFCm.Z
    );

    bShouldBeVisible = OutRelativeLocationUECm.Length() <= MaxVisibleDistanceCm;

    OutMessage = FString::Printf(
        TEXT("%s relative UE location computed. Visible = %s"),
        *BodyName,
        bShouldBeVisible ? TEXT("true") : TEXT("false")
    );

    return true;
}
