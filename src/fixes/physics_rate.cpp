// Runs the local player's movement physics at a fixed rate (PhysicsRate), so jumping,
// falling, fall damage and levitation are the same at every frame rate.
//
// Each frame the client runs, per moving spawn, a physics step (0x8D11A0) and then a
// collision pass (0x509050). The step integrates position with the elapsed time
// (pos += vel * elapsed_ms * 0.02) but applies friction and acceleration once per
// call with no time scaling (0x8D2160: vel = vel * friction + accel), and the
// collision pass judges "on the ground" from how far the spawn moved. At a few hundred
// fps each step barely moves the player, so jumps are cancelled almost immediately,
// falls never build up speed, and levitation stops bobbing.
//
// Friction is also applied once per step while gravity scales with step length, so even
// a capped rate is not enough: terminal velocity (and fall damage) depends on the exact
// step length. Instead we run the client's own step a whole number of times per frame,
// each exactly 1000/PhysicsRate ms long, carrying the remainder to the next frame, with
// the collision pass after every step. Frames that need no step skip both the step and
// its collision pass (running collision without a step zeroes vertical velocity).

#include <cstdint>

#include "../addresses.h"
#include "../config.h"
#include "../fixes.h"
#include "../log.h"
#include "../memory.h"

namespace physics_rate {
namespace {

using PhysicsStepFn = void(__fastcall*)(void* physics, void* edx, uint8_t* spawn);
using CollisionFn = int(__fastcall*)(uint8_t* spawn, void* edx);
using GetTimeMsFn = int(__fastcall*)(void* timer, void* edx);
using GameClockFn = int(__cdecl*)();

PhysicsStepFn g_original_step = nullptr;
CollisionFn g_original_collision = nullptr;

// The spawn whose step was skipped most recently; its collision pass is skipped too.
uint8_t* g_skipped_spawn = nullptr;

template <typename T>
T& Global(uint32_t address) {
  return *reinterpret_cast<T*>(mem::Rebase(address));
}

int GameTimeMs() {
  void* timer = Global<void*>(addr::kGameTimerPtr);
  auto get_time = *reinterpret_cast<GetTimeMsFn*>(*static_cast<uintptr_t**>(timer));
  return get_time(timer, nullptr);
}

// The spawn whose physics the player drives: their mount if riding, else themselves.
uint8_t* LocalPhysicsSpawn() {
  auto* player = Global<uint8_t*>(addr::kLocalPlayerPtr);
  if (!player) return nullptr;
  auto* mount = *reinterpret_cast<uint8_t**>(player + addr::spawn::kMount);
  return mount ? mount : player;
}

// Upper bound on catch-up steps in one frame; time beyond that is dropped (e.g. a hitch).
constexpr int kMaxStepsPerFrame = 8;
// Gaps longer than this (zoning, alt-tab) resynchronize instead of catching up.
constexpr int kResyncGapMs = 250;

void __fastcall PhysicsStepHook(void* physics, void* edx, uint8_t* spawn) {
  if (!spawn || spawn != LocalPhysicsSpawn()) {
    g_original_step(physics, edx, spawn);
    return;
  }

  static uint8_t* simulated_spawn = nullptr;
  static double simulated_until_ms = 0;  // game time the fixed-rate simulation has reached
  static int previous_frame_ms = 0;

  int now = GameTimeMs();
  int frame_ms = previous_frame_ms ? now - previous_frame_ms : 0;
  previous_frame_ms = now;
  int& spawn_timer = *reinterpret_cast<int*>(spawn + addr::spawn::kPhysicsTimer);
  double step_ms = 1000.0 / g_config.physics_rate;

  double behind_ms = now - simulated_until_ms;
  if (spawn != simulated_spawn || spawn_timer <= 0 || behind_ms < 0 || behind_ms > kResyncGapMs) {
    // (Re)start: let the client run its own step and simulate forward from here.
    simulated_spawn = spawn;
    simulated_until_ms = now;
    g_skipped_spawn = nullptr;
    g_original_step(physics, edx, spawn);
    return;
  }

  int steps = 0;
  while (now - simulated_until_ms >= step_ms && steps < kMaxStepsPerFrame) {
    // The game runs the collision pass after this hook returns; run it ourselves between
    // the extra steps so every step is followed by one, as at the fixed rate.
    if (steps > 0) g_original_collision(spawn, nullptr);
    // Steps are whole milliseconds; alternate lengths (e.g. 16/17) so they average step_ms.
    int length_ms = static_cast<int>(simulated_until_ms + step_ms) - static_cast<int>(simulated_until_ms);
    simulated_until_ms += step_ms;
    spawn_timer = now - length_ms;  // the step derives its time factor from this
    g_original_step(physics, edx, spawn);
    ++steps;
  }
  if (now - simulated_until_ms >= step_ms) simulated_until_ms = now;  // dropped time

  g_skipped_spawn = steps == 0 ? spawn : nullptr;
  // The step stores its time factor here for the camera code; keep it per frame.
  Global<float>(addr::kLocalPhysicsTimeFactor) = frame_ms * addr::kPhysicsMsToTicks;
}

int __fastcall CollisionHook(uint8_t* spawn, void* edx) {
  if (spawn && spawn == g_skipped_spawn) {
    // The one side effect of the collision pass that other code relies on every frame.
    auto* display = Global<uint8_t*>(addr::kDisplayPtr);
    if (display)
      *reinterpret_cast<int*>(display + addr::kDisplayLastMoveTime) =
          reinterpret_cast<GameClockFn>(mem::Rebase(addr::kGameClockMs))();
    return 1;
  }
  return g_original_collision(spawn, edx);
}

// push -1; push <SEH handler>  (the handler address is relocated, so only the first 3 bytes)
const uint8_t kStepPrologue[] = {0x6A, 0xFF, 0x68};

bool PatchCollisionCalls() {
  uintptr_t target = mem::Rebase(addr::kSpawnCollision);
  for (uint32_t site : addr::kSpawnCollisionCalls) {
    uintptr_t at = mem::Rebase(site);
    auto* bytes = reinterpret_cast<const uint8_t*>(at);
    int32_t rel;
    __builtin_memcpy(&rel, bytes + 1, sizeof(rel));
    if (bytes[0] != 0xE8 || at + 5 + rel != target) {
      Log("physics_rate: collision call at %08X does not match", static_cast<unsigned>(at));
      return false;
    }
  }
  for (uint32_t site : addr::kSpawnCollisionCalls) {
    uintptr_t at = mem::Rebase(site);
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&CollisionHook) - (at + 5));
    mem::Write(at + 1, &rel, sizeof(rel));
  }
  g_original_collision = reinterpret_cast<CollisionFn>(target);
  return true;
}

}  // namespace

void ApplyEqgame() {
  if (!g_config.physics_rate_fix) return;
  uintptr_t slot = mem::Rebase(addr::kPhysicsVtbl + addr::kPhysicsStepSlot);
  uintptr_t step = mem::Rebase(addr::kPhysicsStep);
  if (*reinterpret_cast<uintptr_t*>(slot) != step ||
      !mem::Verify(step, kStepPrologue, sizeof(kStepPrologue), "physics_rate (physics step)")) {
    Log("physics_rate: physics vtable does not match, not installing");
    return;
  }
  if (!PatchCollisionCalls()) return;
  g_original_step = reinterpret_cast<PhysicsStepFn>(step);
  mem::WriteValue(slot, reinterpret_cast<uintptr_t>(&PhysicsStepHook));
  Log("physics_rate: installed (fixed %d Hz)", g_config.physics_rate);
}

}  // namespace physics_rate
