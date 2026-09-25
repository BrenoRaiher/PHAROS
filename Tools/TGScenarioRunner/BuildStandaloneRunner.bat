@echo off
setlocal

for %%I in ("%~dp0..\..") do set "PROJECT_ROOT=%%~fI"
if not defined UE_ENGINE_ROOT set "UE_ENGINE_ROOT=C:\Program Files\Epic Games\UE_5.7\Engine"
if not defined VCVARS (
    for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS=%%I\VC\Auxiliary\Build\vcvars64.bat"
)

if not defined VCToolsInstallDir (
    if not exist "%VCVARS%" (
        echo Could not find vcvars64.bat. Run this script from an x64 Native Tools prompt,
        echo or set VCVARS to its full path.
        exit /b 1
    )
    call "%VCVARS%" >nul
    if errorlevel 1 exit /b 1
)

set "BUILD_DIR=%PROJECT_ROOT%\Tools\TGScenarioRunner\bin"
set "OBJ_DIR=%BUILD_DIR%\obj"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

pushd "%PROJECT_ROOT%"

rc /nologo /fo "%OBJ_DIR%\PHAROSScenarioRunner.res" Tools\TGScenarioRunner\PHAROSScenarioRunner.rc
if errorlevel 1 (
    popd
    exit /b 1
)

cl /nologo /std:c++17 /EHsc /O2 /MT /W4 /experimental:deterministic ^
 /pathmap:"%PROJECT_ROOT%=PHAROS" ^
 /wd4100 /wd4127 /wd4244 /wd4267 /wd4324 /wd4702 ^
 /I"Source\TGSimCore\include" ^
 /I"Source\ThirdParty\tomlplusplus" ^
 /I"Source\ThirdParty\CSPICE\Include" ^
 /I"ControllerSDK" ^
 /I"%UE_ENGINE_ROOT%\Source\ThirdParty\Eigen" ^
 /I"%UE_ENGINE_ROOT%\Source\ThirdParty\Boost\Deploy\boost-1.85.0\include" ^
 /I"%UE_ENGINE_ROOT%\Source\ThirdParty\nanoflann\1.4.2\include" ^
 /Fo"%OBJ_DIR%\\" ^
 /Fe"%BUILD_DIR%\PHAROSScenarioRunner.exe" ^
 Source\TGSimCore\src\Core\ModelTypes.cpp ^
 Source\TGSimCore\src\Core\Types.cpp ^
 Source\TGSimCore\src\Dynamics\ArticulationConstraintDynamics.cpp ^
 Source\TGSimCore\src\Dynamics\General6DofDynamics.cpp ^
 Source\TGSimCore\src\Dynamics\FloatingBaseTreeDynamics.cpp ^
 Source\TGSimCore\src\Dynamics\SpatialAlgebra.cpp ^
 Source\TGSimCore\src\Environment\EnvironmentModels.cpp ^
 Source\TGSimCore\src\Forces\AerodynamicsModel.cpp ^
 Source\TGSimCore\src\Forces\AerodynamicCoefficientDatabase.cpp ^
 Source\TGSimCore\src\Forces\GravityModel.cpp ^
 Source\TGSimCore\src\Forces\PropulsionModel.cpp ^
 Source\TGSimCore\src\Forces\SolarRadiationPressureModel.cpp ^
 Source\TGSimCore\src\Integrators\FixedStepRK4.cpp ^
 Source\TGSimCore\src\Integrators\AdaptiveDormandPrince54.cpp ^
 Source\TGSimCore\src\Integrators\IntegratorUtilities.cpp ^
 Source\TGSimCore\src\Simulation\SimulationConfigBuilder.cpp ^
 Source\TGSimCore\src\Simulation\SimulationEngine.cpp ^
 Source\TGSimCore\src\Simulation\ScenarioFactory.cpp ^
 Source\TGSimCore\src\Simulation\StateRecorder.cpp ^
 Source\TGSimCore\src\Scenario\CelestialCatalog.cpp ^
 Source\TGSimCore\src\Scenario\ScenarioCompiler.cpp ^
 Source\TGSimCore\src\Scenario\ScenarioDiagnostics.cpp ^
 Source\TGSimCore\src\Scenario\ScenarioFile.cpp ^
 Source\TGSimCore\src\Validation\BackendValidationSuite.cpp ^
 Source\TGSimCore\src\Vehicle\ComponentKinematics.cpp ^
 Source\TGSimCore\src\Vehicle\MassProperties.cpp ^
 Tools\TGScenarioRunner\StandaloneControllerAdapter.cpp ^
 Tools\TGScenarioRunner\StandaloneSpiceProvider.cpp ^
 Tools\TGScenarioRunner\TGScenarioRunnerMain.cpp ^
 /link /LIBPATH:"Source\ThirdParty\CSPICE\Lib\Win64" ^
 "%OBJ_DIR%\PHAROSScenarioRunner.res" cspice.lib oldnames.lib legacy_stdio_definitions.lib

set "RESULT=%ERRORLEVEL%"
popd
if not "%RESULT%"=="0" exit /b %RESULT%

echo Built %BUILD_DIR%\PHAROSScenarioRunner.exe

set "PROJECT_BIN=%PROJECT_ROOT%\Binaries\Win64"
if not exist "%PROJECT_BIN%" mkdir "%PROJECT_BIN%"
copy /Y "%BUILD_DIR%\PHAROSScenarioRunner.exe" "%PROJECT_BIN%\PHAROSScenarioRunner.exe" >nul
if errorlevel 1 exit /b 1

echo Staged %PROJECT_BIN%\PHAROSScenarioRunner.exe
exit /b 0
