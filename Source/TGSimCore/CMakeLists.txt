cmake_minimum_required(VERSION 3.20)

project(TGSimCore LANGUAGES CXX)

set(TGSIM_EIGEN_INCLUDE_DIR "" CACHE PATH "Directory containing the Eigen headers")
set(TGSIM_BOOST_INCLUDE_DIR "" CACHE PATH "Directory containing the Boost headers")
set(TGSIM_NANOFLANN_INCLUDE_DIR "" CACHE PATH "Directory containing nanoflann/nanoflann.hpp")

if(NOT TGSIM_EIGEN_INCLUDE_DIR)
    find_path(TGSIM_EIGEN_INCLUDE_DIR Eigen/LU)
endif()
if(NOT TGSIM_BOOST_INCLUDE_DIR)
    find_path(TGSIM_BOOST_INCLUDE_DIR boost/numeric/odeint.hpp)
endif()
if(NOT TGSIM_NANOFLANN_INCLUDE_DIR)
    find_path(TGSIM_NANOFLANN_INCLUDE_DIR nanoflann/nanoflann.hpp)
endif()
if(NOT TGSIM_EIGEN_INCLUDE_DIR OR NOT TGSIM_BOOST_INCLUDE_DIR OR
   NOT TGSIM_NANOFLANN_INCLUDE_DIR)
    message(FATAL_ERROR
        "TGSimCore needs Eigen, Boost, and nanoflann headers. Set their TGSIM_* "
        "include directories when building outside Unreal Engine.")
endif()

add_library(TGSimCore STATIC
    src/Core/ModelTypes.cpp
    src/Core/Types.cpp
    src/Dynamics/ArticulationConstraintDynamics.cpp
    src/Dynamics/General6DofDynamics.cpp
    src/Dynamics/FloatingBaseTreeDynamics.cpp
    src/Dynamics/SpatialAlgebra.cpp
    src/Environment/EnvironmentModels.cpp
    src/Forces/AerodynamicsModel.cpp
    src/Forces/AerodynamicCoefficientDatabase.cpp
    src/Forces/GravityModel.cpp
    src/Forces/PropulsionModel.cpp
    src/Forces/SolarRadiationPressureModel.cpp
    src/Integrators/FixedStepRK4.cpp
    src/Integrators/AdaptiveDormandPrince54.cpp
    src/Integrators/IntegratorUtilities.cpp
    src/Simulation/SimulationConfigBuilder.cpp
    src/Simulation/SimulationEngine.cpp
    src/Simulation/ScenarioFactory.cpp
    src/Simulation/StateRecorder.cpp
    src/Scenario/CelestialCatalog.cpp
    src/Scenario/ScenarioCompiler.cpp
    src/Scenario/ScenarioDiagnostics.cpp
    src/Scenario/ScenarioFile.cpp
    src/Validation/BackendValidationSuite.cpp
    src/Vehicle/ComponentKinematics.cpp
    src/Vehicle/MassProperties.cpp
)

target_include_directories(TGSimCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${TGSIM_EIGEN_INCLUDE_DIR}
        ${TGSIM_BOOST_INCLUDE_DIR}
        ${TGSIM_NANOFLANN_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/tomlplusplus
)

target_compile_features(TGSimCore PUBLIC cxx_std_17)

option(TGSIM_BUILD_VALIDATION "Build the standalone backend validation executable" ON)
if(TGSIM_BUILD_VALIDATION)
    add_executable(TGSimCoreValidation tests/TGSimCoreValidationMain.cpp)
    target_compile_definitions(TGSimCoreValidation PRIVATE TGSIM_STANDALONE_VALIDATION=1)
    target_link_libraries(TGSimCoreValidation PRIVATE TGSimCore)
endif()

option(TGSIM_BUILD_SCENARIO_TESTS "Build focused .tgscn tests" ON)
if(TGSIM_BUILD_SCENARIO_TESTS)
    add_executable(TGScenarioRoundTripTest tests/TGScenarioRoundTripMain.cpp)
    target_compile_definitions(
        TGScenarioRoundTripTest PRIVATE TGSIM_STANDALONE_SCENARIO_TEST=1)
    target_link_libraries(TGScenarioRoundTripTest PRIVATE TGSimCore)
endif()

option(TGSIM_BUILD_SCENARIO_RUNNER "Build the .tgscn command-line runner" ON)
set(TGSIM_CSPICE_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/CSPICE/Include"
    CACHE PATH "Directory containing SpiceUsr.h")
set(TGSIM_CSPICE_LIBRARY
    "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/CSPICE/Lib/Win64/cspice.lib"
    CACHE FILEPATH "CSPICE library")
if(TGSIM_BUILD_SCENARIO_RUNNER)
    add_executable(PHAROSScenarioRunner
        ../../Tools/TGScenarioRunner/StandaloneControllerAdapter.cpp
        ../../Tools/TGScenarioRunner/StandaloneSpiceProvider.cpp
        ../../Tools/TGScenarioRunner/TGScenarioRunnerMain.cpp
    )
    target_include_directories(PHAROSScenarioRunner PRIVATE
        ../../Tools/TGScenarioRunner
        ../../ControllerSDK
        ${TGSIM_CSPICE_INCLUDE_DIR}
    )
    target_link_libraries(PHAROSScenarioRunner PRIVATE
        TGSimCore
        ${TGSIM_CSPICE_LIBRARY}
    )
    if(MSVC)
        target_link_libraries(PHAROSScenarioRunner PRIVATE
            oldnames
            legacy_stdio_definitions
        )
    endif()
endif()
