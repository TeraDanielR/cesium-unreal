// Copyright 2020-2023 CesiumGS, Inc. and Contributors

#if WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

#include "CesiumGltfComponent.h"
#include "CesiumRuntime.h"
#include "CesiumSceneGeneration.h"
#include "CesiumTestHelpers.h"
#include "GlobeAwareDefaultPawn.h"

using namespace Cesium;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCesiumLoadTestDenver,
    "Cesium.Performance.LoadTestDenver",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCesiumLoadTestGoogleplex,
    "Cesium.Performance.LoadTestGoogleplex",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCesiumLoadTestMontrealPointCloud,
    "Cesium.Performance.LoadTestMontrealPointCloud",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter)

struct LoadTestContext {
  SceneGenerationContext creationContext;
  SceneGenerationContext playContext;

  bool testStarted;
  double startMark;
  double endMark;

  void reset() {
    creationContext = playContext = SceneGenerationContext();
    testStarted = false;
    startMark = endMark = 0;
  }
};

LoadTestContext gLoadTestContext;

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
    TimeLoadingCommand,
    LoadTestContext&,
    context);
bool TimeLoadingCommand::Update() {

  if (!context.testStarted) {

    // Bind all play in editor pointers
    context.playContext.initForPlay(context.creationContext);

    // Start test mark, turn updates back on
    context.startMark = FPlatformTime::Seconds();
    UE_LOG(LogCesium, Display, TEXT("-- Load start mark --"));

    context.playContext.setSuspendUpdate(false);

    context.testStarted = true;
    // Return, let world tick
    return false;
  }

  double timeMark = FPlatformTime::Seconds();
  double testElapsedTime = timeMark - context.startMark;

  // The command is over if tilesets are loaded, or timed out
  // Wait for a maximum of 20 seconds
  const size_t testTimeout = 20;
  bool tilesetsloaded = context.playContext.areTilesetsDoneLoading();
  bool timedOut = testElapsedTime >= testTimeout;

  if (tilesetsloaded || timedOut) {
    context.endMark = timeMark;
    UE_LOG(LogCesium, Display, TEXT("-- Load end mark --"));

    if (timedOut) {
      UE_LOG(
          LogCesium,
          Error,
          TEXT("TIMED OUT: Loading stopped after %.2f seconds"),
          testElapsedTime);
    } else {
      UE_LOG(
          LogCesium,
          Display,
          TEXT("Tileset load completed in %.2f seconds"),
          testElapsedTime);
    }

    // Turn on the editor tileset updates so we can see what we loaded
    gLoadTestContext.creationContext.setSuspendUpdate(false);

    // Command is done
    return true;
  }

  // Let world tick, we'll come back to this command
  return false;
}

bool RunLoadTest(
    std::function<void(SceneGenerationContext&)> locationSetup,
    std::function<void(SceneGenerationContext&)> afterTest = {}) {

  //
  // Programmatically set up the world
  //
  gLoadTestContext.reset();
  UE_LOG(LogCesium, Display, TEXT("Creating world objects..."));
  createCommonWorldObjects(gLoadTestContext.creationContext);

  // Configure location specific objects
  locationSetup(gLoadTestContext.creationContext);
  gLoadTestContext.creationContext.trackForPlay();

  // Halt tileset updates and reset them
  gLoadTestContext.creationContext.setSuspendUpdate(true);
  gLoadTestContext.creationContext.refreshTilesets();

  //
  // Start async commands
  //

  // Start play in editor (don't sim in editor)
  ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));

  // Wait a bit
  ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));

  // Do our timing capture
  ADD_LATENT_AUTOMATION_COMMAND(TimeLoadingCommand(gLoadTestContext));

  // End play in editor
  ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());

  /*
    if (afterTest) {
      afterTest(context);
    }
  */

  return true;
}

bool FCesiumLoadTestDenver::RunTest(const FString& Parameters) {
  return RunLoadTest(setupForDenver);
}

bool FCesiumLoadTestGoogleplex::RunTest(const FString& Parameters) {
  return RunLoadTest(setupForGoogleTiles);
}

bool FCesiumLoadTestMontrealPointCloud::RunTest(const FString& Parameters) {

  auto after = [this](SceneGenerationContext& context) {
    /*
      // Zoom way out
      FCesiumCamera zoomedOut;
      zoomedOut.ViewportSize = FVector2D(1024, 768);
      zoomedOut.Location = FVector(0, 0, 7240000.0);
      zoomedOut.Rotation = FRotator(-90.0, 0.0, 0.0);
      zoomedOut.FieldOfViewDegrees = 90;
      context.setCamera(zoomedOut);

      context.pawn->SetActorLocation(zoomedOut.Location);

      context.setSuspendUpdate(false);
      tickWorldUntil(context, 10, breakWhenTilesetsLoaded);
      context.setSuspendUpdate(true);

      Cesium3DTilesSelection::Tileset* pTileset =
          context.tilesets[0]->GetTileset();
      if (TestNotNull("Tileset", pTileset)) {
        int visibleTiles = 0;
        pTileset->forEachLoadedTile([&](Cesium3DTilesSelection::Tile& tile) {
          if (tile.getState() != Cesium3DTilesSelection::TileLoadState::Done)
            return;
          const Cesium3DTilesSelection::TileContent& content =
      tile.getContent(); const Cesium3DTilesSelection::TileRenderContent*
      pRenderContent = content.getRenderContent(); if (!pRenderContent) {
            return;
          }

          UCesiumGltfComponent* Gltf = static_cast<UCesiumGltfComponent*>(
              pRenderContent->getRenderResources());
          if (Gltf && Gltf->IsVisible()) {
            ++visibleTiles;
          }
        });

        TestEqual("visibleTiles", visibleTiles, 1);
      }
  */
  };

  return RunLoadTest(setupForMelbourne, after);
}

#endif
