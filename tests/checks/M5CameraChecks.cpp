/** @file M5LevelFlow.cpp
 * @brief Simulate spawn/death callbacks; this verifies flow, not gameplay AI.
 */
#include "tests/checks/M5LevelFlow.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/map/CMap.h"
#include "gun_bros_re/gameplay/enemy/CEnemy.h"
#include "gun_bros_re/application/CGunBros.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cmath>
#include "tests/checks/M5LevelFlowInternal.h"
using namespace M5LevelFlowDetail;

namespace M5LevelFlowDetail {

unsigned CheckCameraScale() {
    CCamera camera;
    unsigned failures = 0;
    // Synthetic wire fixture for a source-confirmed branch absent from retail maps.
    const std::vector<std::uint8_t> movieBytes = {
        0x82, 0x75, 0x26, 0, 7, 0, 0, 0, 0x85, 0xff, 0xc8, 1
    };
    CArrayInputStream movieInput(movieBytes);
    CLayerMovie movieLayer;
    if (!movieLayer.Init(movieInput) || movieInput.Available() != 0 ||
        movieLayer.GetMovieRef().packHash != 0x267582 || movieLayer.GetMovieRef().assetId != 7 ||
        movieLayer.GetX() != -123 || movieLayer.GetY() != 456) { ++failures; }
    auto truncatedMovie = movieBytes;
    truncatedMovie.pop_back();
    CArrayInputStream truncatedInput(truncatedMovie);
    CLayerMovie incompleteMovie;
    if (incompleteMovie.Init(truncatedInput)) { ++failures; }
    std::printf("[map-movie-check] reference/signed-position/truncation failures=%u\n", failures);
    camera.SnapScale(0.8f);
    camera.SetScale(0.4f);
    if (std::fabs(camera.GetScale() - 0.8f) > 0.00001f) { ++failures; }
    camera.Update(250);
    if (std::fabs(camera.GetScale() - 0.74142136f) > 0.00001f) { ++failures; }
    camera.Update(250);
    if (std::fabs(camera.GetScale() - 0.6f) > 0.00001f) { ++failures; }
    camera.SetScale(1);
    if (std::fabs(camera.GetScale() - 0.6f) > 0.00001f) { ++failures; }
    camera.Update(500);
    if (std::fabs(camera.GetScale() - 0.8f) > 0.00001f) { ++failures; }
    camera.Update(5000);
    if (camera.GetScale() != 1) { ++failures; }
    CMap map;
    CLevel level;
    CLevel::Template data;
    level.Bind(data, map);
    const std::int16_t argument = 128;
    level.FunctionResolver(14, &argument, 1);
    map.GetCamera().Update(1000);
    if (map.GetCamera().GetScale() != 0.5f) { ++failures; }
    level.Bind(data, map);
    if (map.GetCamera().GetScale() != 0.8f) { ++failures; }
    std::printf("[camera-check] interpolation/retarget/snap/native/restart failures=%u\n", failures);
    for (std::int16_t bit : {1, 4, 1, 31, 32, -1}) { level.FunctionResolver(82, &bit, 1); }
    if (level.GetStat42Bits() != 0x80000012u) { ++failures; }
    level.Bind(data, map);
    if (level.GetStat42Bits() != 0) { ++failures; }
    std::printf("[level-stat-check] record42/set-bit/idempotent/range/reset failures=%u\n", failures);
    const std::int16_t dialogArguments[] = {5, 0, 1};
    level.FunctionResolver(40, dialogArguments, 3);
    const unsigned dialogSerial = level.GetDialogSerial();
    if (level.GetDialogResource() != 5 || !level.DoesDialogAutoClose()) { ++failures; }
    level.FunctionResolver(70, nullptr, 0);
    if (level.GetDialogResource() != 5 || !level.IsDialogCloseRequested() ||
        level.GetDialogSerial() != dialogSerial) { ++failures; }
    level.CompleteDialog();
    if (level.GetDialogResource() != -1 || level.IsDialogCloseRequested()) { ++failures; }
    level.FunctionResolver(70, nullptr, 0);
    if (level.IsDialogCloseRequested()) { ++failures; }
    std::printf("[level-dialog-check] script-open/deferred-close/empty-close failures=%u\n", failures);
    level.FunctionResolver(66, nullptr, 0);
    level.FunctionResolver(61, nullptr, 0);
    level.Update(250);
    if (std::fabs(level.GetBrotherLabelAlpha() - 0.5f) > 0.00001f || level.GetStopwatchTime() != 250) { ++failures; }
    level.FunctionResolver(67, nullptr, 0);
    level.FunctionResolver(62, nullptr, 0);
    level.Update(100);
    if (std::fabs(level.GetBrotherLabelAlpha() - 0.3f) > 0.00001f || level.GetStopwatchTime() != 250) { ++failures; }
    level.Bind(data, map);
    if (level.GetBrotherLabelAlpha() != 0 || level.GetStopwatchTime() != 0) { ++failures; }
    std::printf("[level-clock-check] brother-label-fade/stopwatch/pause/restart failures=%u\n", failures);
    camera.Reset();
    camera.UpdatePosition(600, 500, 0, 0, 2000, 1500, 400, 300);
    camera.SetTarget(1200, 800);
    camera.SetCameraMode(2);
    camera.Update(500);
    camera.UpdatePosition(600, 500, 0, 0, 2000, 1500, 400, 300);
    if (std::fabs(camera.GetX() - 900) > 0.001f || std::fabs(camera.GetY() - 650) > 0.001f) { ++failures; }
    camera.Update(500);
    camera.UpdatePosition(600, 500, 0, 0, 2000, 1500, 400, 300);
    if (camera.GetX() != 1200 || camera.GetY() != 800) { ++failures; }
    camera.Shake(1000);
    camera.Shake(200);
    if (camera.GetShakeTime() != 1000) { ++failures; }
    camera.Update(1000);
    if (camera.GetShakeTime() != 0) { ++failures; }
    camera.SetCameraMode(0);
    camera.Update(1000);
    camera.UpdatePosition(5, 5, 0, 0, 2000, 1500, 400, 300);
    if (camera.GetX() != 200 || camera.GetY() != 150) { ++failures; }
    const std::int16_t disabled = 0;
    level.FunctionResolver(77, &disabled, 1);
    level.FunctionResolver(78, &disabled, 1);
    if (level.CanPlayerMove() || level.CanPlayerShoot()) { ++failures; }
    level.Bind(data, map);
    if (!level.CanPlayerMove() || !level.CanPlayerShoot()) { ++failures; }
    std::printf("[camera-position-check] target/follow/clamp/shake/input-gates failures=%u\n", failures);
    IndicatorWorld indicatorWorld;
    level.Bind(data, map, &indicatorWorld);
    const std::int16_t marker[] = {7, 1};
    level.FunctionResolver(49, marker, 2);
    level.UpdateIndicators(100, 20, 20, 984, 668);
    if (level.GetIndicators().size() != 1 || level.GetIndicators()[0].Alpha() != 1) { ++failures; }
    level.FunctionResolver(50, marker, 1);
    level.UpdateIndicators(100, 20, 20, 984, 668);
    if (level.GetIndicators().size() != 1 || std::fabs(level.GetIndicators()[0].Alpha() - 0.5f) > 0.001f) { ++failures; }
    level.UpdateIndicators(100, 20, 20, 984, 668);
    if (!level.GetIndicators().empty()) { ++failures; }
    level.FunctionResolver(49, marker, 2);
    level.UpdateIndicators(16, 1500, 20, 984, 668);
    level.UpdateIndicators(200, 1500, 20, 984, 668);
    if (!level.GetIndicators().empty() || level.SetIndicator(8, 1)) { ++failures; }
    level.FunctionResolver(49, marker, 2);
    level.Bind(data, map, &indicatorWorld);
    if (!level.GetIndicators().empty()) { ++failures; }
    std::printf("[indicator-check] native-create/remove cosine-fade=200ms onscreen-retire/restart failures=%u\n", failures);
    return failures;
}
}
