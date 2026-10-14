#include "actor_gl.h"
#include "ambient_animals.h"
#include "archive.h"
#include "enemy_encounter.h"
#include "enemy_group.h"
#include "forest_animals.h"
#include "village_animals.h"
#include "dog_traps.h"
#include "event_playback.h"
#include "event_actor_gl.h"
#include "environment.h"
#include "foot_shadow.h"
#include "world_effects.h"
#include "room_area_effects.h"
#include "input.h"
#include "ports/rnd.h"
#include "enemy_routes.h"
#include "ports/yz2.h"
#include "room_flow.h"
#include "room_ladders.h"
#include "room_objects.h"
#include "room_blocks.h"
#include "room_interactions.h"
#include "room_items.h"
#include "room_pickups.h"
#include "room_item_events.h"
#include "item_rules.h"
#include "item_effects.h"
#include "item_effects_gl.h"
#include "inventory_case.h"
#include "save_native.h"
#include "save_clock.h"
#include "subscreen_data.h"
#include "startup_data.h"
#include "startup_compositor.h"
#include "r100_scenario.h"
#include "r101_scenario.h"
#include "village_binoculars.h"
#include "weapon_carry_source.h"
#include "village_bottles.h"
#include "player_projectiles.h"
#include "village_flow.h"
#include "r100_secondary.h"
#include "radio_term.h"
#include "radio_intro.h"
#include "radio_term_gl.h"
#include "sound_footsteps.h"
#include "display_brightness_gl.h"
#include "depth_of_field_gl.h"
#include "subscreen_item_gl.h"
#include "subscreen_portrait_gl.h"
#include "graphics_options_gl.h"
#include "object_actions.h"
#include "destruction_effects.h"
#include "sound_bank.h"
#include "sound_mixer.h"
#include "sound_acoustics.h"
#include "sound_ambient.h"
#include "muzzle_effects.h"
#include "world_effects_gl.h"
#include "room_effect_cloth_gl.h"
#include "r100_room_hit_effects.h"
#include "sat.h"
#include "scenario_collision.h"
#include "scene.h"
#include "room_shadows.h"
#include "scene_gl.h"
#include "smd.h"
#include "texture.h"
#include "walk_probe.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <atomic>
#include <android/log.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <jni.h>
#include <memory>
#include <map>
#include <limits>
#include <mutex>
#include <sstream>
namespace {
std::mutex stateMutex;
std::mutex subscreenAssetMutex;
std::mutex startupAssetMutex;
std::shared_ptr<const re4::TitleData> startupArt;
re4::SoundMixer startupMixer;
std::atomic<bool> audioStartupActive{false};
std::vector<std::pair<unsigned,unsigned>> pendingStartupAudio;
std::atomic<std::uint64_t> startupVoice{0}; // Audio worker owns title channel 6.
std::atomic<bool> startupVoiceComplete{true};
std::atomic<std::uint64_t> startupLoadVoice{0};
std::atomic<bool> startupLoadComplete{true},startupLoadFence{false};
std::string startupConfiguredRoot;
re4::InputBuffer input;
re4::InputFrame frame;
std::string graphics = "Initializing graphics";
std::string hudInfo; // Short gameplay HUD; render publishes under stateMutex.
// life/max, loaded/reserve/weapon, action/button, tick/epoch,
// active partner actor id, Ashley life/max/condition, player type.
std::array<jint,14> cockpitValues{}; // Immutable publication; UI never reads simulation owners.
std::array<jint,4> binocularValues{};
std::array<jint,4> scopeValues{};
std::array<jint,3> eventTitleValues{};
std::string hudRoomName;
std::atomic<bool> binocularUiActive{};
// Integration point for SubCharInit/Exit and SubCharGetCondition. Presence
// follows the actual partner actor (id3), never a saved scenario flag: Luis
// and Ashley in a different room must not expose an Ashley meter.
unsigned activePartnerId{},activePartnerCondition{};
unsigned cockpitTicks{},cockpitEpoch{};
double cockpitUiRemainder{};
bool sceneStatus = false; // UI status format, protected by stateMutex.
bool inspectorRequested = false;
bool useGreenRequested = false; // UI mailbox; consumed only by draw().
std::string inventoryInfo; // Published by the GL thread under stateMutex.
std::string inventoryMessage; // Last explicit item result, GL-thread owned.
std::string caseInfo, pickupInfo; // Published snapshots, stateMutex protected.
std::vector<unsigned> pendingMenuAudio; // UI-to-audio mailbox, stateMutex protected.
std::string subscreenInfo;
bool subscreenOpen{}; // GL-thread-owned pause reason, independent of item-exam.
bool subscreenExamining{};
bool subscreenItemDirty{};
std::string subscreenAssetRoot;
constexpr unsigned subscreenLanguage = 1; // Fixed English MDT slot.
std::map<std::string,re4::SubscreenArt> subscreenArt;
std::unique_ptr<re4::TitleData> binocularArtwork;
std::unique_ptr<re4::TitleData> villageTitleArtwork;
std::map<std::string,std::unique_ptr<re4::TitleData>> scopeArtwork;
std::map<std::string,re4::Texture> subscreenImages;
// Portrait construction evaluates original Motion64. Only the paused GL owner
// prepares it; the UI asset worker reads these immutable CPU images.
std::map<unsigned,re4::Texture> subscreenPortraits;
std::unique_ptr<re4::OriginalMenuModel> subscreenLeon;
std::string subscreenLeonRoot;
unsigned subscreenLeonWeapon=~0u;
std::shared_ptr<const std::vector<re4::Texture>> coreSpecular;
int subscreenItemId = -1;
std::chrono::steady_clock::time_point subscreenFrame;
re4::CollisionVec ladderWindowFrom{};
bool ladderWindowSweep{};
re4::InventoryCase inventoryCase;
std::atomic<bool> saveMenuRequested{false};
std::optional<re4::OriginalSaveCard> pendingSavedGame;
std::optional<re4::OriginalSaveCard> pendingSaveWrite;
std::uint64_t saveWriteSequence{},pendingSaveWriteId{};
std::optional<re4::OriginalSaveSections> originalSaveBasis;
re4::NativeSaveSnapshot originalSaveMetadata;
std::optional<re4::NativeSaveSnapshot> entryCheckpoint,pendingContinue;
std::optional<re4::OriginalSaveSections> checkpointBasis;
std::atomic<bool> deathRequested{false};
std::atomic<bool> continueConfirmed{false};
unsigned deathTicks{};
std::vector<jint> deathPicture; // Completed default framebuffer, guarded by stateMutex.
re4::OriginalSaveClock originalSaveClock;
std::mutex saveClockMutex;
const re4::EnemyGroup *saveAccuracyOwner{};
std::uint64_t saveAccuracyCursor{};
std::map<const re4::EnemyEncounter*,unsigned> saveKillCursors;
std::uint32_t saveWallSeconds() {
  return std::uint32_t(std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}
void setSaveClockRunning(bool running) {
  std::lock_guard<std::mutex> lock(saveClockMutex);
  originalSaveClock.setRunning(saveWallSeconds(),running);
}
jlong savedGameSession{};
bool weaponGpuDirty{};
struct PendingPickup {
  std::uint64_t token{};
  std::uint16_t room{};
  std::size_t index{};
  unsigned id{}, count{};
  bool loose{};
};
PendingPickup pendingPickup;
std::uint64_t nextPickupToken{}, pickupAnswerToken{};
bool pickupAnswerAccepted{};
bool pickupAnswerUse{};
GLuint program{}, vao{}, vbo{}, texture{};
int width = 1, height = 1, texWidth = 1, texHeight = 1;
bool hasTexture = false;
bool sceneMode = false;
bool normalView =
    false; // Protected by stateMutex; only toggled by the GL thread.
std::unique_ptr<re4::Smd> previewSmd, previewCommon;
std::unique_ptr<re4::Smx> previewSmx;
std::unique_ptr<re4::EnvironmentData> previewEnvironment;
std::unique_ptr<re4::WorldEffectsData> previewWorldEffects;
std::unique_ptr<re4::RoomAreaEffectsData> roomAreaEffectsData;
std::unique_ptr<re4::RoomAreaEffects> roomAreaEffects;
std::unique_ptr<re4::R100RoomHitEffects> roomHitEffects;
std::unique_ptr<re4::DestructionEffectsData> destructionData;
std::unique_ptr<re4::SoundAcoustics> roomAcoustics;
std::unique_ptr<re4::SoundAmbient> roomAmbient;
std::string musicAssetRoot;
std::map<unsigned,std::shared_ptr<const re4::SoundSequence>> musicCache;
// SHD songs can hold tens of megabytes of decoded PCM. Retain only songs still
// owned by a live/prepared room or the mixer while preserving adjacent-room
// playback identity, rather than accumulating every track visited this session.
std::map<unsigned,std::weak_ptr<const re4::SoundStream>> streamCache;
std::unique_ptr<re4::MuzzleEffects> muzzleEffects;
std::unique_ptr<re4::WorldEffectsData> enemyEffectsData;
std::unique_ptr<re4::WorldEffectsSimulation> enemyEffects;
std::array<std::uint64_t,4> enemyVoiceHandles{};
std::map<const re4::EnemyEncounter*,std::array<std::uint64_t,4>> groupVoiceHandles;
unsigned muzzleWeaponId{};
std::uint64_t windowEffectSerial{};
std::vector<std::uint8_t> coreEnvironmentEff;
std::shared_ptr<const re4::LaserSightData> laserSightData;
std::shared_ptr<const re4::Texture> footShadowTexture;
std::shared_ptr<const re4::RoomLighting> coreLighting;
std::unique_ptr<re4::RoomObjectLayer> previewObjects;
std::unique_ptr<re4::RoomBlockLayer> previewBlocks;
std::unique_ptr<re4::Sat> previewSat;
std::unique_ptr<re4::Sat> previewEat;
std::unique_ptr<re4::SatCollision> effectWorld;
std::unique_ptr<re4::DestructionEffectsSimulation> destruction;
std::unique_ptr<re4::EnemyEncounter> encounter;
std::shared_ptr<const re4::EnemyRoutes> enemyRoutes;
const re4::EnemyEncounter *enemyEffectsOwner{};
std::unique_ptr<re4::ObjectActions> objectActions;
std::unique_ptr<re4::RoomItemLayer> previewItems;
std::unique_ptr<re4::RoomItemLayer> previewLooseItems;
std::shared_ptr<const re4::RoomArchive> pickupArchive;
std::unique_ptr<re4::RoomPickups> roomPickups;
std::unique_ptr<re4::RoomItemEvents> roomItemEvents;
double roomItemEventClock{};
std::unique_ptr<re4::ItemEffects> itemEffects;
// Session-only state. Reopening a test room must not replenish its drops.
std::map<std::uint16_t, re4::ObjectActionsSnapshot> objectRoomStates;
re4::ScenarioState scenarioState;
std::unique_ptr<re4::RoomInteractions> roomInteractions;
std::unique_ptr<re4::AmbientAnimals> ambientAnimals;
std::unique_ptr<re4::ForestAnimals> forestAnimals;
std::unique_ptr<re4::VillageAnimals> villageAnimals;
std::map<unsigned,std::uint64_t> villageAnimalVoices;
std::unique_ptr<re4::DogTraps> dogTraps;
std::map<unsigned,std::uint64_t> dogTrapVoiceHandles;
std::unique_ptr<re4::WorldEffectsData> trapEffectsData;
std::unique_ptr<re4::WorldEffectsSimulation> trapEffects;
std::map<std::uint64_t,re4::DogTrapEffect> trapEffectParents;
std::set<unsigned> r100UsedTraps;
std::map<std::uint16_t,std::vector<re4::RoomPickupRecord>> runtimeRoomDrops;
re4::EnemyDropState enemyDropState;
bool forestGunFired{};
bool r100PlayerFootstep{};
std::shared_ptr<const re4::SoundStream> dogRescueStream;
re4::EnemyDropContext currentEnemyDropContext;
std::set<unsigned> r100DroppedIndices;
std::string chickenAssetPath, chickenSpawnPath;
std::string enemyAssetPath, enemySpawnPath;
std::string playerAssetPath, weaponAssetPath;
std::string handgunAssetPath, shotgunAssetPath;
std::unique_ptr<re4::LeonPreview> leon;
std::string coreAssetPath, staticAudioProfile;
re4::SoundMixer soundMixer;
re4::SoundMixer eventMixer; // Original stream block1 speech is independent of persistent block0 room BGM.
std::atomic<bool> audioUiActive{false}, audioGameActive{false};
std::atomic<bool> audioSubscreenActive{false};
// Owned by the GL item-get transaction, including both UI handoff gaps.
std::atomic<bool> audioPickupActive{false};
std::mutex audioGateMutex;
std::uint64_t pyreVoice{};
re4::PlayerAction lastSoundAction = re4::PlayerAction::Footwork;
re4::WeaponMotion lastWeaponSoundMotion = re4::WeaponMotion::Idle;
float lastWeaponSoundFrame{};
void syncAudioPause() {
  std::lock_guard<std::mutex> gate(audioGateMutex);
  soundMixer.setSubscreen(audioSubscreenActive.load() && !audioPickupActive.load());
  soundMixer.setPaused(!(audioUiActive.load() && (audioGameActive.load() || audioSubscreenActive.load() || audioPickupActive.load())));
  eventMixer.setPaused(!(audioUiActive.load() && (audioGameActive.load() || audioPickupActive.load())));
}
std::uint64_t playRoomSound(unsigned block, unsigned number, const re4::CollisionVec &source,unsigned argument=0) {
  if (!roomAcoustics || !leon) return soundMixer.play(block, number,1,-1,true,argument);
  const auto camera = re4::SceneFollowPose();
  return soundMixer.playSpatial(block, number, *roomAcoustics, source,
                                leon->feet(), camera.eye, camera.right,true,argument);
}
void updatePyreAudio() {
  if (!roomAcoustics || !leon || !previewWorldEffects ||
      !previewWorldEffects->originalPyre) return;
  if (!pyreVoice && audioUiActive.load() && audioGameActive.load())
    pyreVoice = playRoomSound(6, 0x58, {-2880, 1550, 2810});
  if (!pyreVoice) return;
  const auto camera = re4::SceneFollowPose();
  soundMixer.updateSpatial(pyreVoice, *roomAcoustics, {-2880, 1550, 2810},
                          leon->feet(), camera.eye, camera.right);
}
re4::CollisionVec playerPartPosition(unsigned part) {
  const auto matrix = re4::WeaponAttachmentMatrix(
      leon->rig().jointMatrices().at(part), leon->placement());
  const auto point = re4::TransformPoint(matrix, {0, 0, 0});
  return {point[0], point[1], point[2]};
}
bool collisionView = false; // GL-thread owned diagnostic state.
std::unique_ptr<re4::SatCollision> collisionWorld;
std::unique_ptr<re4::ScenarioCollision> scenarioCollision;
std::unique_ptr<re4::WalkProbe> walkProbe;
std::unique_ptr<re4::RoomFlow> roomFlow;
std::unique_ptr<re4::R101Scenario> villageScenario;
std::unique_ptr<re4::VillageBinoculars> villageBinocular;
std::unique_ptr<re4::VillageBottles> villageBottles;
std::unique_ptr<re4::PlayerProjectiles> playerProjectiles;
unsigned projectileGpuGeneration{};
std::unique_ptr<re4::EnemySpawns> villageSpawns;
std::array<std::unique_ptr<re4::OriginalEvent>,3> villageEvents;
std::optional<re4::VillageEvent> villageEvent;
std::optional<re4::CollisionVec> villageReturnPosition;
float villageReturnHeading{};
std::shared_ptr<const re4::SoundStream> villageCombatStream;
std::unique_ptr<re4::RadioIntro> villageRadioIntro;
std::shared_ptr<const re4::SoundStream> villageRadio12Intro;
re4::NativeSaveSnapshot captureNativeGame();
unsigned roomEntryTicks{};
std::array<std::unique_ptr<re4::R100RoomCamera>,2> villageCameras;
std::optional<unsigned> villageWaveCamera;
std::array<std::shared_ptr<const re4::SoundStream>,2> villageWaveStreams;
std::vector<unsigned> villageWaveActors;
void finishVillageWave();
void drainVillageCues();
void beginVillageEvent(re4::VillageEvent event);
void finishVillageEvent(bool canceled=false);
std::unique_ptr<re4::R100Scenario> r100Scenario;
std::unique_ptr<re4::FootstepSounds> roomFootsteps;
bool campaignSceneDirty{};
std::unique_ptr<re4::EnemyGroup> enemyGroup;
std::unique_ptr<re4::EnemyEncounter> preparedFirstEnemy;
std::unique_ptr<re4::OriginalEvent> r100FirstEvent, r100TruckEvent;
std::array<std::unique_ptr<re4::OriginalEvent>,3> openingEvents;
std::array<std::unique_ptr<re4::OriginalEvent>,4> secondaryEvents;
std::unique_ptr<re4::R100Secondary> r100Secondary;
std::unique_ptr<re4::EnemyEncounter> r100HouseEnemy;
std::unique_ptr<re4::R100MessageRuntime> r100RoomMessage;
std::shared_ptr<const re4::SoundStream> r100HouseStream,r100RoomStream,r100SkullStream;
std::array<std::shared_ptr<const re4::RadioTermData>,13> r100RadioData;
std::unique_ptr<re4::RadioTermRuntime> r100RadioTerm;
std::shared_ptr<const re4::SoundStream> r100RadioIntroStream;
std::optional<unsigned> r100RadioQueued;
unsigned r100RadioCall=~0u;
std::array<std::uint32_t,3> r100RadioHeard{};
unsigned r100RadioMessageNo=24;
std::optional<re4::CollisionVec> r100RadioReturnPosition;
float r100RadioReturnHeading{};
// OpeSetOpenTermEnd's two independent FadeSetW slots, source30Hz.
std::optional<unsigned> r100RadioFadeFrame;
std::array<bool,3> r100RadioControls{};
bool r100MessageConfirm{};
std::atomic<bool> messageUiActive{false};
std::optional<re4::CollisionVec> secondaryReturnPosition;
float secondaryReturnHeading{};
bool r100OfficerPressed{},r100WeaponDontFire{};
bool configuredOpeningCanceled{};
re4::OriginalEvent *activeEvent{};
std::atomic<bool> eventUiActive{false}; // UI reads only this mailbox, never the GL-owned pointer.
unsigned r100EventPhase{}; //1=s03,2=s20,3..5=opening,6..9=optional EVD,10=house,11=room MDT/CAM,12/13=radio.
double eventAccumulator{};
bool r100DeathPending{};
std::set<unsigned> r100DeadIndices;
std::vector<std::uint64_t> eventVoiceHandles;

std::unique_ptr<re4::RoomLadders> roomLadders;
std::shared_ptr<const re4::RoomLighting> previewLighting;
std::shared_ptr<const re4::RoomShadowData> previewShadows;
std::unique_ptr<re4::RoomInfo> roomInfo;
std::uint16_t activeRoomId{};
unsigned activePart{};
std::int64_t sceneSession{};
std::int64_t playModeSession{};
std::int64_t testProfileSession{};
unsigned testProfile{}; // 1 = explicit house-ladder action sandbox, not campaign progress.
std::atomic<unsigned> testCheatFlags{0};
bool campaignMode{};
unsigned campaignDifficulty{};
std::int64_t campaignSession{};
std::atomic<bool> aimInverted{false};
std::atomic<bool> gyroEnabled{false},gyroPresent{false};
std::mutex gyroMutex;
float gyroYawRate{},gyroPitchRate{};
std::chrono::steady_clock::time_point gyroSample;
extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_NativeBridge_gyroAvailable(JNIEnv *,jclass,jboolean available) {
 gyroPresent.store(available==JNI_TRUE);if(!gyroPresent.load())gyroEnabled.store(false);
}
extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_NativeBridge_gyroRate(JNIEnv *,jclass,jfloat yaw,jfloat pitch) {
 std::lock_guard<std::mutex> lock(gyroMutex);
 gyroYawRate=std::isfinite(yaw)?std::clamp(yaw,-3.f,3.f):0;
 gyroPitchRate=std::isfinite(pitch)?std::clamp(pitch,-3.f,3.f):0;
 gyroSample=std::chrono::steady_clock::now();
}
std::atomic<int> displayBrightness{64};
bool exploreMode = true;
bool initialCombatTest = false;
bool paused = false;
std::string flowInfo;
struct RoomRequest {
  std::uint64_t token{};
  std::uint16_t room{};
  unsigned part{};
  re4::CollisionVec position{};
  float heading{};
  bool combat{};
  bool village{};
};
// Published to the UI under stateMutex. All scene changes stay on the GL
// thread.
RoomRequest roomRequest;
struct RoomCompletion {
  std::uint64_t token{};
  std::string path, error;
};
RoomCompletion roomCompletion;
void applyRoomCompletion();
std::uint64_t nextRoomToken{};
std::string playerInfo;
bool actorGpuReady = false;
bool walking = false;
std::chrono::steady_clock::time_point walkFrame, walkLog;
std::uint64_t inputGeneration{},
    seenInputGeneration{}; // generation is mutex protected.
void suspendSimulationInput() {
  if (leon)
    leon->suspendClock();
  if (encounter)
    encounter->suspendInput();
  if (enemyGroup) enemyGroup->suspendInput();
  if (ambientAnimals)
    ambientAnimals->suspendClock();
  if(forestAnimals)forestAnimals->suspendClock();
}
void resetWalk() {
  walking = false;
  cockpitTicks=0;cockpitUiRemainder=0;++cockpitEpoch;
  deathTicks=0;deathRequested.store(false);continueConfirmed.store(false);
  r100PlayerFootstep=false;
  if (walkProbe) walkProbe->setObjectCollision(nullptr);
  roomItemEvents.reset();roomItemEventClock=0;
  roomInteractions.reset();
  roomLadders.reset();
  ambientAnimals.reset();
  forestAnimals.reset();villageAnimals.reset();villageAnimalVoices.clear();dogTraps.reset();
  for(const auto &voice:dogTrapVoiceHandles)soundMixer.stop(voice.second);
  dogTrapVoiceHandles.clear();
  trapEffects.reset();trapEffectsData.reset();trapEffectParents.clear();
  roomAreaEffects.reset();roomAreaEffectsData.reset();
  roomHitEffects.reset();
  encounter.reset();
  enemyRoutes.reset();
  enemyGroup.reset();preparedFirstEnemy.reset();
  activeEvent=nullptr;eventUiActive.store(false);r100EventPhase=0;eventAccumulator=0;r100DeathPending=false;
  villageBinocular.reset();binocularUiActive.store(false);
  r100FirstEvent.reset();r100TruckEvent.reset();
  villageBottles.reset();villageScenario.reset();villageSpawns.reset();villageEvent.reset();villageReturnPosition.reset();villageRadioIntro.reset();villageRadio12Intro.reset();roomEntryTicks=0;
  playerProjectiles.reset();projectileGpuGeneration=0;
  for(auto &event:villageEvents)event.reset();
  villageCombatStream.reset();villageWaveCamera.reset();villageWaveActors.clear();
  for(auto &camera:villageCameras)camera.reset();
  villageWaveStreams={};
  for(auto &event:openingEvents)event.reset();
  for(auto &event:secondaryEvents)event.reset();
  r100Secondary.reset();r100HouseEnemy.reset();r100RoomMessage.reset();
  r100HouseStream.reset();r100RoomStream.reset();r100SkullStream.reset();secondaryReturnPosition.reset();
  r100RadioData={};r100RadioTerm.reset();r100RadioIntroStream.reset();r100RadioQueued.reset();
  r100RadioCall=~0u;r100RadioReturnPosition.reset();r100RadioFadeFrame.reset();r100RadioControls={};re4::DestroyRadioTermGl();
  r100OfficerPressed=r100WeaponDontFire=false;
  r100MessageConfirm=false;messageUiActive.store(false);audioPickupActive.store(false);
  for(const auto handle:eventVoiceHandles)soundMixer.stop(handle);
  eventVoiceHandles.clear();
  eventMixer.clear();
  for(auto &owner:groupVoiceHandles)for(auto handle:owner.second)soundMixer.stop(handle);
  groupVoiceHandles.clear();
  r100Scenario.reset();
  roomFootsteps.reset();
  campaignSceneDirty=false;
  enemyEffects.reset();enemyEffectsData.reset();enemyEffectsOwner=nullptr;
  for(auto &handle:enemyVoiceHandles) {soundMixer.stop(handle);handle=0;}
  objectActions.reset();
  previewItems.reset();
  roomPickups.reset();
  itemEffects.reset();
  previewLooseItems.reset();pickupArchive.reset();
  destruction.reset();
  destructionData.reset();
  roomAcoustics.reset();
  roomAmbient.reset();
  effectWorld.reset();
  previewEat.reset();
  walkProbe.reset();
  collisionWorld.reset();
  scenarioCollision.reset();
  walkFrame = {};
  walkLog = {};
}
void logWalk() {
  if (!walkProbe || !walkProbe->ready())
    return;
  auto p = walkProbe->position();
  auto g = *walkProbe->floorHit();
  std::ostringstream report;
  report << std::fixed << std::setprecision(1) << "Test actor: " << p[0] << ", "
         << p[1] << ", " << p[2] << " · Floor #" << g.polygon << " Attr 0x"
         << std::hex << g.attributes << std::dec << " · Wall "
         << (walkProbe->wallHit()
                 ? std::to_string(walkProbe->wallHit()->polygon)
                 : "clear")
         << " · Edge " << (walkProbe->edgeBlocked() ? "locked" : "clear");
  __android_log_print(ANDROID_LOG_INFO, "RE4Walk", "%s", report.str().c_str());
}

std::vector<int> previewPlacements;
int selectedPlacement = -1;
std::string roomName(std::uint16_t id) {
  std::ostringstream text;
  text << 'r' << std::hex << std::setw(3) << std::setfill('0') << id;
  return text.str();
}
std::uint16_t parseRoom(const std::string &text) {
  if (text.size() != 4 || text[0] != 'r')
    throw std::runtime_error("Invalid room ID");
  unsigned id = 0;
  for (unsigned i = 1; i < 4; ++i) {
    const char c = text[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      throw std::runtime_error("Invalid room ID");
    id = id * 16 + (c <= '9' ? c - '0' : c - 'a' + 10);
  }
  return static_cast<std::uint16_t>(id);
}
std::string jstringValue(JNIEnv *env, jstring value) {
  if (!value)
    return {};
  const char *text = env->GetStringUTFChars(value, nullptr);
  if (!text)
    throw std::runtime_error("JNI string is missing");
  std::string result(text);
  env->ReleaseStringUTFChars(value, text);
  return result;
}
std::vector<std::uint8_t> readRoomInfo(const std::string &path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in || in.tellg() != 10790)
    throw std::runtime_error("roominfo.dat is missing or has the wrong size");
  std::vector<std::uint8_t> bytes(10790);
  in.seekg(0);
  if (!in.read(reinterpret_cast<char *>(bytes.data()), bytes.size()))
    throw std::runtime_error("roominfo.dat is truncated");
  return bytes;
}
struct NativeLoadClock {
  const char *phase;unsigned room;
  std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
  NativeLoadClock(const char *label,unsigned id):phase(label),room(id) {}
  ~NativeLoadClock() {
    const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    __android_log_print(ANDROID_LOG_INFO,"RE4Load","r%03x · %s · %.1f ms",room,phase,ms);
  }
};
struct PreparedRoom {
  std::unique_ptr<re4::LeonPreview> player;
  bool replacePlayer{};
  std::unique_ptr<re4::Smd> smd, common;
  std::unique_ptr<re4::Smx> smx;
  std::unique_ptr<re4::EnvironmentData> environment;
  std::shared_ptr<const re4::RoomLighting> lighting;
  std::shared_ptr<const re4::RoomShadowData> shadows;
  std::unique_ptr<re4::WorldEffectsData> worldEffects;
  std::unique_ptr<re4::RoomAreaEffectsData> areaData;
  std::unique_ptr<re4::RoomAreaEffects> areas;
  std::unique_ptr<re4::DestructionEffectsData> fragments;
  std::unique_ptr<re4::SoundAcoustics> acoustics;
  std::unique_ptr<re4::SoundAmbient> ambient;
  std::array<std::shared_ptr<const re4::SoundSequence>,2> music;
  std::shared_ptr<const re4::SoundStream> stream, rescueStream;
  std::unique_ptr<re4::RoomObjectLayer> objects;
  std::unique_ptr<re4::RoomBlockLayer> blocks;
  std::unique_ptr<re4::Sat> sat;
  std::unique_ptr<re4::Sat> eat;
  std::unique_ptr<re4::SatCollision> world;
  std::unique_ptr<re4::SatCollision> effect;
  std::unique_ptr<re4::ScenarioCollision> scenario;
  std::unique_ptr<re4::DestructionEffectsSimulation> destructionSimulation;
  std::vector<std::shared_ptr<const re4::SoundBank>> sounds;
  std::unique_ptr<re4::ObjectActions> actions;
  std::unique_ptr<re4::RoomInteractions> interactions;
  std::unique_ptr<re4::RoomItemLayer> items;
  std::unique_ptr<re4::RoomItemLayer> looseItems;
  std::shared_ptr<const re4::RoomArchive> itemArchive;
  std::unique_ptr<re4::RoomPickups> pickups;
  std::unique_ptr<re4::RoomItemEvents> itemEvents;
  std::unique_ptr<re4::ItemEffects> itemEffectsOwner;
  std::unique_ptr<re4::EnemyEncounter> enemy, firstEnemy;
  std::shared_ptr<const re4::EnemyRoutes> routes;
  std::unique_ptr<re4::R100RoomHitEffects> surfaceHits;
  std::unique_ptr<re4::EnemyGroup> group;
  std::unique_ptr<re4::OriginalEvent> firstEvent, truckEvent;
  std::array<std::unique_ptr<re4::OriginalEvent>,3> opening;
  std::array<std::unique_ptr<re4::OriginalEvent>,4> secondary;
  std::unique_ptr<re4::R100Secondary> secondaryScript;
  std::unique_ptr<re4::EnemyEncounter> houseEnemy;
  std::unique_ptr<re4::R100MessageRuntime> roomMessage;
  bool messageConfirm{};
  std::shared_ptr<const re4::SoundStream> houseStream,roomStream,skullStream;
  std::array<std::shared_ptr<const re4::RadioTermData>,13> radioData;
  std::unique_ptr<re4::RadioTermRuntime> radioTerm;
  std::shared_ptr<const re4::SoundStream> radioIntroStream;
  std::optional<unsigned> radioQueued;
  unsigned radioCall=~0u;
  std::optional<re4::CollisionVec> radioReturnPosition;
  float radioReturnHeading{};
  std::optional<unsigned> radioFadeFrame;
  std::array<bool,3> radioControls{};
  std::vector<std::shared_ptr<const re4::SoundStream>> addressedStreams;
  std::optional<re4::CollisionVec> returnPosition;
  float returnHeading{};
  re4::OriginalEvent *event{};
  unsigned eventPhase{};
  double eventClock{};
  bool deathPending{};
  std::unique_ptr<re4::WorldEffectsData> feedbackData;
  std::unique_ptr<re4::WorldEffectsSimulation> feedback;
  const re4::EnemyEncounter *feedbackOwner{};
  std::array<std::uint64_t,4> feedbackVoices{};
  std::map<const re4::EnemyEncounter*,std::array<std::uint64_t,4>> groupVoices;
  std::unique_ptr<re4::AmbientAnimals> animals;
  std::unique_ptr<re4::ForestAnimals> forest;
  std::unique_ptr<re4::VillageAnimals> villageAnimalsOwner;
  std::map<unsigned,std::uint64_t> villageAnimalVoiceOwners;
  std::unique_ptr<re4::DogTraps> traps;
  std::map<unsigned,std::uint64_t> trapVoices;
  std::unique_ptr<re4::WorldEffectsData> trapData;
  std::unique_ptr<re4::WorldEffectsSimulation> trapSimulation;
  std::map<std::uint64_t,re4::DogTrapEffect> trapParents;
  std::unique_ptr<re4::WalkProbe> probe;
  std::unique_ptr<re4::RoomFlow> flow;
  std::unique_ptr<re4::R100Scenario> r100Script;
  std::unique_ptr<re4::R101Scenario> villageScript;
  std::unique_ptr<re4::VillageBinoculars> binocular;
  std::unique_ptr<re4::VillageBottles> villageBottlesOwner;
  std::unique_ptr<re4::EnemySpawns> villageEsl;
  std::array<std::unique_ptr<re4::OriginalEvent>,3> villageEvd;
  std::shared_ptr<const re4::SoundStream> combatStream;
  std::array<std::unique_ptr<re4::R100RoomCamera>,2> villageCamera;
  std::optional<unsigned> villageCut;
  std::array<std::shared_ptr<const re4::SoundStream>,2> villageWaveVoices;
  std::vector<unsigned> villageWave;
  std::unique_ptr<re4::RadioIntro> villageRadio;
  std::shared_ptr<const re4::SoundStream> villageIntro12;
  std::optional<re4::VillageEvent> activeVillageEvent;
  std::optional<re4::CollisionVec> villageReturn;
  float villageHeading{};
  unsigned entryTicks{};
  std::unique_ptr<re4::FootstepSounds> footsteps;
  std::unique_ptr<re4::RoomLadders> ladders;
  std::vector<int> placements;
  int selected = -1;
  // A swap keeps the previous room and borrowed SAT lifetimes intact until
  // destination placement and GPU upload have succeeded.
  void swapActive() {
    if (replacePlayer)
      player.swap(leon);
    smd.swap(previewSmd);
    common.swap(previewCommon);
    smx.swap(previewSmx);
    environment.swap(previewEnvironment);
    lighting.swap(previewLighting);
    shadows.swap(previewShadows);
    worldEffects.swap(previewWorldEffects);
    areaData.swap(roomAreaEffectsData);areas.swap(roomAreaEffects);
    feedbackData.swap(enemyEffectsData);feedback.swap(enemyEffects);
    std::swap(feedbackOwner,enemyEffectsOwner);std::swap(feedbackVoices,enemyVoiceHandles);
    groupVoices.swap(groupVoiceHandles);
    fragments.swap(destructionData);
    acoustics.swap(roomAcoustics);
    ambient.swap(roomAmbient);rescueStream.swap(dogRescueStream);
    objects.swap(previewObjects);
    blocks.swap(previewBlocks);
    sat.swap(previewSat);
    eat.swap(previewEat);
    world.swap(collisionWorld);
    effect.swap(effectWorld);
    scenario.swap(scenarioCollision);
    destructionSimulation.swap(destruction);
    actions.swap(objectActions);
    interactions.swap(roomInteractions);
    items.swap(previewItems);
    looseItems.swap(previewLooseItems);itemArchive.swap(pickupArchive);
    pickups.swap(roomPickups);itemEvents.swap(roomItemEvents);roomItemEventClock=0;
    itemEffectsOwner.swap(itemEffects);
    enemy.swap(encounter);
    routes.swap(enemyRoutes);
    surfaceHits.swap(roomHitEffects);
    firstEnemy.swap(preparedFirstEnemy);group.swap(enemyGroup);
    firstEvent.swap(r100FirstEvent);truckEvent.swap(r100TruckEvent);
    villageBottlesOwner.swap(villageBottles);villageScript.swap(villageScenario);villageEsl.swap(villageSpawns);villageEvd.swap(villageEvents);
    binocular.swap(villageBinocular);binocularUiActive.store(villageBinocular!=nullptr);
    villageCamera.swap(villageCameras);villageCut.swap(villageWaveCamera);villageWaveVoices.swap(villageWaveStreams);villageWave.swap(villageWaveActors);
    villageRadio.swap(villageRadioIntro);villageIntro12.swap(villageRadio12Intro);
    combatStream.swap(villageCombatStream);activeVillageEvent.swap(villageEvent);
    villageReturn.swap(villageReturnPosition);std::swap(villageHeading,villageReturnHeading);std::swap(entryTicks,roomEntryTicks);
    opening.swap(openingEvents);
    secondary.swap(secondaryEvents);secondaryScript.swap(r100Secondary);houseEnemy.swap(r100HouseEnemy);roomMessage.swap(r100RoomMessage);
    std::swap(messageConfirm,r100MessageConfirm);
    messageUiActive.store(r100RoomMessage && !r100RoomMessage->finished());
    houseStream.swap(r100HouseStream);roomStream.swap(r100RoomStream);skullStream.swap(r100SkullStream);
    radioData.swap(r100RadioData);radioTerm.swap(r100RadioTerm);radioIntroStream.swap(r100RadioIntroStream);
    radioQueued.swap(r100RadioQueued);std::swap(radioCall,r100RadioCall);
    radioReturnPosition.swap(r100RadioReturnPosition);std::swap(radioReturnHeading,r100RadioReturnHeading);
    radioFadeFrame.swap(r100RadioFadeFrame);
    radioControls.swap(r100RadioControls);
    returnPosition.swap(secondaryReturnPosition);std::swap(returnHeading,secondaryReturnHeading);
    std::swap(event,activeEvent);std::swap(eventPhase,r100EventPhase);
    eventUiActive.store(activeEvent!=nullptr || r100EventPhase>=10);
    std::swap(eventClock,eventAccumulator);std::swap(deathPending,r100DeathPending);
    if(enemyGroup) {enemyGroup->setObjectActions(objectActions.get());enemyGroup->setRoomInteractions(roomInteractions.get());enemyGroup->setExternalTarget([](const re4::CollisionVec &from,const re4::CollisionVec &to,unsigned weapon) {
      std::optional<re4::AuthoredHitResult> nearest;
      if(forestAnimals)nearest=forestAnimals->target(from,to,weapon);
      if(villageAnimals)if(const auto hit=villageAnimals->target(from,to,weapon,nearest?nearest->lenSq:1e16f))nearest=hit->hit;
      if(dogTraps)if(const auto hit=dogTraps->target(from,to,weapon,nearest?nearest->lenSq:1e16f))nearest=hit->hit;
      return nearest;
    });}
    if(group) {group->setObjectActions(actions.get());group->setRoomInteractions(interactions.get());}
    if (encounter) encounter->setObjectActions(objectActions.get());
    if (enemy) enemy->setObjectActions(actions.get());
    animals.swap(ambientAnimals);
    forest.swap(forestAnimals);traps.swap(dogTraps);trapVoices.swap(dogTrapVoiceHandles);
    villageAnimalsOwner.swap(villageAnimals);villageAnimalVoiceOwners.swap(villageAnimalVoices);
    trapData.swap(trapEffectsData);trapSimulation.swap(trapEffects);trapParents.swap(trapEffectParents);
    probe.swap(walkProbe);
    flow.swap(roomFlow);
    r100Script.swap(r100Scenario);
    footsteps.swap(roomFootsteps);
    ladders.swap(roomLadders);
    placements.swap(previewPlacements);
    std::swap(selected, selectedPlacement);
  }
};
std::vector<re4::ItemEffectSource> itemVisualSources(const re4::ObjectActions *objects,
                                                   const re4::RoomPickups *pickups) {
  auto sources=objects?objects->effectSources():std::vector<re4::ItemEffectSource>{};
  if(pickups) {const auto loose=pickups->effectSources();sources.insert(sources.end(),loose.begin(),loose.end());}
  return sources;
}
void syncItemEffects() {
  if(itemEffects)itemEffects->sync(itemVisualSources(objectActions.get(),roomPickups.get()),collisionWorld.get());
}
std::vector<std::shared_ptr<const re4::SoundBank>> readAudio(const std::string &path) {
  if (path.empty()) return {};
  try {
    auto banks = re4::SoundBank::LoadDvd(path);
    for (const auto &bank : banks)
      __android_log_print(ANDROID_LOG_INFO, "RE4Audio", "%s · %s",
                          path.substr(path.find_last_of('/') + 1).c_str(),
                          bank->describe().c_str());
    return banks;
  } catch (const std::exception &e) {
    __android_log_print(ANDROID_LOG_WARN, "RE4Audio", "%s", e.what());
    return {};
  }
}
void installStaticAudio() {
  const auto profile = coreAssetPath + '\n' + playerAssetPath + '\n' + weaponAssetPath + '\n' + enemyAssetPath;
  if (profile == staticAudioProfile) return;
  soundMixer.clear();
  std::vector<std::shared_ptr<const re4::SoundBank>> banks;
  for (const auto &path : {coreAssetPath, playerAssetPath, weaponAssetPath, enemyAssetPath}) {
    auto source = readAudio(path);
    banks.insert(banks.end(), source.begin(), source.end());
  }
  if(!playerAssetPath.empty()) {
    const auto emDir=playerAssetPath.substr(0,playerAssetPath.find_last_of('/'));
    for(const auto &name:{"em21.drs","em23.drs","em2a.drs"}) {
      const auto source=readAudio(emDir+"/"+name);banks.insert(banks.end(),source.begin(),source.end());
    }
  }
  soundMixer.installBanks(banks);
  staticAudioProfile = profile;
  pyreVoice = 0;
}
std::vector<std::shared_ptr<const re4::SoundBank>> readRoomAudio(std::uint16_t room) {
  // Decoded geometry omits DVD audio: read the imported original disc file.
  const auto marker = playerAssetPath.rfind("/disc1/files/em/pl00.drs");
  if (marker == std::string::npos) return {};
  const auto game = playerAssetPath.substr(0, marker);
  for (unsigned disc : {1u, 2u}) {
    const auto path = game + "/disc" + std::to_string(disc) + "/files/St" +
                      std::to_string(room >> 8) + "/" + roomName(room) + ".das";
    if (std::ifstream(path, std::ios::binary)) return readAudio(path);
  }
  __android_log_print(ANDROID_LOG_WARN, "RE4Audio", "%s: Original DVD audio is missing",
                      roomName(room).c_str());
  return {};
}
void installWeaponAudio() {
  if (!leon || !leon->weapon() || !leon->weaponAttackImplemented()) return;
  const auto &path = leon->weapon()->sourcePath();
  if (!path.empty()) soundMixer.installBanks(readAudio(path));
  lastSoundAction = re4::PlayerAction::Footwork;
  lastWeaponSoundMotion = re4::WeaponMotion::Idle;
  lastWeaponSoundFrame = 0;
}
void commitRoomAudio(const PreparedRoom &room,bool restarting=false) {
  for(auto handle:room.feedbackVoices)soundMixer.stop(handle);
  for(const auto &owner:room.groupVoices)for(auto handle:owner.second)soundMixer.stop(handle);
  for(const auto &voice:room.trapVoices)soundMixer.stop(voice.second);
  soundMixer.installBanks(room.sounds, true);
  if(restarting)soundMixer.requestStream(0,0,4,0,0);
  for (unsigned slot = 0; slot < 2; ++slot) soundMixer.installSequenceSlot(slot,room.music[slot],restarting);
  soundMixer.installStream(room.stream,restarting);
  soundMixer.installStreamAssets(room.addressedStreams,true);
  eventMixer.installStreamAssets(room.addressedStreams,true);
  pyreVoice = previewWorldEffects && previewWorldEffects->originalPyre
                  ? playRoomSound(6, 0x58, {-2880, 1550, 2810}) : 0;
  lastSoundAction = re4::PlayerAction::Footwork;
}
void storeInteractionState() {
  if (!roomInteractions) return;
  auto &saved = scenarioState.room(roomInteractions->room());
  // Interior routines mutate ETC words only. Campaign scripts own RSF and
  // pickups own item words; neither may be replaced by the room-load copy.
  saved.etc = roomInteractions->state().etc;
}
PreparedRoom prepareRoom(const std::string &path, std::uint16_t room,bool keepPrevious=true) {
  NativeLoadClock loadClock("Room data and events",room);
  if(keepPrevious) {
  if (objectActions)
    objectRoomStates[objectActions->room()] = objectActions->snapshot();
  storeInteractionState();
  if(roomPickups)runtimeRoomDrops[activeRoomId]=room==activeRoomId?
      roomPickups->runtimeDrops():roomPickups->persistentRuntimeDrops();
  if(forestAnimals)for(const auto id:forestAnimals->deadIndices())r100DeadIndices.insert(id);
  if(villageAnimals)for(const auto id:villageAnimals->deadIndices())r100DeadIndices.insert(id);
  }
  if (testProfile == 1 && room == 0x101)
    scenarioState.room(room).room.set(8); // R101Init: house ladders are visible after RSF8.
  // R400Init::SetBreakModel opens this window before the first game tick.
  // Its replacement mesh, zero HP and removed SAT share the same ETC bit.
  if (room == 0x400)
    scenarioState.room(room).setObject(0, 1);
  if(campaignMode && room==0x10e && (activeRoomId==0x119 || originalSaveMetadata.previousRoom==0x119))scenarioState.scenario.set(39);
  if(campaignMode && re4::IsVillageRoom(room)) {
    const unsigned oldList=unsigned(originalSaveMetadata.enemyListNo);
    const unsigned nextList=re4::VillageEnemyList(room,scenarioState.scenario.test(39),oldList);
    if(nextList!=oldList) {
      if(!originalSaveMetadata.enemyDeaths)originalSaveMetadata.enemyDeaths.emplace();
      for(unsigned index:r100DeadIndices)if(index<255)re4::SetOriginalSaveEnemyDead(*originalSaveMetadata.enemyDeaths,oldList,index);
      for(unsigned index:r100UsedTraps)if(index<255)re4::SetOriginalSaveEnemyDead(*originalSaveMetadata.enemyDeaths,oldList,index);
      r100DeadIndices.clear();r100DroppedIndices.clear();r100UsedTraps.clear();
      for(unsigned index=0;index<255;++index)if(re4::OriginalSaveEnemyDead(*originalSaveMetadata.enemyDeaths,nextList,index)) {
        r100DeadIndices.insert(index);r100DroppedIndices.insert(index);r100UsedTraps.insert(index);
      }
    }
    originalSaveMetadata.enemyListNo=int(nextList);
  }
  auto arc = re4::RoomArchive::Load(path);
  std::shared_ptr<const re4::SoundStream> deathStream;
  if(campaignMode) {
    deathStream=streamCache[0].lock();
    if(!deathStream) {
      const auto files=playerAssetPath.substr(0,playerAssetPath.find_last_of('/'))+"/..";
      deathStream=re4::SoundStream::Load(files+"/bgm/bio4str.hed",files+"/bgm/bio4bgm.sbb",0,0);
      streamCache[0]=deathStream;
    }
  }
  auto copy = [&](const re4::ArchiveEntry *entry) {
    return arc.copy(static_cast<std::size_t>(entry - arc.entries().data()));
  };
  const auto *entry = arc.find("SMD");
  if (!entry)
    throw std::runtime_error("No SMD in target room");
  PreparedRoom next;
  next.shadows=re4::ReadRoomShadows(arc);
  if(deathStream)next.addressedStreams.push_back(deathStream);
  next.sounds = readRoomAudio(room);
  if(arc.find("LIT")) {
    next.lighting=re4::ReadRoomLighting(arc);
    __android_log_print(ANDROID_LOG_INFO,"RE4Lighting","%s · %s",roomName(room).c_str(),next.lighting->describe().c_str());
  }
  next.acoustics = std::make_unique<re4::SoundAcoustics>(arc);
  next.footsteps=std::make_unique<re4::FootstepSounds>(arc);
  try {
    next.ambient = std::make_unique<re4::SoundAmbient>(arc);
    if (!musicAssetRoot.empty()) {
      const auto selection = re4::ReadRoomMusicSelection(musicAssetRoot + "/bgmtbl.dat",room);
      for (unsigned slot = 0; slot < 2; ++slot) {
        const auto code = (selection.bgm[0] >> (slot*16)) & 0xffff;
        if (code == 0xffff || (code & 0xc000) != 0xc000) continue;
        if (!musicCache.count(code)) musicCache[code] = re4::SoundSequence::Load(
            musicAssetRoot + "/bio4midi.hed",musicAssetRoot + "/bio4midi.dat",code & 255,(code >> 8) & 3);
        next.music[slot] = musicCache.at(code);
        __android_log_print(ANDROID_LOG_INFO,"RE4Audio","%s · BGM%u · %s",roomName(room).c_str(),slot,next.music[slot]->describe().c_str());
      }
      // SndRoomStrStartCheck starts only resident+automatic streams. In r101
      // 0x8002 is resident combat music controlled by encounter scripts and must
      // remain silent in the enemy-free village.
      const auto streamCode = selection.streams[0] & 0xffff;
      if (streamCode != 0xffff && (streamCode & 0xc000) == 0xc000) {
        next.stream = streamCache[streamCode].lock();
        if (!next.stream) {
          next.stream = re4::SoundStream::Load(musicAssetRoot + "/bio4str.hed",
              musicAssetRoot + "/bio4bgm.sbb",0,streamCode & 255);
          streamCache[streamCode] = next.stream;
        }
        __android_log_print(ANDROID_LOG_INFO,"RE4Audio","%s · Stream · %s",roomName(room).c_str(),next.stream->describe().c_str());
      }
    }
  } catch (const std::exception &e) {
    __android_log_print(ANDROID_LOG_WARN,"RE4Audio","%s · Music/Ambience: %s",roomName(room).c_str(),e.what());
  }
  next.smd = std::make_unique<re4::Smd>(copy(entry));
  if (const auto *common = arc.find("SMD", 1))
    next.common = std::make_unique<re4::Smd>(copy(common),
                                             next.smd->commonRequirements());
  if (const auto *smx = arc.find("SMX"))
    next.smx = std::make_unique<re4::Smx>(copy(smx));
  if (room == 0x100) {
    next.blocks = std::make_unique<re4::RoomBlockLayer>(
        re4::BuildRoomBlocks(arc, next.smx.get(), path, room));
    __android_log_print(ANDROID_LOG_INFO, "RE4Blocks", "%s · %s",
                        roomName(room).c_str(), next.blocks->describe().c_str());
  }
  if (const auto *sat = arc.find("SAT")) {
    next.sat = std::make_unique<re4::Sat>(copy(sat));
    next.world = std::make_unique<re4::SatCollision>(*next.sat);
    re4::WalkConfig walk;
    walk.originalScenerySphere = true;
    walk.stepUp = 600; walk.drop = 999;
    next.probe = std::make_unique<re4::WalkProbe>(*next.world, walk);
  }
  if (const auto *eat = arc.find("EAT")) {
    next.eat = std::make_unique<re4::Sat>(copy(eat));
    next.effect = std::make_unique<re4::SatCollision>(
        *next.eat, re4::SatQueryMode::Effect);
  }
  if (const auto *aev = arc.find("AEV")) {
    next.scenario = std::make_unique<re4::ScenarioCollision>(room,copy(aev));
    next.scenario->sync(static_cast<const re4::ScenarioState &>(scenarioState).room(room),
                        next.world.get(),next.effect.get());
    __android_log_print(ANDROID_LOG_INFO,"RE4Collision",
        "%s · Original scenario collision areas: %zu · Parent-attached areas: %u",
        roomName(room).c_str(),next.scenario->pieces().size(),next.scenario->parentedCount());
  }
  if (const auto *aev = arc.find("AEV")) {
    try {
      next.flow = std::make_unique<re4::RoomFlow>(room, copy(aev));
      if(room==0x100)next.r100Script=std::make_unique<re4::R100Scenario>(copy(aev));
      next.ladders = std::make_unique<re4::RoomLadders>(*next.flow);
      if (next.ladders->count())
        __android_log_print(ANDROID_LOG_INFO, "RE4Ladder", "%s · %s",
                            roomName(room).c_str(), next.ladders->describe().c_str());
    } catch (const std::exception &e) {
      __android_log_print(ANDROID_LOG_WARN, "RE4Flow", "%s: %s",
                          roomName(room).c_str(), e.what());
    }
  }
  if (next.smx) {
    next.environment =
        std::make_unique<re4::EnvironmentData>(re4::BuildRoomEnvironment(
            arc, *next.smd, *next.smx, room, coreEnvironmentEff));
    __android_log_print(ANDROID_LOG_INFO, "RE4Environment", "%s · %s",
                        roomName(room).c_str(),
                        next.environment->describe().c_str());
    for (const auto &warning : next.environment->warnings)
      __android_log_print(ANDROID_LOG_WARN, "RE4Environment", "%s · %s",
                          roomName(room).c_str(), warning.c_str());
  }
  next.worldEffects = std::make_unique<re4::WorldEffectsData>(
      re4::BuildRoomWorldEffects(arc, room, coreEnvironmentEff));
  __android_log_print(ANDROID_LOG_INFO, "RE4WorldEffects", "%s · %s",
                      roomName(room).c_str(), next.worldEffects->describe().c_str());
  for (const auto &warning : next.worldEffects->sprites.warnings)
    __android_log_print(ANDROID_LOG_WARN, "RE4WorldEffects", "%s", warning.c_str());
  if (arc.find("ETS") && arc.find("ETM")) {
    const auto &saved = static_cast<const re4::ScenarioState &>(scenarioState).room(room);
    next.objects = std::make_unique<re4::RoomObjectLayer>(
        re4::BuildRoomObjects(arc, coreEnvironmentEff, room, &saved));
    __android_log_print(ANDROID_LOG_INFO, "RE4Objects", "%s · %s",
                        roomName(room).c_str(), next.objects->describe().c_str());
    for (const auto &warning : next.objects->warnings)
      __android_log_print(ANDROID_LOG_WARN, "RE4Objects", "%s", warning.c_str());
  }
  const bool supportedInterior = next.objects && std::any_of(
      next.objects->placements.begin(),next.objects->placements.end(),
      [](const auto &p) { return p.id==0 || p.id==7 || p.id==0x25 || p.id==3 || p.id==0xd; });
  if (supportedInterior && next.probe && !playerAssetPath.empty()) {
    try {
    next.interactions = std::make_unique<re4::RoomInteractions>(
        room, arc, next.objects->placements, playerAssetPath,
        static_cast<const re4::ScenarioState &>(scenarioState).room(room), next.effect.get());
    next.probe->setObjectCollision(next.interactions->collision());
    __android_log_print(ANDROID_LOG_INFO, "RE4Interaction", "%s",
                        next.interactions->describe().c_str());
    } catch (const std::exception &e) {
      next.interactions.reset();
      __android_log_print(ANDROID_LOG_WARN,"RE4Interaction","%s · Unsupported original path: %s",roomName(room).c_str(),e.what());
    }
  }
  if (next.flow && next.objects && next.probe && !playerAssetPath.empty()) {
    next.ladders = std::make_unique<re4::RoomLadders>(*next.flow, arc,
        next.objects->placements,
        static_cast<const re4::ScenarioState &>(scenarioState).room(room), playerAssetPath);
    if (next.interactions) {
      next.interactions->setLadders(next.ladders.get());
      next.ladders->setInteractions(next.interactions.get());
      next.interactions->setSceneryBodies(next.ladders->objectBodies(), next.probe.get());
    } else if (!next.ladders->objectBodies().empty()) {
      next.probe->setObjectCollision(next.ladders->collision());
      next.probe->setObjectBodies(next.ladders->objectBodies());
    }
    __android_log_print(ANDROID_LOG_INFO, "RE4Ladder", "%s · %s", roomName(room).c_str(), next.ladders->describe().c_str());
  }
  if (next.objects) {
    try {
    next.actions = std::make_unique<re4::ObjectActions>(
        room, arc, next.objects->placements, next.effect.get(), &scenarioState);
    if (const auto at = objectRoomStates.find(room); at != objectRoomStates.end())
      next.actions->restore(at->second);
    next.items = std::make_unique<re4::RoomItemLayer>(
        re4::BuildRoomItems(arc, *next.actions));
    __android_log_print(ANDROID_LOG_INFO, "RE4Items", "%s · %s · %s",
                        roomName(room).c_str(), next.actions->describe().c_str(),
                        next.items->describe().c_str());
    if (!next.actions->objects().empty()) {
      next.fragments = std::make_unique<re4::DestructionEffectsData>(
        re4::BuildDestructionEffects(arc, coreEnvironmentEff, room));
    next.destructionSimulation = std::make_unique<re4::DestructionEffectsSimulation>(
        *next.fragments, next.effect.get(), next.world.get());
    const auto &states = next.actions->states();
    const auto &objects = next.actions->objects();
    for (std::size_t i = 0; i < states.size(); ++i)
      if (states[i].broken)
        next.destructionSimulation->restoreBroken(
            objects[i].placement.id, objects[i].boxType, states[i].slot,
            objects[i].placement.position, objects[i].placement.rotation);
    __android_log_print(ANDROID_LOG_INFO, "RE4Destruction", "%s · %s",
                        roomName(room).c_str(), next.fragments->describe().c_str());
    for (const auto &warning : next.fragments->warnings)
      __android_log_print(ANDROID_LOG_WARN, "RE4Destruction", "%s", warning.c_str());
    }
    } catch (const std::exception &e) {
      next.actions.reset();next.items.reset();next.fragments.reset();next.destructionSimulation.reset();
      __android_log_print(ANDROID_LOG_WARN,"RE4Items","%s · Unsupported container path: %s",roomName(room).c_str(),e.what());
    }
  }
  if (arc.find("ITA")) {
    try {
    next.itemArchive=std::make_shared<const re4::RoomArchive>(arc);
    next.pickups = std::make_unique<re4::RoomPickups>(room, arc, scenarioState, next.effect.get());
    if(const auto saved=runtimeRoomDrops.find(room);saved!=runtimeRoomDrops.end())next.pickups->restoreRuntimeDrops(saved->second);
    next.looseItems = std::make_unique<re4::RoomItemLayer>(
        re4::BuildRoomItems(arc, next.pickups->renderPlacements()));
    for (auto &draw : next.looseItems->mesh.draws) {
      draw.roomPickupIndex = draw.roomItemIndex;
      draw.roomItemIndex = -1;
      if (draw.roomItemId == 0xaf) draw.orderType = 1;
    }
    __android_log_print(ANDROID_LOG_INFO, "RE4Items", "%s · %s",roomName(room).c_str(), next.pickups->describe().c_str());
    } catch (const std::exception &e) {
      next.pickups.reset();next.looseItems.reset();
      __android_log_print(ANDROID_LOG_WARN,"RE4Items","%s · Unsupported pickup path: %s",roomName(room).c_str(),e.what());
    }
  }
  if(room==0x103 || room==0x113 || room==0x106) {
    if(!next.pickups)throw std::runtime_error("Item event: room pickup owner missing");
    next.itemEvents=std::make_unique<re4::RoomItemEvents>(room,arc,*next.smd,scenarioState,*next.pickups);
  }
  if (next.interactions && !next.interactions->windows().empty() && !next.fragments) {
    try {
      next.fragments = std::make_unique<re4::DestructionEffectsData>(re4::BuildDestructionEffects(arc,coreEnvironmentEff,room));
      next.destructionSimulation = std::make_unique<re4::DestructionEffectsSimulation>(*next.fragments,next.effect.get(),next.world.get());
    } catch (const std::exception &e) {
      next.fragments.reset(); next.destructionSimulation.reset();
      __android_log_print(ANDROID_LOG_WARN,"RE4Effects","%s · Unsupported debris resource: %s",roomName(room).c_str(),e.what());
    }
  }
  if((next.pickups || next.actions) && !coreEnvironmentEff.empty()) {
    next.itemEffectsOwner=std::make_unique<re4::ItemEffects>(coreEnvironmentEff);
    next.itemEffectsOwner->sync(itemVisualSources(next.actions.get(),next.pickups.get()),next.world.get());
  }
  if (room == 0x400 && next.world && !chickenAssetPath.empty() && !chickenSpawnPath.empty()) {
    std::ifstream file(chickenSpawnPath, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() != re4::EnemySpawns::FileBytes)
      throw std::runtime_error("r400: omake01.esl is missing or invalid");
    std::vector<std::uint8_t> bytes(re4::EnemySpawns::FileBytes);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(bytes.data()), bytes.size()))
      throw std::runtime_error("r400: omake01.esl is truncated");
    next.animals = std::make_unique<re4::AmbientAnimals>(*next.world, bytes, chickenAssetPath);
    __android_log_print(ANDROID_LOG_INFO, "RE4Animals", "%s", next.animals->describe().c_str());
  }
  if(campaignMode && (room==0x120 || (room==0x100 && !scenarioState.room(room).room.test(13))) &&
      !re4::OriginalOpeningCanceled(scenarioState)) {
    const auto files=playerAssetPath.substr(0,playerAssetPath.find_last_of('/'))+"/..";
    if(room==0x120) {
      next.opening[0]=std::make_unique<re4::OriginalEvent>(files,"r120s00");
      next.opening[1]=std::make_unique<re4::OriginalEvent>(files,"r120s01");
    } else next.opening[2]=std::make_unique<re4::OriginalEvent>(files,"r100s40");
    for(auto &event:next.opening)if(event)event->prepareRoomEffectModels(*next.smd,next.common.get());
  }
  if(campaignMode && room==0x101 && next.world && next.effect) {
    const auto files=playerAssetPath.substr(0,playerAssetPath.find_last_of('/'))+"/..";
    const auto *aev=arc.find("AEV");if(!aev)throw std::runtime_error("r101: AEV missing");
    next.villageScript=std::make_unique<re4::R101Scenario>(copy(aev));
    std::ifstream source(enemySpawnPath,std::ios::binary|std::ios::ate);
    if(!source || source.tellg()!=re4::EnemySpawns::FileBytes)throw std::runtime_error("r101: Original ESL missing");
    std::vector<std::uint8_t> bytes(re4::EnemySpawns::FileBytes);source.seekg(0);
    if(!source.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))throw std::runtime_error("r101: Truncated ESL");
    next.villageBottlesOwner=std::make_unique<re4::VillageBottles>(arc,coreEnvironmentEff);
    next.villageEsl=std::make_unique<re4::EnemySpawns>(bytes);
    next.villageAnimalsOwner=std::make_unique<re4::VillageAnimals>(*next.world,*next.villageEsl,files+"/em");
    next.villageAnimalsOwner->restoreDead(std::vector<unsigned>(r100DeadIndices.begin(),r100DeadIndices.end()));
    next.villageAnimalsOwner->setAuthoredDrops(arc,scenarioState);
    next.villageAnimalsOwner->setDropState(&enemyDropState);
    for(const auto &name:{"em26.drs","em28.drs"}) {
      const auto banks=readAudio(files+"/em/"+name);next.sounds.insert(next.sounds.end(),banks.begin(),banks.end());
    }
    const auto active=next.villageScript->initialSpawns(*next.villageEsl,scenarioState,r100DeadIndices);
    const auto enemy=files+"/em/em15.drs";
    next.group=std::make_unique<re4::EnemyGroup>(*next.world,*next.effect,enemy);
    next.group->setDifficulty(campaignDifficulty==1?2:campaignDifficulty==2?10:5,campaignDifficulty==2);
    next.group->setObjectActions(next.actions.get());next.group->setRoomInteractions(next.interactions.get());next.group->setRoutes(next.routes);
    for(auto index:re4::R101Scenario::actorIndices()) {
      auto &actor=next.group->spawn(*next.villageEsl->find(index));
      const bool live=std::find(active.begin(),active.end(),index)!=active.end();
      actor.setVisible(live);actor.setScriptSuspended(!live);
    }
    next.villageScript->resume(scenarioState,next.group->villageAlive());
    if(scenarioState.room(room).room.test(6) && !scenarioState.room(room).room.test(7))next.group->releaseVillageAlarm();
    const auto *cam=arc.find("CAM");if(!cam)throw std::runtime_error("r101: CAM missing");
    for(unsigned i=0;i<2;++i) {
      next.villageCamera[i]=std::make_unique<re4::R100RoomCamera>(copy(cam),4+i);
      next.villageWaveVoices[i]=re4::SoundStream::Load(files+"/bgm/bio4str.hed",files+"/bgm/bio4evt.sbb",1,i?0x3e:0x3b);
      next.addressedStreams.push_back(next.villageWaveVoices[i]);
    }
    next.villageRadio=std::make_unique<re4::RadioIntro>(playerAssetPath);
    for(unsigned call:{2u,12u}) {
      next.radioData[call]=std::make_shared<re4::RadioTermData>(files,call,1);next.addressedStreams.push_back(next.radioData[call]->voice());
    }
    next.radioIntroStream=re4::SoundStream::Load(files+"/bgm/bio4str.hed",files+"/bgm/bio4evt.sbb",1,3);
    next.villageIntro12=re4::SoundStream::Load(files+"/bgm/bio4str.hed",files+"/bgm/bio4evt.sbb",1,0x33);
    next.addressedStreams.push_back(next.radioIntroStream);next.addressedStreams.push_back(next.villageIntro12);
    const char *names[]={"r101s00","r101s21","r101s30"};
    for(unsigned i=0;i<3;++i) {
      next.villageEvd[i]=std::make_unique<re4::OriginalEvent>(files,names[i],1);
      next.villageEvd[i]->prepareRoomEffectModels(*next.smd,next.common.get());
    }
    next.combatStream=re4::SoundStream::Load(files+"/bgm/bio4str.hed",files+"/bgm/bio4bgm.sbb",0,2);
    next.addressedStreams.push_back(next.combatStream);
    if(scenarioState.room(room).room.test(6) && !scenarioState.room(room).room.test(7))next.stream=next.combatStream;
    const auto banks=readAudio(enemy);next.sounds.insert(next.sounds.end(),banks.begin(),banks.end());
    const auto enemyArchive=re4::RoomArchive::LoadDrs(enemy);
    if(const auto *eff=enemyArchive.find("EFF")) {
      next.feedbackData=std::make_unique<re4::WorldEffectsData>(re4::BuildEnemyWorldEffects(enemyArchive.copy(eff-enemyArchive.entries().data()),coreEnvironmentEff));
      next.feedback=std::make_unique<re4::WorldEffectsSimulation>(*next.feedbackData);
    }
    if(next.interactions)next.interactions->syncVillageState(scenarioState.room(room));
  }
  if(campaignMode && room==0x100 && next.r100Script && next.world && next.effect && !enemyAssetPath.empty()) {
    const auto *routeData=arc.find("RTP");
    if(!routeData)throw std::runtime_error("r100: Original enemy routes missing");
    next.routes=std::make_shared<const re4::EnemyRoutes>(copy(routeData),*next.world);
    const auto profile=re4::R100Profile(scenarioState.room(room));
    const auto files=playerAssetPath.substr(0,playerAssetPath.find_last_of('/'))+"/..";
    if(!profile.afterTruck) {
      auto first=std::make_unique<re4::EnemyEncounter>(*next.world,*next.effect,
          re4::R100FirstGanadoSpawn(),enemyAssetPath,true);
      first->setDifficulty(campaignDifficulty==1?2:campaignDifficulty==2?10:5,campaignDifficulty==2);
      first->setObjectActions(next.actions.get());
      first->setRoutes(next.routes);
      if(profile.firstActive)next.enemy=std::move(first);
      else next.firstEnemy=std::move(first);
      if(!profile.firstActive)next.firstEvent=std::make_unique<re4::OriginalEvent>(files,"r100s03");
      next.truckEvent=std::make_unique<re4::OriginalEvent>(files,"r100s20");
    }
    std::ifstream file(enemySpawnPath,std::ios::binary|std::ios::ate);
    if(!file || file.tellg()!=re4::EnemySpawns::FileBytes)throw std::runtime_error("r100: Original ESL missing");
    std::vector<std::uint8_t> bytes(re4::EnemySpawns::FileBytes);file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))throw std::runtime_error("r100: Truncated ESL");
    const re4::EnemySpawns spawns(bytes);
    const auto emDir=playerAssetPath.substr(0,playerAssetPath.find_last_of('/'));
    next.secondaryScript=std::make_unique<re4::R100Secondary>(arc,enemyAssetPath,spawns,scenarioState.room(room),1);
    next.secondaryScript->prepareMessages(files,subscreenLanguage);
    for(unsigned call=0;call<2;++call) {
      next.radioData[call]=std::make_shared<re4::RadioTermData>(files,call,subscreenLanguage);
      next.addressedStreams.push_back(next.radioData[call]->voice());
    }
    next.radioIntroStream=re4::SoundStream::Load(files+"/bgm/bio4str.hed",files+"/bgm/bio4evt.sbb",1,3);
    next.addressedStreams.push_back(next.radioIntroStream);
    const char *optionalNames[]{"r100s41","r100s43","r100s44","r100s30"};
    for(unsigned i=0;i<4;++i)if(!profile.afterTruck || i==3) {
      next.secondary[i]=std::make_unique<re4::OriginalEvent>(files,optionalNames[i],subscreenLanguage);
      next.secondary[i]->prepareRoomEffectModels(*next.smd,next.common.get());
    }
    if(!scenarioState.room(room).room.test(15)) {
      next.houseEnemy=std::make_unique<re4::EnemyEncounter>(*next.world,*next.effect,next.secondaryScript->houseSpawn(),enemyAssetPath,true);
      next.houseEnemy->setRoutes(next.routes);
      next.houseStream=re4::SoundStream::Load(files+"/bgm/bio4str.hed",files+"/bgm/bio4evt.sbb",1,0x22);
    }
    next.skullStream=re4::SoundStream::Load(files+"/bgm/bio4str.hed",files+"/bgm/bio4bgm.sbb",0,0x12);
    const auto roomMusic=re4::ReadRoomMusicSelection(files+"/bgm/bgmtbl.dat",room);
    const auto roomStreamCode=roomMusic.streams[0]&0xffff;
    if(roomStreamCode!=0xffff && (roomStreamCode&0x8000))next.roomStream=re4::SoundStream::Load(
        files+"/bgm/bio4str.hed",files+"/bgm/bio4bgm.sbb",0,roomStreamCode&255);
    if(next.roomStream)next.addressedStreams.push_back(next.roomStream);
    next.addressedStreams.push_back(next.skullStream);
    for(unsigned stream:{4u,5u,6u})next.addressedStreams.push_back(re4::SoundStream::Load(
        files+"/bgm/bio4str.hed",files+"/bgm/bio4evt.sbb",1,stream));
    next.forest=std::make_unique<re4::ForestAnimals>(*next.world,bytes,emDir+"/em23.drs",room);
    next.forest->setAuthoredDrops(arc,scenarioState);
    next.forest->setDropState(&enemyDropState);
    next.forest->restoreDead(std::vector<unsigned>(r100DeadIndices.begin(),r100DeadIndices.end()));
    next.traps=std::make_unique<re4::DogTraps>(*next.world,spawns,emDir+"/em21.drs",emDir+"/em2a.drs",scenarioState,&r100UsedTraps);
    if(next.player)next.traps->installPlayerMotions(*next.player);
    else if(leon)next.traps->installPlayerMotions(*leon);
    next.rescueStream=re4::SoundStream::Load(files+"/bgm/bio4str.hed",files+"/bgm/bio4evt.sbb",1,0x0e);
    if(profile.afterTruck)next.traps->releaseAfterTruck();
    const auto trapArchive=re4::RoomArchive::LoadDrs(emDir+"/em2a.drs");
    const auto *trapEff=trapArchive.find("EFF");
    if(!trapEff)throw std::runtime_error("r100: Original EM2A effects missing");
    const auto trapBytes=trapArchive.copy(trapEff-trapArchive.entries().data());
    next.trapData=std::make_unique<re4::WorldEffectsData>(re4::BuildTrapWorldEffects(trapBytes,coreEnvironmentEff));
    next.trapSimulation=std::make_unique<re4::WorldEffectsSimulation>(*next.trapData);
    const auto trapFragments=re4::BuildTrapDestructionEffects(trapBytes,coreEnvironmentEff);
    auto oldFragments=next.destructionSimulation?next.destructionSimulation->state():re4::DestructionEffectsState{};
    if(!next.fragments)next.fragments=std::make_unique<re4::DestructionEffectsData>(trapFragments);
    else re4::AppendDestructionData(*next.fragments,trapFragments);
    next.destructionSimulation=std::make_unique<re4::DestructionEffectsSimulation>(*next.fragments,next.effect.get(),next.world.get());
    if(oldFragments.signature) {
      oldFragments.signature=next.fragments->signature;
      if(!next.destructionSimulation->restore(oldFragments))throw std::runtime_error("r100: Invalid destruction state while extending effects");
    }

    next.group=std::make_unique<re4::EnemyGroup>(*next.world,*next.effect,enemyAssetPath);
    next.group->setDifficulty(campaignDifficulty==1?2:campaignDifficulty==2?10:5,campaignDifficulty==2);
    next.group->setObjectActions(next.actions.get());
    next.group->setRoomInteractions(next.interactions.get());
    for(unsigned index:{3u,4u,5u,6u,7u,8u,0x12u,0x13u,0x26u,0x27u}) {
      const auto *source=spawns.find(index);
      if(!source || source->room!=0x100 || !source->id)throw std::runtime_error("r100: Ambush record missing");
      if(r100DeadIndices.count(index))continue;
      auto &actor=next.group->spawn(*source);
      actor.setRoutes(next.routes);
      if(index>=6 && index<=8)actor.installR100CliffMotion(arc.copy(0x2e + index - 6 - 4));
      actor.setScriptSuspended(!profile.afterTruck);
      actor.setVisible(profile.afterTruck);
    }
    if(profile.afterTruck)next.group->releaseR100CliffActors();
    if(profile.ambushReleased)next.group->releaseR100HouseAmbush();
    const auto enemyArchive=re4::RoomArchive::LoadDrs(enemyAssetPath);
    if(const auto *effect=enemyArchive.find("EFF")) {
      next.feedbackData=std::make_unique<re4::WorldEffectsData>(re4::BuildEnemyWorldEffects(
          enemyArchive.copy(effect-enemyArchive.entries().data()),coreEnvironmentEff));
      next.feedback=std::make_unique<re4::WorldEffectsSimulation>(*next.feedbackData);
      next.feedbackOwner=next.enemy?next.enemy.get():next.firstEnemy.get();
    }
  }
  if(campaignMode && (room==0x103 || room==0x106 || room==0x111 || room==0x113 || room==0x11a) && next.world && next.effect) {
    const auto files=playerAssetPath.substr(0,playerAssetPath.find_last_of('/'))+"/..";
    const auto listPath=files+"/etc/"+(originalSaveMetadata.enemyListNo?std::string("emleon01.esl"):std::string("emleon00.esl"));
    std::ifstream source(listPath,std::ios::binary|std::ios::ate);
    if(!source || source.tellg()!=re4::EnemySpawns::FileBytes)throw std::runtime_error("Village: Original ESL missing");
    std::vector<std::uint8_t> bytes(re4::EnemySpawns::FileBytes);source.seekg(0);
    if(!source.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))throw std::runtime_error("Village: Truncated ESL");
    next.villageEsl=std::make_unique<re4::EnemySpawns>(bytes);
    if(const auto *routes=arc.find("RTP"))next.routes=std::make_shared<const re4::EnemyRoutes>(copy(routes),*next.world);
    const unsigned enemyId=(room==0x111 || room==0x113)?0x17:0x12;
    const auto enemy=files+"/em/"+(enemyId==0x17?std::string("em17.drs"):std::string("em12.drs"));
    next.group=std::make_unique<re4::EnemyGroup>(*next.world,*next.effect,enemy);
    next.group->setDifficulty(campaignDifficulty==1?2:campaignDifficulty==2?10:5,campaignDifficulty==2);
    next.group->setObjectActions(next.actions.get());next.group->setRoomInteractions(next.interactions.get());next.group->setRoutes(next.routes);
    for(const auto &spawn:next.villageEsl->records()) {
      if(spawn.room!=room || spawn.id!=enemyId || !spawn.alive() || spawn.alreadySet() || r100DeadIndices.count(unsigned(spawn.listIndex)))continue;
      next.group->spawn(spawn);
    }
    auto banks=readAudio(enemy);next.sounds.insert(next.sounds.end(),banks.begin(),banks.end());
    const auto resource=re4::RoomArchive::LoadDrs(enemy);
    if(const auto *eff=resource.find("EFF")) {
      next.feedbackData=std::make_unique<re4::WorldEffectsData>(re4::BuildEnemyWorldEffects(resource.copy(eff-resource.entries().data()),coreEnvironmentEff));
      next.feedback=std::make_unique<re4::WorldEffectsSimulation>(*next.feedbackData);
    }
    if(room==0x103) {
      next.villageAnimalsOwner=std::make_unique<re4::VillageAnimals>(*next.world,*next.villageEsl,files+"/em",room);
      next.villageAnimalsOwner->restoreDead(std::vector<unsigned>(r100DeadIndices.begin(),r100DeadIndices.end()));
      next.villageAnimalsOwner->setAuthoredDrops(arc,scenarioState);next.villageAnimalsOwner->setDropState(&enemyDropState);
      for(const auto &name:{"em26.drs","em28.drs"}) {
        auto animalBanks=readAudio(files+"/em/"+name);next.sounds.insert(next.sounds.end(),animalBanks.begin(),animalBanks.end());
      }
    }
    if(room==0x106 || room==0x111 || room==0x113) {
      next.traps=std::make_unique<re4::DogTraps>(*next.world,*next.villageEsl,files+"/em/em21.drs",files+"/em/em2a.drs",scenarioState,&r100UsedTraps,room);
      next.traps->releaseAfterTruck();
      const auto resource=re4::RoomArchive::LoadDrs(files+"/em/em2a.drs");
      const auto *eff=resource.find("EFF");if(!eff)throw std::runtime_error("Village: Trap effects missing");
      const auto data=resource.copy(eff-resource.entries().data());
      next.trapData=std::make_unique<re4::WorldEffectsData>(re4::BuildTrapWorldEffects(data,coreEnvironmentEff));
      next.trapSimulation=std::make_unique<re4::WorldEffectsSimulation>(*next.trapData);
      const auto fragmentData=re4::BuildTrapDestructionEffects(data,coreEnvironmentEff,room);
      auto saved=next.destructionSimulation?next.destructionSimulation->state():re4::DestructionEffectsState{};
      if(!next.fragments)next.fragments=std::make_unique<re4::DestructionEffectsData>(fragmentData);
      else re4::AppendDestructionData(*next.fragments,fragmentData);
      next.destructionSimulation=std::make_unique<re4::DestructionEffectsSimulation>(*next.fragments,next.effect.get(),next.world.get());
      if(saved.signature) {
        saved.signature=next.fragments->signature;
        if(!next.destructionSimulation->restore(saved))throw std::runtime_error("Village: trap fragment state");
      }
      // EM2A bear traps retain the same original player reaction in all rooms.
      if(next.player)next.traps->installPlayerMotions(*next.player);
      auto trapBanks=readAudio(files+"/em/em2a.drs");next.sounds.insert(next.sounds.end(),trapBanks.begin(),trapBanks.end());
    }
  }
  // Validate all CPU resources before touching the current room or its GPU.
  if(room==0x100) {
    next.areaData=std::make_unique<re4::RoomAreaEffectsData>(re4::BuildRoomAreaEffects(arc,room,coreEnvironmentEff));
    const auto *roomEff=arc.find("EFF");
    if(!roomEff)throw std::runtime_error("r100: Original hit effects missing");
    next.surfaceHits=std::make_unique<re4::R100RoomHitEffects>(
        re4::BuildR100RoomHitEffects(copy(roomEff),coreEnvironmentEff));
    // Prepare weather in the destination's own camera frame. The active
    // renderer still belongs to the old room during this transaction.
    const bool destination=roomRequest.token && roomRequest.room==room;
    const auto feet=destination?roomRequest.position:re4::R100EntryPlayerPosition();
    const auto heading=destination?roomRequest.heading:re4::R100EntryPlayerHeading;
    const auto follow=re4::PoseFollowCamera({},next.world.get(),feet,heading,
        false,0,0,double(std::max(width,1))/std::max(height,1));
    re4::WorldEffectCamera initialCamera;
    for(unsigned k=0;k<3;++k) {
      initialCamera.eye[k]=float(follow.eye[k]);initialCamera.right[k]=float(follow.right[k]);
      initialCamera.up[k]=float(follow.up[k]);initialCamera.forward[k]=float(follow.look[k]);
    }
    next.areas=std::make_unique<re4::RoomAreaEffects>(*next.areaData,
        next.secondaryScript?next.secondaryScript->effectDisplayMask():0xffffffffu,&initialCamera);
    for(const auto &warning:next.areaData->warnings)__android_log_print(ANDROID_LOG_WARN,"RE4Effects","%s",warning.c_str());
  }
  if(room==0x103 || room==0x106 || room==0x111 || room==0x113 || room==0x11a) {
    next.areaData=std::make_unique<re4::RoomAreaEffectsData>(re4::BuildRoomAreaEffects(arc,room,coreEnvironmentEff));
    const bool destination=roomRequest.token && roomRequest.room==room;
    const auto feet=destination?roomRequest.position:(next.player?next.player->feet():re4::R100EntryPlayerPosition());
    const auto heading=destination?roomRequest.heading:(next.player?next.player->heading():re4::R100EntryPlayerHeading);
    const auto follow=re4::PoseFollowCamera({},next.world.get(),feet,heading,false,0,0,double(std::max(width,1))/std::max(height,1));
    re4::WorldEffectCamera camera;
    for(unsigned k=0;k<3;++k) {
      camera.eye[k]=float(follow.eye[k]);camera.right[k]=float(follow.right[k]);
      camera.up[k]=float(follow.up[k]);camera.forward[k]=float(follow.look[k]);
    }
    next.areas=std::make_unique<re4::RoomAreaEffects>(*next.areaData,0xffffffffu,&camera);
    for(const auto &warning:next.areaData->warnings)__android_log_print(ANDROID_LOG_WARN,"RE4Effects","%s",warning.c_str());
  }
  auto scene = re4::BuildStaticScene(*next.smd, -1, next.common.get());
  re4::ApplyRoomScriptVisibility(room, static_cast<const re4::ScenarioState &>(scenarioState).room(room), scene);
  if (scene.vertices.empty())
    throw std::runtime_error("No supported rigid room geometry");
  re4::AttachSceneTextures(*next.smd, scene, next.common.get());
  if (next.smx)
    re4::ApplySceneSmx(*next.smx, scene);
  if (next.environment)
    re4::AttachSceneEnvironment(*next.environment, scene);
  if (next.blocks) {
    re4::AppendStaticSceneLayer(scene, next.blocks->mesh);
    re4::ApplyRoomScriptVisibility(room,
        static_cast<const re4::ScenarioState &>(scenarioState).room(room), scene);
  }
  if (next.objects) re4::AppendRoomObjectScene(scene, *next.objects);
  if (next.items) re4::AppendRoomItemScene(scene, *next.items);
  if (next.looseItems) re4::AppendRoomItemScene(scene, *next.looseItems);
  if (next.fragments) re4::AppendDestructionScene(scene, *next.fragments);
  if (next.worldEffects) scene.worldEffects = *next.worldEffects;
  scene.laserSight = laserSightData;
  scene.lighting = next.lighting;
  scene.shadows = next.shadows;
  scene.footShadowTexture = footShadowTexture;
  if(coreSpecular)re4::AttachSceneSpecular(scene,*coreSpecular);
  std::size_t largest = 0;
  for (std::size_t i = 0; i < next.smd->placements.size(); ++i) {
    auto part = re4::BuildStaticScene(*next.smd, static_cast<int>(i),
                                      next.common.get());
    if (!part.vertices.empty()) {
      next.placements.push_back(static_cast<int>(i));
      if (part.triangles > largest) {
        largest = part.triangles;
        next.selected = static_cast<int>(i);
      }
    }
  }
  return next;
}
bool startWalking() {
  if (!walkProbe)
    return false;
  bool original = false;
  float heading = 3.7415927f;
  if (testProfile == 1 && activeRoomId == 0x101 && roomInfo)
    if (const auto *start = roomInfo->find(activeRoomId, 1)) {
      original = walkProbe->place(start->position);
      if (original) heading = static_cast<float>(start->heading);
    }
  // r100_StartEvent's actual playable placement after the opening movie.
  // Debug/roominfo point0 is the editor point at the bridge, not this start.
  if (activeRoomId == 0x100) {
    original = walkProbe->place({-99685, -484, -1343});
    if (original) heading = 2.246f;
  }
  if (!original && roomInfo) {
    if (const auto *start = roomInfo->find(activeRoomId)) {
      original = walkProbe->place(start->position);
      if (original)
        heading = static_cast<float>(start->heading);
    }
  }
  if (!original && !walkProbe->reset())
    return false;
  if (leon) {
    leon->setPlacement(walkProbe->position(), heading);
  }
  flowInfo = original ? "Original room start"
                      : "SAT test start (original start unusable)";
  paused = false;
  walking = true;
  __android_log_print(ANDROID_LOG_INFO, "RE4Flow", "%s: %s",
                      roomName(activeRoomId).c_str(), flowInfo.c_str());
  return true;
}
void requestRoom(std::uint16_t target, unsigned part,
                 const re4::CollisionVec &position, float heading,
                 bool combat = false, bool village = false) {
  std::lock_guard<std::mutex> lock(stateMutex);
  if (roomRequest.token)
    return;
  roomRequest = {++nextRoomToken, target, part, position, heading, combat, village};
  flowInfo = "Loading " + roomName(target) + " …";
  __android_log_print(
      ANDROID_LOG_INFO, "RE4Flow", "Request %llu %s -> %s Part %u",
      static_cast<unsigned long long>(roomRequest.token),
      roomName(activeRoomId).c_str(), roomName(target).c_str(), part);
}
bool transferPending() {
  std::lock_guard<std::mutex> lock(stateMutex);
  return roomRequest.token != 0;
}
void uploadPlayer(bool strict) {
  re4::SetActorSpecularBank(coreSpecular);
  actorGpuReady = false;
  if (!leon)
    return;
  try {
    re4::CreateActorGl(*leon);
    if (leon->weapon())
      re4::CreateWeaponGl(*leon->weapon());
    if(playerProjectiles) {
      for(const auto &entry:playerProjectiles->models()) {
        const auto &model=*entry.second;re4::CreateGenericActorGl(&model,model.mesh(),model.skin());
      }
      re4::CreateProjectileEffectsGl(playerProjectiles->effectsData());
      projectileGpuGeneration=playerProjectiles->modelGeneration();
    }
    if (leon->weapon() && leon->weapon()->hasMuzzleEffects() && !coreEnvironmentEff.empty()) {
      if (!muzzleEffects || muzzleWeaponId != leon->equippedWeaponId()) {
        muzzleEffects = std::make_unique<re4::MuzzleEffects>(
            re4::BuildWeaponWorldEffects(leon->weapon()->effects(),coreEnvironmentEff));
        muzzleWeaponId = leon->equippedWeaponId();
      }
      re4::CreateMuzzleEffectsGl(muzzleEffects->data());
    } else {
      muzzleEffects.reset();
      muzzleWeaponId = 0;
    }
    if (encounter) {
      re4::CreateEnemyGl(encounter->actor());
      if(enemyEffectsOwner!=encounter.get()) {
        const auto resource=re4::RoomArchive::LoadDrs(enemyAssetPath);
        if(const auto *effect=resource.find("EFF")) {
          enemyEffectsData=std::make_unique<re4::WorldEffectsData>(
              re4::BuildEnemyWorldEffects(resource.copy(effect-resource.entries().data()),coreEnvironmentEff));
          enemyEffects=std::make_unique<re4::WorldEffectsSimulation>(*enemyEffectsData);
          enemyEffectsOwner=encounter.get();
          __android_log_print(ANDROID_LOG_INFO,"RE4Enemy","%s",enemyEffectsData->describe().c_str());
        }
      }
      if(enemyEffectsData)re4::CreateEnemyEffectsGl(*enemyEffectsData);
    }
    if(villageBottles) {
      const auto &model=villageBottles->model();re4::CreateGenericActorGl(&model,model.mesh(),model.skin());
      re4::CreateVillageBottleEffectsGl(villageBottles->coreData(),villageBottles->trailData());
    }
    if(enemyGroup)
      for(const auto &enemy:enemyGroup->actors())re4::CreateEnemyGl(enemy->actor());
    if(enemyGroup && enemyEffectsData)re4::CreateEnemyEffectsGl(*enemyEffectsData);
    if (ambientAnimals)
      re4::CreateAnimalsGl(*ambientAnimals);
    if(forestAnimals)for(const auto &animal:forestAnimals->animals())for(unsigned wing:{0u,1u})
      re4::CreateGenericActorGl(animal.gpuOwner(wing),forestAnimals->mesh(wing),forestAnimals->skin(wing));
    if(villageAnimals)for(const auto &animal:villageAnimals->animals())
      re4::CreateGenericActorGl(&animal->model(),animal->model().mesh(),animal->model().skin());
    if(dogTraps)for(const auto &animal:dogTraps->actors())
      re4::CreateGenericActorGl(animal.get(),animal->mesh(),animal->skin());
    if(r100Secondary)for(const auto &actor:r100Secondary->actors())
      re4::CreateGenericActorGl(actor.get(),actor->mesh(),actor->skin());
    if(r100Secondary) {
      const auto &radio=r100Secondary->radioActor();
      re4::CreateGenericActorGl(&radio,radio.mesh(),radio.skin());
    }
    if(villageRadioIntro) {
      const auto &radio=villageRadioIntro->actor();
      re4::CreateGenericActorGl(&radio,radio.mesh(),radio.skin());
    }
    if(r100RadioTerm)re4::CreateRadioTermGl(*r100RadioTerm);
    if(r100HouseEnemy && r100EventPhase==10)re4::CreateEnemyGl(r100HouseEnemy->actor());
    if(trapEffectsData)re4::CreateTrapEffectsGl(*trapEffectsData);
    if(roomAreaEffectsData) {
      re4::CreateRoomAreaEffectsGl(roomAreaEffectsData->effects);
      re4::CreateRoomEffectClothGl(roomAreaEffectsData->effects);
    }
    if(roomHitEffects)re4::CreateRoomHitEffectsGl(roomHitEffects->data());
    if(itemEffects)re4::CreateItemEffectsGl(*itemEffects);
    actorGpuReady = true;
  } catch (const std::exception &e) {
    re4::DestroyActorGl();
    playerInfo = "Leon GPU unavailable: " + std::string(e.what());
    __android_log_print(ANDROID_LOG_ERROR, "RE4Player", "%s",
                        playerInfo.c_str());
    if (strict)
      throw;
  }
  if (walking && leon)
    re4::SceneFollowDistance(4000);
}
void uploadPreview(bool strict = false, bool preserveView = true) {
  NativeLoadClock loadClock("Geometry and GPU",activeRoomId);
  const auto view = re4::SaveSceneView();
  if (collisionView && previewSat) {
    auto scene = previewSat->scene();
    re4::DestroySceneGl();
    re4::CreateSceneGl(scene);
    uploadPlayer(strict);
    if (preserveView)
      re4::RestoreSceneView(view);
    auto info = previewSat->describe();
    __android_log_print(ANDROID_LOG_INFO, "RE4Collision", "%s", info.c_str());
    std::lock_guard<std::mutex> lock(stateMutex);
    graphics = "SAT collision · " + std::to_string(scene.triangles) +
               " triangles · Green: Floor · Yellow: Slope · Blue: Wall";
    return;
  }
  auto scene = re4::BuildStaticScene(
      *previewSmd, walking ? -1 : selectedPlacement, previewCommon.get());
  if (walking || selectedPlacement < 0)
    re4::ApplyRoomScriptVisibility(activeRoomId, static_cast<const re4::ScenarioState &>(scenarioState).room(activeRoomId), scene);
  scene.laserSight = laserSightData;
  scene.lighting = previewLighting;
  scene.shadows = previewShadows;
  scene.footShadowTexture = footShadowTexture;
  re4::AttachSceneTextures(*previewSmd, scene, previewCommon.get());
  if (previewSmx)
    re4::ApplySceneSmx(*previewSmx, scene);
  if (previewEnvironment)
    re4::AttachSceneEnvironment(*previewEnvironment, scene);
  if (previewWorldEffects && (walking || selectedPlacement < 0))
    scene.worldEffects = *previewWorldEffects;
  if (previewBlocks && (walking || selectedPlacement < 0)) {
    re4::AppendStaticSceneLayer(scene, previewBlocks->mesh);
    re4::ApplyRoomScriptVisibility(activeRoomId,
        static_cast<const re4::ScenarioState &>(scenarioState).room(activeRoomId), scene);
  }
  if (previewObjects && (walking || selectedPlacement < 0))
    re4::AppendRoomObjectScene(scene, *previewObjects);
  if (previewItems && (walking || selectedPlacement < 0))
    re4::AppendRoomItemScene(scene, *previewItems);
  if (previewLooseItems && (walking || selectedPlacement < 0))
    re4::AppendRoomItemScene(scene, *previewLooseItems);
  if (destructionData && (walking || selectedPlacement < 0))
    re4::AppendDestructionScene(scene, *destructionData);
  if(coreSpecular)re4::AttachSceneSpecular(scene,*coreSpecular);
  re4::DestroySceneGl();
  re4::CreateSceneGl(scene);
  uploadPlayer(strict);
  if (preserveView)
    re4::RestoreSceneView(view);
  std::size_t mipTextures = 0, mipLevels = 0, boundedMipChains = 0;
  for (const auto &t : scene.textures)
    if (!t.mips.empty()) {
      ++mipTextures;
      mipLevels += t.mips.size();
      boundedMipChains += t.mips.size() < t.maxLOD;
    }
  std::string info =
      ((walking || selectedPlacement < 0)
           ? "Total geometry"
           : "Base model " + std::to_string(selectedPlacement)) +
      "\n" + scene.describe() +
      "Base texture verification: " + std::to_string(scene.texturedTriangles) +
      " textured triangles · " + std::to_string(scene.fallbackTriangles) +
      " unresolved\n" +
      "Common textures: " + std::to_string(scene.commonTextureTriangles) +
      " base triangles · " + std::to_string(scene.commonAlphaTriangles) +
      " mask triangles\n" +
      "Alpha test: " + std::to_string(scene.alphaTriangles) +
      " triangles · " + std::to_string(scene.unresolvedAlphaTriangles) +
      " unresolved\n" + "Original-Mipmaps: " + std::to_string(mipTextures) +
      " textures · " + std::to_string(mipLevels) + " additional stages · " +
      std::to_string(boundedMipChains) + " bounded chains\n" +
      "SMX: " + std::to_string(scene.smxDraws) + " Draws · " +
      std::to_string(scene.materialColorTriangles) +
      " material color triangles\n" +
      "Vertex color path: " + std::to_string(scene.vertexColorTriangles) +
      " triangles\n" + "Normals: " + std::to_string(scene.normals.size() / 3) +
      " vertices · " + std::to_string(scene.zeroNormals) + " zero normals · " +
      std::to_string(scene.singularNormalPlacements) +
      " singular placements\n" +
      "A/Cross: Door action · X/Square: Model inspector · Y/Triangle: Inventory/Test menu · "
      "SELECT: Normals · Right: Camera "
      "· LT/RT: Distance · LB/RB: "
      "Model · Start: Total";
  __android_log_print(ANDROID_LOG_INFO, "RE4Scene", "%s", info.c_str());
  std::lock_guard<std::mutex> lock(stateMutex);
  graphics = ((walking || selectedPlacement < 0)
                  ? "Total geometry"
                  : "Base model " + std::to_string(selectedPlacement)) +
             " · " + std::to_string(scene.triangles) + " triangles · " +
             std::to_string(scene.textures.size()) + " textures";
}
const char *itemName(unsigned id) {
  return re4::OriginalItemLabel(id);
}
void synchronizeCase() {
  if (leon && re4::OriginalItemInfo(leon->equippedWeaponId()).type==1 && inventoryCase.equippedId()==leon->equippedWeaponId() &&
      !inventoryCase.synchronizeWeaponAmmo(leon->equippedWeaponId(), leon->magazine(), leon->reserve()))
    throw std::runtime_error("Could not consistently synchronize case and weapon ammo");
}
void applyCaseAmmo() {
  if (!leon) return;
  for (const auto &item : inventoryCase.items())
    if (item.active && re4::OriginalWeaponCarry(item.id)) {
      const auto id=inventoryCase.weaponId(item.slot);
      const bool direct=re4::OriginalItemInfo(item.id).type==3;
      leon->setWeaponAmmo(id,direct?1:item.loaded,
          direct?0:inventoryCase.count(re4::OriginalWeaponAmmo(id)));
    }
}
std::string jsonString(const std::string &value) {
  std::ostringstream out;out << '"';
  for(const auto ch:value) {
    const auto c=static_cast<unsigned char>(ch);
    if(c=='"' || c=='\\') out << '\\' << char(c);
    else if(c<32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
    else out << char(c);
  }
  out << '"';return out.str();
}
const char *menuLanguage() {
  return "eng";
}
re4::SubscreenArt &menuArt(const std::string &name) {
  const auto it=subscreenArt.find(name);
  if(it!=subscreenArt.end()) return it->second;
  if(name=="casechoices") {
    std::vector<std::array<int,4>> bounds;
    (void)re4::LoadOriginalPickupMessage(subscreenAssetRoot,0,0,0,1,true,&bounds);
    re4::SubscreenArt art;
    for(unsigned n=0;n<bounds.size();++n){const auto &b=bounds[n];re4::SubscreenQuad q;q.mark=n;q.kind=1;q.texture=255;q.vertices={float(b[0]),-float(b[1]),float(b[2]),-float(b[1]),float(b[2]),-float(b[3]),float(b[0]),-float(b[3])};art.quads.push_back(q);}
    return subscreenArt.emplace(name,std::move(art)).first->second;
  }
  if(name=="village_title") {
    auto authored=std::make_unique<re4::TitleData>(re4::TitleData::VillageTitle(subscreenAssetRoot));
    re4::SubscreenArt art;art.textures=authored->textures();
    auto &result=subscreenArt.emplace(name,std::move(art)).first->second;
    villageTitleArtwork=std::move(authored);return result;
  }
  if(name=="binocular") {
    const re4::RoomArchive room(re4::LoadYz2(subscreenAssetRoot+"/St1/r101.das").bytes);
    const auto textures=room.copy(0x27-4),layout=room.copy(0x28-4);
    auto authored=std::make_unique<re4::TitleData>(textures,layout);
    re4::SubscreenArt art;art.textures=re4::DecodeSubscreenIdTextures(textures);
    auto &result=subscreenArt.emplace(name,std::move(art)).first->second;
    binocularArtwork=std::move(authored);return result;
  }
  if(name.rfind("scope_",0)==0) {
    const auto id=unsigned(std::stoul(name.substr(6)));
    const auto *source=re4::OriginalWeaponCarry(id);
    if(!source || (source->number!=9 && source->number!=10 && source->number!=13 && id!=0xab))throw std::runtime_error("Scope: Invalid weapon");
    std::ostringstream path;path<<subscreenAssetRoot<<"/em/wep"<<std::setw(2)<<std::setfill('0')<<source->archive<<".drs";
    const auto archive=re4::RoomArchive::LoadDrs(path.str());
    const auto textures=archive.copy(0),layout=archive.copy(source->number==9 || source->number==10?source->variant+1:1);
    re4::SubscreenArt art;art.textures=re4::DecodeSubscreenIdTextures(textures);
    art.quads=re4::DecodeSubscreenLayout(layout,8000);
    // IdScope registers its weapon textures over the surviving CORE cockpit
    // registration. Scope plates/digits (including IDs10/242) retain that
    // shared owner; only missing IDs used by this layout need to be copied.
    const auto core=re4::RoomArchive::LoadDrs(subscreenAssetRoot+"/etc/core.das");
    for(auto shared:re4::DecodeSubscreenIdTextures(core.copy(0x1d-4)))
      if(!art.texture(shared.id,shared.frame) &&
          std::any_of(art.quads.begin(),art.quads.end(),[&](const auto &q) {
            return q.kind==0 && (q.texture==shared.id || q.mask==shared.id);
          }))art.textures.push_back(std::move(shared));
    auto authored=std::make_unique<re4::TitleData>(art.textures,layout);
    auto &result=subscreenArt.emplace(name,std::move(art)).first->second;
    scopeArtwork.emplace(name,std::move(authored));return result;
  }
  if(name=="examine")return subscreenArt.emplace(name,re4::LoadOriginalExamArt(subscreenAssetRoot)).first->second;
  const auto kind=name=="common"?re4::SubscreenArtKind::Common:
      name=="case"?re4::SubscreenArtKind::Case:name=="item"?re4::SubscreenArtKind::Keys:
      name=="file"?re4::SubscreenArtKind::Files:name=="map"?re4::SubscreenArtKind::Map:
      throw std::runtime_error("Original menu: Unknown pack");
  return subscreenArt.emplace(name,re4::LoadSubscreenArt(subscreenAssetRoot,kind,menuLanguage())).first->second;
}
void prepareSubscreenPortrait(unsigned ticks=0,double fraction=1) {
  if (!leon || subscreenAssetRoot.empty()) return;
  std::unique_lock<std::mutex> lock(subscreenAssetMutex);
  const auto id = leon->equippedWeaponId();
  const auto cached=subscreenPortraits.find(id);
  // SS uses an authored 640x480 camera. Physical-display readback multiplied
  // every GL stall, JNI allocation and bitmap upload without adding detail.
  constexpr int portraitWidth=640,portraitHeight=480;
  if(!ticks && fraction==1 && cached!=subscreenPortraits.end() && cached->second.width==portraitWidth && cached->second.height==portraitHeight && subscreenLeonRoot==subscreenAssetRoot)return;
  try {
    if(!subscreenLeon || subscreenLeonWeapon!=id || subscreenLeonRoot!=subscreenAssetRoot) {
      auto next=std::make_unique<re4::OriginalMenuModel>(re4::LoadOriginalMenuPortrait(subscreenAssetRoot,id,true));
      re4::DestroySubscreenPortraitGl(subscreenLeon.get());
      subscreenLeon=std::move(next);subscreenLeonWeapon=id;subscreenLeonRoot=subscreenAssetRoot;
    }
    subscreenLeon->advance(ticks);subscreenLeon->renderFraction=fraction;
    lock.unlock();
    auto portrait=re4::RenderSubscreenPortraitGl(*subscreenLeon,portraitWidth,portraitHeight);
    lock.lock();
    subscreenPortraits.insert_or_assign(id,std::move(portrait));
  } catch (const std::exception &e) {
    __android_log_print(ANDROID_LOG_WARN,"RE4Subscreen","Leon menu model: %s",e.what());
  }
}
void publishSubscreen() {
  std::ostringstream out;out << "{\"room\":" << jsonString(roomName(activeRoomId))
      << ",\"roomTitle\":" << jsonString(roomName(activeRoomId));
  if(leon) {
    const auto feet=leon->feet();
    out << ",\"player\":{\"x\":" << feet[0] << ",\"y\":" << feet[1]
        << ",\"z\":" << feet[2] << ",\"yaw\":" << leon->heading() << '}';
  }
  out << ",\"files\":[";bool first=true;
  for(const auto &item:inventoryCase.items()) if(item.active && re4::OriginalItemInfo(item.id).type==10) {
    if(!first)out << ',';
    first=false;
    out << "{\"id\":" << item.id << ",\"title\":" << jsonString(re4::OriginalItemLabel(item.id));
    try {
      const auto number=re4::OriginalFileNumber(item.id);
      const auto page=re4::OriginalFilePageInfo(number,0,menuLanguage());
      out << ",\"number\":" << number << ",\"pageCount\":" << page.pages;
    } catch(const std::exception&) {out << ",\"pageCount\":0";}
    out << '}';
  }
  out << ']';
  if(leon && !subscreenAssetRoot.empty()) try {
    const auto map=re4::OriginalMapSnapshotJson(subscreenAssetRoot,activeRoomId,leon->feet(),leon->heading(),-1,
        campaignMode?&scenarioState:nullptr,campaignMode?&originalSaveMetadata.passedRooms:nullptr,menuLanguage());
    out << ",\"map\":" << map;
  }catch(const std::exception &e) {
    out << ",\"map\":{\"paths\":[],\"markerValid\":false,\"note\":" << jsonString(e.what()) << '}';
  }
  out << '}';
  std::lock_guard<std::mutex> lock(stateMutex);subscreenInfo=out.str();
}
void uploadSubscreenItem() {
  subscreenItemDirty=false;
  re4::DestroySubscreenItemGl();
  if(subscreenItemId<0 || subscreenAssetRoot.empty())return;
  try {
    const auto id=unsigned(subscreenItemId);
    const auto profile=re4::OriginalItemExamProfile(id,subscreenExamining);
    const std::lock_guard<std::mutex> lock(subscreenAssetMutex);
    const std::string framePack="examine";
    const auto &frame=menuArt(framePack);
    std::array<float,2> frameY{145,-35};
    for(const auto &unit:frame.quads) {
      if(unit.mark==0xf2)frameY[0]=unit.position[1];
      if(unit.mark==0xf3)frameY[1]=unit.position[1];
    }
    re4::CreateSubscreenItemGl(re4::LoadOriginalItemScene(subscreenAssetRoot,id,subscreenExamining,pickupArchive.get()),
        profile.rotationDegrees,profile.scale,id,re4::LoadOriginalExamLighting(subscreenAssetRoot,profile.light),
        subscreenExamining?profile.inventoryAxis:profile.pickupAxis,subscreenExamining,frameY,
        re4::LoadOriginalItemSpecular(subscreenAssetRoot,id,subscreenExamining,pickupArchive.get()),frame);
    subscreenFrame={};
  } catch(const std::exception &e) {
    __android_log_print(ANDROID_LOG_WARN,"RE4Subscreen","Item view: %s",e.what());
  }
}
void publishCase() {
  synchronizeCase();
  const auto snapshot = inventoryCase.snapshotJson(scenarioState.pesetas, inventoryMessage);
  std::lock_guard<std::mutex> lock(stateMutex);
  caseInfo = snapshot;
}
std::string suppliesText() {
  std::ostringstream text;
  text << "Case " << inventoryCase.width() << "×" << inventoryCase.height();
  if(leon)text << (leon->unlimitedHealth()?" · HP ∞":" · HP "+std::to_string(leon->life())+"/"+std::to_string(re4::LeonPreview::MaxLife));
  if (leon) text << "\n" << re4::OriginalItemLabel(leon->equippedWeaponId()) << ": "
                 << leon->magazine() << "/" << leon->magazineCapacity() << " + " << leon->reserve();
  text << "\nPesetas: " << scenarioState.pesetas;
  for (const auto &item : inventoryCase.items()) if (item.active)
    text << "\n" << re4::OriginalItemLabel(item.id) << ": " << item.count;
  if (!inventoryMessage.empty()) text << "\n" << inventoryMessage;
  return text.str();
}
void useGreenHerb() {
  if (!leon || !leon->alive()) return;
  for (const auto &item : inventoryCase.items()) if (item.active && item.id == 0x06) {
    if (inventoryCase.consumeHealing(item.slot, leon->life(), re4::LeonPreview::MaxLife)) {
      leon->heal(600); inventoryMessage = "Green herb used";
    } else inventoryMessage = "Health is already full · Herb remains in the case";
    return;
  }
  inventoryMessage = "No green herb in the case";
}
void publishPickup() {
  if(!pendingPickup.token)return;
  const auto id=pendingPickup.id,count=pendingPickup.count;
  const auto type=re4::OriginalItemInfo(id).type;
  const bool automatic=type==0 || type==4 || type==5 || type==7 || type==8 || type==0xc;
  const bool usable=type==6 && id!=0x15 && id!=0x16 && re4::OriginalHealingAmount(id) && leon && leon->life()<re4::LeonPreview::MaxLife;
  const unsigned message=type==8?0x14:automatic?0x15:type==2?0x13:usable?0x12:0x11;
  std::ostringstream data;
  data << "{\"token\":" << pendingPickup.token << ",\"id\":" << id << ",\"count\":" << count
       << ",\"money\":" << re4::OriginalMoneyValue(id, count)
       << ",\"message\":" << message << ",\"choices\":" << (automatic?0:usable?3:2)
       << ",\"fits\":" << (inventoryCase.canAdd(id,count) ? "true" : "false")
       << ",\"name\":" << jsonString(re4::OriginalItemLabel(id)) << '}';
  std::lock_guard<std::mutex> lock(stateMutex);
  pickupInfo = data.str();
}
void beginPickup(bool loose, std::size_t index, unsigned id, unsigned count) {
  if (!leon || pendingPickup.token || !leon->beginItemExam()) return;
  pendingPickup = {++nextPickupToken, activeRoomId, index, id, count, loose};
  audioPickupActive.store(true);
  paused = true;
  suspendSimulationInput();
  publishPickup();
}
void completePickup(std::uint64_t token, bool accepted, bool use=false) {
  if (!pendingPickup.token || token != pendingPickup.token || pendingPickup.room != activeRoomId) return;
  const auto pending = pendingPickup;
  const auto type=re4::OriginalItemInfo(pending.id).type;
  accepted|=type==0 || type==4 || type==5 || type==7 || type==8 || type==0xc;
  bool taken = false;
  if (accepted) {
    auto candidate = inventoryCase;
    const auto healing=re4::OriginalHealingAmount(pending.id);
    const bool directUse=use && healing && pending.id!=0x15 && pending.id!=0x16 && leon && leon->life()<re4::LeonPreview::MaxLife;
    if (directUse || (!use && candidate.add(pending.id, pending.count))) {
      if (pending.loose && roomPickups && pending.index < roomPickups->items().size()) {
        taken = roomPickups->take(pending.index).has_value();
        if (taken && !re4::OriginalMoneyValue(pending.id,pending.count)) {
          auto &count = scenarioState.carriedItems.at(pending.id);
          count -= pending.count;
          if (!count) scenarioState.carriedItems.erase(pending.id);
        }
      } else if (!pending.loose && objectActions && leon && walkProbe) {
        const auto pickup = objectActions->takePickup(pending.index);
        taken = pickup && pickup->index == pending.index && pickup->id == pending.id && pickup->count == pending.count;
        if (taken) {
          objectActions->consumeItem(pending.id, pending.count);
          const auto money = re4::OriginalMoneyValue(pending.id,pending.count);
          scenarioState.pesetas = unsigned(std::min(std::uint64_t(99999999), std::uint64_t(scenarioState.pesetas)+money));
        }
      }
      if (taken) {if(directUse)leon->heal(healing);else inventoryCase = std::move(candidate);applyCaseAmmo();}
    }
    inventoryMessage = taken ? std::string(re4::OriginalItemLabel(pending.id)) + " picked up"
                             : "Not picked up · Check case space or pickup";
  } else inventoryMessage = "Pickup canceled · Item remains in the room";
  if (leon) leon->endItemExam(taken);
  pendingPickup = {};
  paused = false;
  audioGameActive.store(true);
  audioPickupActive.store(false);
  suspendSimulationInput(); walkFrame = {};
  { std::lock_guard<std::mutex> lock(stateMutex); pickupInfo.clear(); }
  __android_log_print(ANDROID_LOG_INFO,"RE4Items","Pickup %llu · %s",
      static_cast<unsigned long long>(token),inventoryMessage.c_str());
  publishCase();
}
void updateDropContext() {
  if (!leon) return;
  re4::EnemyDropContext context;
  context.health = leon->life();
  context.stage = activeRoomId >> 8;
  context.gameLevel = campaignMode && campaignDifficulty==1?2:campaignMode && campaignDifficulty==2?10:5;
  context.mercenaries = activeRoomId == 0x400;
  context.handgunAmmo = leon->handgunMagazine() + inventoryCase.count(0x04);
  context.shotgunAmmo = (inventoryCase.count(0x2c) ? leon->shotgunMagazine() : 0) + inventoryCase.count(0x18);
  context.tmpAmmo = inventoryCase.count(0x20);
  context.bowAmmo = inventoryCase.count(0x6a);
  context.greenHerbs = inventoryCase.count(0x06);
  for (const auto &item : inventoryCase.items()) if (item.active) {
    if (item.id == 0x05 || item.id == 0x06 || (item.id >= 0x12 && item.id <= 0x15))
      context.recoveryPoints += item.count;
    if (item.id == 0x16) context.recoveryPoints += item.count * 2;
  }
  if(objectActions)objectActions->setDropContext(context);
  for(const auto &item:inventoryCase.items())if(item.active && re4::OriginalItemInfo(item.id).type==1)context.carriedWeapons.push_back(item.id);
  context.gameMode=campaignDifficulty==1?1:0;
  currentEnemyDropContext=context;
  if(forestAnimals)forestAnimals->setDropContext(context);
  if(villageAnimals)villageAnimals->setDropContext(context);
}
bool routeRoomWeaponHits(const re4::PlayerShot *shot) {
  if(shot && shot->projectile())shot=nullptr;
  if ((!objectActions && !roomInteractions) || !leon) return false;
  // EmMgr already captured these EAT hits before setting pending damage or
  // breaking a box. Repeating the query afterwards can expose a wall that
  // was behind the actual target of this same shot.
  const auto shotScenery=shot && enemyGroup?enemyGroup->takeShotScenery(shot->serial):
      shot && encounter?encounter->takeShotScenery(shot->serial):std::vector<std::optional<re4::SatHit>>{};
  const auto shotExternal=shot && enemyGroup?enemyGroup->takeShotExternal(shot->serial):std::vector<bool>{};
  unsigned shotRay=0;
  struct Target {
    std::optional<re4::InteriorHit> interior;
    std::optional<re4::ObjectActionHit> object;
    unsigned weapon{}, serial{};
    re4::CollisionVec from{},to{};
    bool crow{};
    std::optional<re4::VillageAnimalHit> villageAnimal;
    std::optional<re4::DogTrapHit> dog;
    std::optional<re4::SatHit> surface;
    double surfaceDistance{};
  };
  std::vector<Target> targets;
  const auto registerRay = [&](const re4::CollisionVec &from,const re4::CollisionVec &to,unsigned weapon,unsigned serial) {
    Target target;
    target.weapon = weapon; target.serial = serial;target.from=from;target.to=to;
    // No enemy hit is still bounded by the authored segment. FLT_MAX is
    // not a valid ObjectActions distance and used to destroy Leon on a miss.
    double rayLengthSquared=0;
    for(unsigned axis=0;axis<3;++axis) {
      const auto delta=to[axis]-from[axis];rayLengthSquared+=delta*delta;
    }
    float nearest=float(rayLengthSquared);
    if(encounter)if(const auto hit=encounter->target(from,to,weapon))nearest=hit->lenSq;
    if(enemyGroup)if(const auto hit=enemyGroup->target(from,to,weapon,encounter.get()))nearest=std::min(nearest,hit->lenSq);
    if(effectWorld)if(const auto hit=effectWorld->line(from,to,re4::SatSubset::All,0,weapon==0x10?0:0x400000)) {
      double distance=0;for(unsigned k=0;k<3;++k){const auto d=hit->point[k]-from[k];distance+=d*d;}
      nearest=std::min(nearest,float(distance));
    }
    if(forestAnimals)if(const auto hit=forestAnimals->target(from,to,weapon))
      if(hit->lenSq<=nearest) {target.crow=true;nearest=hit->lenSq;}
    if(villageAnimals)if(const auto hit=villageAnimals->target(from,to,weapon,nearest)) {
      target.villageAnimal=hit;target.crow=false;nearest=hit->hit.lenSq;
    }
    if(dogTraps)if(const auto hit=dogTraps->target(from,to,weapon,nearest)) {
      target.dog=hit;target.crow=false;target.villageAnimal.reset();nearest=hit->hit.lenSq;
    }
    if (roomInteractions) {
      target.interior = roomInteractions->target(from,to,weapon);
      if(target.interior && target.interior->hit.lenSq>nearest)target.interior.reset();
    }
    if (objectActions) target.object = objectActions->target(from,to,weapon,nearest);
    if(roomHitEffects && effectWorld && weapon!=0x10) {
      target.surface=effectWorld->line(from,to,re4::SatSubset::All,0,0x400000);
      if(target.surface) {
        for(unsigned k=0;k<3;++k) {
          const auto delta=target.surface->point[k]-from[k];target.surfaceDistance+=delta*delta;
        }
        if(float(target.surfaceDistance)>nearest ||
           (target.object && target.object->hit.lenSq<=target.surfaceDistance) ||
           (target.interior && target.interior->hit.lenSq<=target.surfaceDistance) ||
           target.crow || target.dog || target.villageAnimal)target.surface.reset();
      }
    }
    if(target.object || (target.interior && target.interior->kind!=re4::InteriorHit::Window)) {
      target.crow=false;target.dog.reset();target.villageAnimal.reset();
    }
    targets.push_back(std::move(target));
  };
  const auto restoreExternalWinner=[&](Target &target,bool selected) {
    target.crow=false;target.dog.reset();target.villageAnimal.reset();
    // Group already consumed its winning box/barrel, including the source
    // secondary-pellet exclusion. A post-damage query cannot select another.
    target.object.reset();
    if(!selected)return;
    float nearest=1e16f;
    if(forestAnimals)if(const auto hit=forestAnimals->target(target.from,target.to,target.weapon))
      if(hit->lenSq<=nearest) {target.crow=true;nearest=hit->lenSq;}
    if(villageAnimals)if(const auto hit=villageAnimals->target(target.from,target.to,target.weapon,nearest)) {
      target.villageAnimal=hit;target.crow=false;nearest=hit->hit.lenSq;
    }
    if(dogTraps)if(const auto hit=dogTraps->target(target.from,target.to,target.weapon,nearest)) {
      target.dog=hit;target.crow=false;target.villageAnimal.reset();
    }
    // Through-glass remains the independent source target in front of
    // the selected animal.
    if(target.interior && target.interior->kind!=re4::InteriorHit::Window)target.interior.reset();
  };
  // Source: all thirteen shotgun rays register their targets before EmMgr
  // consumes any damage. Breaking a near pane/box cannot expose a farther
  // target to the remaining pellets of that same shot.
  if (shot) {
    const unsigned weapon = shot->weaponNumber();
    const auto firearmRay=[&](const re4::CollisionVec &from,const re4::CollisionVec &to) {
      registerRay(from,to,weapon,shot->serial);
      if(shotRay<shotScenery.size())targets.back().surface=shotScenery[shotRay];
      if(shotRay<shotExternal.size())restoreExternalWinner(targets.back(),shotExternal[shotRay]);
      ++shotRay;
    };
    if (shot->pellets.empty()) firearmRay(shot->from,shot->to);
    else for (const auto &pellet : shot->pellets) firearmRay(pellet[0],pellet[1]);
  }
  if (const auto &melee = leon->lastMeleeEvent())
    if (melee->kind == re4::PlayerMeleeKind::Knife) {
      const auto external=enemyGroup?enemyGroup->takeMeleeExternal(melee->serial):std::vector<bool>{};
      unsigned rayIndex=0;
      for (const auto &ray : melee->rays) {
        registerRay(ray.first,ray.second,0x10,melee->serial);
        if(rayIndex<external.size())restoreExternalWinner(targets.back(),external[rayIndex]);
        ++rayIndex;
      }
    }
  re4::PlayerShot crowShot;if(shot) {crowShot=*shot;crowShot.pellets.clear();}
  re4::PlayerMeleeEvent crowMelee;if(leon->lastMeleeEvent()) {crowMelee=*leon->lastMeleeEvent();crowMelee.rays.clear();}
  unsigned surfaceRay=0;
  for (const auto &target : targets) {
    if(target.surface && roomHitEffects) {
      const bool puddle=roomFootsteps && roomFootsteps->resolve(0,target.surface->point,nullptr,{},0xff).effect==1;
      roomHitEffects->trigger(*target.surface,target.weapon,
          (std::uint64_t(target.serial)<<16)|surfaceRay,previewEnvironment.get(),puddle);
    }
    ++surfaceRay;
    if(target.crow) {
      if(target.weapon==0x10)crowMelee.rays.push_back({target.from,target.to});
      else crowShot.pellets.push_back({target.from,target.to});
    }
    if(target.dog && dogTraps)dogTraps->applyShot(*target.dog,target.serial,target.weapon);
    if(target.villageAnimal && villageAnimals)villageAnimals->applyShot(*target.villageAnimal,target.serial,target.weapon,
        shot && target.weapon!=16?shot->powerLevel:0,target.from);
    if (target.interior && (!target.object || target.interior->hit.lenSq <= target.object->hit.lenSq))
      roomInteractions->damage(*target.interior,target.weapon,target.serial);
    else if (target.object) {
      if (target.weapon == 0x10) objectActions->applyKnife(*target.object,target.serial);
      else objectActions->applyShot(*target.object,target.serial,target.weapon);
    }
  }
  if(forestAnimals) {
    if(!crowShot.pellets.empty())forestAnimals->queueShot(crowShot);
    if(!crowMelee.rays.empty())forestAnimals->queueMelee(crowMelee);
  }
  return shot && !targets.empty() && (targets.front().crow || targets.front().dog || targets.front().villageAnimal || targets.front().interior || targets.front().object);
}
void drainTrapFragments() {
  if(!trapEffects || !destruction || !dogTraps)return;
  for(const auto &fragment:trapEffects->drainFragmentSpawns()) {
    const auto parent=trapEffectParents.find(fragment.eventSerial);
    if(parent==trapEffectParents.end())continue;
    const unsigned slot=0x100+unsigned(dogTraps->sources().at(parent->second.actor).listIndex);
    destruction->triggerTrapRecord(fragment.sequence,fragment.sourceRecord,slot,fragment.placement,
                                  fragment.eventSerial,fragment.spawnSerial,fragment.seed);
  }
}
void drainEnemyFeedback(re4::EnemyEncounter &owner,std::array<std::uint64_t,4> &handles) {
    for(const auto &event:owner.drainSounds()) {
      auto &handle=handles.at(unsigned(event.channel));
      if(event.stop || event.replace) {soundMixer.stop(handle);handle=0;}
      if(!event.stop) {
        const auto number=event.block==5 && roomFootsteps
            ? roomFootsteps->resolve(event.number,event.position).number : event.number;
        const auto voice=playRoomSound(event.block,number,event.position,event.block==8?0x10:0);
        if(event.replace || event.channel!=re4::EnemySoundChannel::Impact) handle=voice;
      }
    }
    for(const auto &event:owner.drainEffects())if(enemyEffects) {
      const auto serial=re4::GanadoEffectTriggerSerial(owner.listIndex(),event.eventSerial);
      enemyEffects->trigger(event.sequence,event.position,event.rotation,serial,collisionWorld.get());
    }
}
void afterWorldTick(const re4::PlayerShot *shot) {
  if(shot && shot->projectile()) {
    if(!playerProjectiles)playerProjectiles=std::make_unique<re4::PlayerProjectiles>(playerAssetPath,coreEnvironmentEff);
    playerProjectiles->launch(*shot,*leon,*effectWorld);
  }
  if(shot && re4::OriginalItemInfo(shot->weaponItemId).type==3 &&
      !leon->unlimitedAmmo() && !leon->weapon()->infinite()) {
    if(!inventoryCase.consume(shot->weaponItemId,1))throw std::runtime_error("Subweapon: Case item missing at release");
    if(inventoryCase.count(shot->weaponItemId))leon->setWeaponAmmo(shot->weaponItemId,1,0);
  }
  ++cockpitTicks; // One original 30-Hz player/world tick, independent of display refresh.
  if(campaignMode && leon && !leon->alive() && !deathRequested.load()) {
    // gameDiedemoCheck uses DiedemoExec(90,0) for normal Leon deaths.
    // This follows world ticks; the later ID/menu timeline owns its own clock.
    if(++deathTicks>=90){
      soundMixer.fadeAll();eventMixer.fadeAll();soundMixer.requestStream(0,0,0x80000003u);
      deathRequested.store(true);subscreenOpen=true;suspendSimulationInput();
    }
  }
  const bool roomAccuracyHit=routeRoomWeaponHits(shot);
  if(campaignMode) {
    if(shot) {
      ++originalSaveMetadata.chapterShots;++originalSaveMetadata.totalShots;
      bool hit=roomAccuracyHit;
      if(enemyGroup) {
        if(saveAccuracyOwner!=enemyGroup.get() || enemyGroup->accuracyHits()<saveAccuracyCursor) {
          saveAccuracyOwner=enemyGroup.get();saveAccuracyCursor=0;
        }
        hit=enemyGroup->accuracyHits()>saveAccuracyCursor;
        saveAccuracyCursor=enemyGroup->accuracyHits();
      }
      if(hit){++originalSaveMetadata.chapterHits;++originalSaveMetadata.totalHits;}
    }
    const auto countKills=[&](const re4::EnemyEncounter &enemy) {
      auto &before=saveKillCursors[&enemy];const unsigned current=enemy.sourceKillCount();
      if(current<before)before=0;
      originalSaveMetadata.chapterKills+=current-before;originalSaveMetadata.totalKills+=current-before;before=current;
    };
    if(encounter)countKills(*encounter);
    if(enemyGroup)for(const auto &enemy:enemyGroup->actors())if(enemy.get()!=encounter.get())countKills(*enemy);
  }
  if(shot)forestGunFired=true;
  if(forestAnimals) {
    for(const auto &event:forestAnimals->takeSounds())playRoomSound(event.block,event.number,event.position,event.argument);
    for(const auto &drop:forestAnimals->takeDrops())if(roomPickups) {
      const auto index=roomPickups->addRuntimeDrop(drop.id,drop.count,drop.position,drop.effect);
      r100DeadIndices.insert(drop.listIndex);
      // Ordinary drops are original glow effects with no drawn ITM body.
      // Their live effect owner updates independently of the room VBO.
      campaignSceneDirty=campaignSceneDirty || roomPickups->visible(int(index));
    }
  }
  if(villageAnimals) {
    for(const auto &event:villageAnimals->takeSounds()) {
      auto &handle=villageAnimalVoices[event.listIndex];soundMixer.stop(handle);
      handle=playRoomSound(8,event.number,event.position,event.enemyId);
    }
    for(const auto &drop:villageAnimals->takeDrops())if(roomPickups) {
      const auto index=roomPickups->addRuntimeDrop(drop.id,drop.count,drop.position,drop.effect);
      campaignSceneDirty=campaignSceneDirty || roomPickups->visible(int(index));
    }
  }
  if(dogTraps) {
    for(const auto &event:dogTraps->drainSounds()) {
      auto &handle=dogTrapVoiceHandles[event.enemyId];
      if(event.stop || event.replace) {soundMixer.stop(handle);handle=0;}
      if(!event.stop) {
        const auto voice=playRoomSound(event.block,event.number,event.position,event.block==8?event.enemyId:0);
        // Only the replacing primary voice corresponds to EM21::sndId;
        // independent sequence sounds and trap snaps can overlap it.
        if(event.replace)handle=voice;
      }
    }
    for(const auto &effect:dogTraps->drainEffects())if(leon && trapEffects) {
      const auto parts=dogTraps->effectParents(effect,*leon);
      if(trapEffects->triggerAttached(effect.sequence,effect.position,effect.rotation,
                                     effect.eventSerial,parts,collisionWorld.get()))
        trapEffectParents.emplace(effect.eventSerial,effect);
    }
    drainTrapFragments();
    if(dogTraps->takeRescueStreamRequest())eventMixer.installStream(dogRescueStream,true);
  }
  if(encounter)drainEnemyFeedback(*encounter,enemyVoiceHandles);
  if(r100HouseEnemy)drainEnemyFeedback(*r100HouseEnemy,groupVoiceHandles[r100HouseEnemy.get()]);
  if(enemyGroup)for(const auto &enemy:enemyGroup->actors())drainEnemyFeedback(*enemy,groupVoiceHandles[enemy.get()]);
  if(roomHitEffects) {
    for(const auto &sound:roomHitEffects->drainSounds())
      playRoomSound(sound.block,sound.number,{sound.position[0],sound.position[1],sound.position[2]});
    for(const auto &request:roomHitEffects->drainUnsupported())
      __android_log_print(ANDROID_LOG_WARN,"RE4RoomHit","Original hit effect %u/%u requires an additional backend path",request.effect.owner,request.effect.sequence);
  }
  if (ladderWindowSweep && roomInteractions && leon)
    roomInteractions->breakWindowsCrossed(ladderWindowFrom,playerPartPosition(0));
  ladderWindowSweep = false;
  if (leon) {
    const auto pose=re4::SceneFollowPose();
    std::array<float,3> cameraTarget{};
    for(unsigned k=0;k<3;++k)cameraTarget[k]=float(pose.target[k]);
    r100PlayerFootstep=false;
    for(const auto &event:leon->drainSequenceSounds()) {
      switch(event.number) {
        case 0:case 1:case 2:case 3:case 0xd:case 0xe:case 0x14:
          r100PlayerFootstep=true;break;
        default:break;
      }
      for(const auto &cue:re4::SourceFootSoundCue(event.number+1,event.sub)) {
        const auto &point=cue.part==0?event.rootPosition:event.position;
        const auto selected=cue.block==5 && roomFootsteps
            ? roomFootsteps->resolve(cue.number,point,previewEnvironment.get(),cameraTarget)
            : re4::FootstepSoundSelection{cue.number,0,-1,false};
        playRoomSound(cue.block,selected.number,point);
      }
    }
    for (const auto &sound : leon->drainLadderSounds())
      playRoomSound(sound.block, sound.number, leon->feet());
  }
  if(campaignMode && activeRoomId==0x100 && encounter && encounter->scenarioDeathReady() &&
      !scenarioState.room(0x100).room.test(10) && !activeEvent)r100DeathPending=true;
  if(enemyGroup)for(const auto &enemy:enemyGroup->actors())
    if(!enemy->alive() && (!enemy->r100CliffEncounter() || enemy->cliffFalling()))r100DeadIndices.insert(enemy->listIndex());
  if (roomLadders && leon)
    for (const auto &sound : roomLadders->drainSounds())
      playRoomSound(sound.block,sound.number,sound.position);
  const auto dropEnemy=[&](const re4::EnemyEncounter &enemy) {
    if(enemy.r100CliffEncounter() || enemy.alive() || !enemy.scenarioDeathReady() ||
        !r100DroppedIndices.insert(enemy.listIndex()).second || !roomPickups)return;
    std::optional<re4::EnemyDropChoice> drop;
    int effect=-1;
    if(pickupArchive) {if(const auto authored=re4::FindAuthoredEnemyDrop(*pickupArchive,enemy.listIndex(),scenarioState,activeRoomId)) {
      effect=int(authored->effect);
      if(re4::ClaimAuthoredEnemyDrop(*authored,scenarioState))
        drop=re4::ResolveAuthoredEnemyDrop(*authored,currentEnemyDropContext,enemyDropState);
    } else drop=re4::OriginalEnemyRandomDrop(enemy.villageGanado()?0x15:0x12,currentEnemyDropContext,enemyDropState);}
    if(drop) {
      const auto index=roomPickups->addRuntimeDrop(drop->id,drop->count,enemy.feet(),effect);
      campaignSceneDirty=campaignSceneDirty || roomPickups->visible(int(index));
    }
  };
  if(encounter)dropEnemy(*encounter);
  if(enemyGroup)for(const auto &enemy:enemyGroup->actors())dropEnemy(*enemy);
  synchronizeCase();
  if (shot) {
    if (muzzleEffects) muzzleEffects->trigger(shot->serial);
    if(shot->weaponNumber()!=19 && shot->weaponNumber()!=22 && shot->weaponNumber()!=23)playRoomSound(2,0,shot->from);
    if(shot->weaponNumber()==1 || shot->weaponNumber()==2 || shot->weaponNumber()==4 || shot->weaponNumber()==6 || shot->weaponNumber()==17)playRoomSound(2,2,shot->from);
  }
  if (leon) {
    if (muzzleEffects && leon->weapon()) {
      const auto posed=leon->weapon()->posedParts(leon->rig().jointMatrices(),leon->action(),leon->rig().sequenceFrame());
      const auto part=re4::WeaponAttachmentMatrix(posed.at(muzzleEffects->data().weaponPart),leon->placement());
      muzzleEffects->tick(part);
    }
    const auto action = leon->action();
    const bool shotgun = leon->weapon()->shotgun();
    if (action == re4::PlayerAction::Reload && lastSoundAction != action)
      playRoomSound(2, shotgun ? 7 : 0x16, playerPartPosition(10));
    if (shotgun && leon->weapon()) {
      const auto motion = leon->weapon()->motion();
      const auto frame = leon->weapon()->frame();
      const float previous = motion == lastWeaponSoundMotion ? lastWeaponSoundFrame : -1;
      if ((motion == re4::WeaponMotion::Fire && previous < 20 && frame >= 20) ||
          (motion == re4::WeaponMotion::Reload && previous < 66 && frame >= 66))
        playRoomSound(2,2,playerPartPosition(10));
      lastWeaponSoundMotion = motion; lastWeaponSoundFrame = frame;
    }
    for(const auto &sound:leon->drainWeaponSounds())
      playRoomSound(sound.block,sound.number,sound.position);
    lastSoundAction = action;
  }
  if (roomInteractions) {
    for (const auto &event : roomInteractions->drainEvents()) {
      if (destruction && event.effectSequence >= 0) {
        const auto serial = ++windowEffectSerial;
        destruction->triggerWindow(event.effectObject,unsigned(event.effectSequence),event.slot,
          {float(event.position[0]),float(event.position[1]),float(event.position[2])},
          event.effectRotation,serial,unsigned(serial));
      }
      if (event.soundBlock >= 0)
        playRoomSound(unsigned(event.soundBlock), event.soundNo, event.position);
      flowInfo = re4::InteriorEventLabel(event.kind);
      __android_log_print(ANDROID_LOG_INFO, "RE4Interaction",
                          "r101 ETS #%u · %s · Sound %d/%02x",
                          event.slot, flowInfo.c_str(), event.soundBlock, event.soundNo);
    }
    storeInteractionState();
  }
  ++roomEntryTicks;
  if(villageScenario && enemyGroup && leon && leon->alive()) {
    villageScenario->tick(scenarioState,leon->feet(),leon->heading(),enemyGroup->villageAlive(),enemyGroup->villageFoundPlayer(),false,leon->action()==re4::PlayerAction::Footwork && !villageBinocular);
    drainVillageCues();
    if(roomInteractions)roomInteractions->syncVillageState(scenarioState.room(0x101));
  }
  if (!objectActions) return;
  for (const auto &event : objectActions->drainEvents()) {
    scenarioState.room(objectActions->room()).setObject(event.slot, 1);
    if (roomInteractions) roomInteractions->recordBrokenObject(event.slot);
    if (destruction)
      destruction->trigger(event.objectId, event.boxType, event.slot,
                           event.position, event.rotation, event.breakKind,
                           event.serial, event.serial ^ unsigned(objectActions->profile()));
    if (event.explosive)
      re4::TriggerWorldEffectsGl(event.objectId, event.position, event.rotation, event.serial);
    playRoomSound(event.soundBlock, event.soundNo,
                  {event.position[0], event.position[1], event.position[2]});
    __android_log_print(ANDROID_LOG_INFO, "RE4Destruction",
                        "ETS #%u · ET%02x · Event %u · %s · Sound %u/%02x",
                        event.slot, event.objectId, event.serial,
                        event.explosive ? "Explosion" : "Wood fragments",
                        event.soundBlock, event.soundNo);
  }
}
void drainEventAudio() {
  if(!activeEvent)return;
  for(const auto &cue:activeEvent->takeAudioCues()) {
    if(cue.stream) {
      if(cue.block)eventMixer.installStream(activeEvent->audioStream(cue),true);
      else soundMixer.installStream(activeEvent->audioStream(cue),true);
    }
    else eventVoiceHandles.push_back(soundMixer.play(cue.block,cue.number));
    __android_log_print(ANDROID_LOG_INFO,"RE4Event","%s · Sound %s %u/%u · Frame %u",
        cue.eventName.c_str(),cue.stream?"Stream":"Cue",cue.block,cue.number,cue.sourceFrame);
  }
}
void drainVillageCues() {
  if(!villageScenario || !enemyGroup || !villageSpawns)return;
  for(const auto &cue:villageScenario->takeCues()) {
    switch(cue.kind) {
    case re4::VillageCue::Spawn:
      for(auto index:cue.indices) {
        auto *actor=enemyGroup->find(index);if(!actor)throw std::runtime_error("r101: Spawn owner missing");
        actor->resetVillageSpawn(*villageSpawns->find(index));
        r100DeadIndices.erase(index);r100DroppedIndices.erase(index);
        if(scenarioState.room(0x101).room.test(6))actor->releaseVillageAlarm();
      }
      break;
    case re4::VillageCue::Alarm:
      enemyGroup->releaseVillageAlarm();soundMixer.installStream(villageCombatStream,true);break;
    case re4::VillageCue::CombatStart:
      soundMixer.requestStream(0,2,0x80000003u,600,0);break;
    case re4::VillageCue::CombatStop:
      soundMixer.requestStream(0,2,4,600,0);break;
    case re4::VillageCue::RemoveGanados:case re4::VillageCue::BellComplete:
      for(const auto &actor:enemyGroup->actors()) {
        actor->setScriptSuspended(true);actor->setVisible(false);
        if(auto voices=groupVoiceHandles.find(actor.get());voices!=groupVoiceHandles.end()) {
          for(auto handle:voices->second)soundMixer.stop(handle);
          groupVoiceHandles.erase(voices);
        }
        actor->drainSounds();
      }
      if(walkProbe)walkProbe->setActorBlockers({});
      if(cue.kind==re4::VillageCue::BellComplete)soundMixer.installStream(nullptr);
      break;
    case re4::VillageCue::StartBell:
      soundMixer.requestStream(0,2,4,600,0);campaignSceneDirty=true;break;
    case re4::VillageCue::FirstWaveCamera:
      villageWaveCamera=cue.camera-4;villageCameras.at(*villageWaveCamera)->reset();
      villageWaveActors.clear();
      for(unsigned i=cue.camera==5?0x28:0x2e;i<=(cue.camera==5?0x2c:0x32);++i)villageWaveActors.push_back(i);
      eventMixer.installStream(villageWaveStreams.at(*villageWaveCamera),true);
      r100EventPhase=15;eventAccumulator=0;eventUiActive.store(true);suspendSimulationInput();break;
    case re4::VillageCue::TowerBottle:if(villageBottles)villageBottles->throwAt(villageScenario->towerTarget(),*effectWorld);break;
    }
  }
}
void finishVillageWave() {
  if(!villageWaveCamera)return;
  villageScenario->finishWaveCamera();villageWaveCamera.reset();villageWaveActors.clear();
  eventMixer.installStream(nullptr);r100EventPhase=0;eventAccumulator=0;eventUiActive.store(false);
  if(leon)re4::SceneFollowHeading(leon->heading());
  suspendSimulationInput();
}
void beginVillageEvent(re4::VillageEvent event) {
  if(playerProjectiles)playerProjectiles->clear();
  if(!villageScenario || !enemyGroup || !leon || !walkProbe)throw std::runtime_error("r101: Event prerequisites missing");
  villageReturnPosition=leon->feet();villageReturnHeading=leon->heading();villageEvent=event;
  const bool seenBinocular=scenarioState.room(0x101).room.test(5);
  villageScenario->begin(event,scenarioState,enemyGroup->villageAlive());drainVillageCues();
  if(event==re4::VillageEvent::Binoculars && seenBinocular) {finishVillageEvent();return;}
  if(event==re4::VillageEvent::House && roomInteractions)roomInteractions->setVillageHouseRack(*walkProbe);
  if(event!=re4::VillageEvent::Binoculars)soundMixer.installStream(nullptr);
  activeEvent=villageEvents.at(unsigned(event)).get();if(!activeEvent)throw std::runtime_error("r101: Prepared event missing");
  activeEvent->reset();eventAccumulator=0;eventUiActive.store(true);suspendSimulationInput();
  re4::SyncEventActorsGl(*activeEvent);drainEventAudio();
}
void finishVillageEvent(bool canceled) {
  if(!villageEvent || !villageScenario)return;
  const auto event=*villageEvent;
  if(canceled)for(auto voice:eventVoiceHandles)soundMixer.stop(voice);
  eventVoiceHandles.clear();if(canceled)eventMixer.installStream(nullptr);
  activeEvent=nullptr;villageEvent.reset();eventAccumulator=0;eventUiActive.store(false);
  if(event==re4::VillageEvent::House) {
    if(roomInteractions)roomInteractions->breakVillageWindow(0,*walkProbe);
    if(walkProbe->placeAuthored({2666,0,-8495}))leon->setPlacement(walkProbe->position(),-1.21f);
    if(roomLadders)roomLadders->setVillageHouse(true,*walkProbe,roomInteractions.get());
  } else if(event==re4::VillageEvent::Binoculars) {
    // Event00 uses cPlayer::setPos directly, then holds the English camera.
    const re4::CollisionVec position{-38027,141,11114};
    walkProbe->setScriptPosition(position);leon->setPlacement(position,1.72f);
    leon->holdBinoculars(true);villageBinocular=std::make_unique<re4::VillageBinoculars>();
    binocularUiActive.store(true);r100EventPhase=16;eventUiActive.store(true);
    soundMixer.play(1,2);flowInfo="Binoculars · LS/D-pad: Look · RS up/down: Zoom · B: Exit";
  } else if(villageReturnPosition && walkProbe->placeAuthored(*villageReturnPosition))
    leon->setPlacement(walkProbe->position(),villageReturnHeading);
  villageScenario->finish(event,scenarioState);drainVillageCues();
  if(roomInteractions) {roomInteractions->syncVillageState(scenarioState.room(0x101));storeInteractionState();}
  if(pickupArchive)previewObjects=std::make_unique<re4::RoomObjectLayer>(re4::BuildRoomObjects(*pickupArchive,coreEnvironmentEff,0x101,&scenarioState.room(0x101)));
  if(event==re4::VillageEvent::House)soundMixer.installStream(villageCombatStream,true);
  if(event==re4::VillageEvent::Bell) {scenarioState.room(0x101).room.set(10);r100RadioQueued=2;}
  campaignSceneDirty=true;villageReturnPosition.reset();re4::SceneFollowHeading(leon->heading());suspendSimulationInput();
}
void finishVillageBinoculars() {
  if(!villageBinocular)return;
  villageBinocular.reset();binocularUiActive.store(false);
  if(leon) {leon->holdBinoculars(false);re4::SceneFollowHeading(leon->heading());}
  r100EventPhase=0;eventAccumulator=0;eventUiActive.store(false);
  flowInfo.clear();suspendSimulationInput();
}
void selectR100RadioMessage(unsigned call) {
  if(call>=96)throw std::runtime_error("r100: Radio message outside original bitfield");
  r100RadioHeard[call>>5]|=0x80000000u>>(call&31);r100RadioMessageNo=call;
}
void drainR100RadioAudio() {
  if(!r100RadioTerm)return;
  for(const auto &cue:r100RadioTerm->takeAudio()) {
    if(cue.kind==re4::RadioTermAudio::Sample)soundMixer.play(cue.block,cue.number);
    else if(cue.kind==re4::RadioTermAudio::StreamRequest) {
      // SsTermMain::OpeMesMove stops the take-out jingle before its voice
      // request, rather than allowing it to overlap the conversation.
      eventMixer.requestStream(1,r100RadioCall==12?0x33:3,8);eventMixer.requestStream(cue.block,cue.number,1);
    }
    else if(cue.kind==re4::RadioTermAudio::StreamStart)eventMixer.requestStream(cue.block,cue.number,2);
    else eventMixer.requestStream(cue.block,cue.number,8);
  }
}
void beginR100Radio(unsigned call) {
  if(call>12 || (!r100Secondary && !villageRadioIntro) || !leon || !walkProbe || !r100RadioData[call])
    throw std::runtime_error("r100: Original radio not prepared");
  r100RadioCall=call;r100RadioQueued.reset();selectR100RadioMessage(call);
  r100RadioReturnPosition.reset();
  if(call) {r100RadioReturnPosition=leon->feet();r100RadioReturnHeading=leon->heading();}
  if(villageRadioIntro)villageRadioIntro->begin(*leon,*walkProbe);
  else r100Secondary->beginRadioIntro(*leon,*walkProbe,call);
  eventMixer.requestStream(1,call==12?0x33:3,0x80000003);
  r100EventPhase=12;eventAccumulator=0;eventUiActive.store(true);
  r100RadioFadeFrame.reset();r100RadioControls={};suspendSimulationInput();flowInfo="Radio contact";
}
void finishR100Radio() {
  drainR100RadioAudio();
  const bool entrySave=activeRoomId==0x101 && r100RadioCall==12;
  if(r100RadioCall<=12)eventMixer.requestStream(1,0x8c+r100RadioCall,8);
  eventMixer.requestStream(1,r100RadioCall==12?0x33:3,8);
  re4::DestroyRadioTermGl();r100RadioTerm.reset();
  if(villageRadioIntro && leon)villageRadioIntro->end(*leon);
  else if(r100Secondary && leon)r100Secondary->finishRadioIntro(*leon);
  if(r100RadioReturnPosition && leon && walkProbe) {
    walkProbe->setScriptPosition(*r100RadioReturnPosition);
    leon->setPlacement(*r100RadioReturnPosition,r100RadioReturnHeading);
  }
  r100RadioReturnPosition.reset();r100RadioCall=~0u;r100RadioQueued.reset();
  r100RadioControls={};
  // FadeSetW(0x80000000,3) and FadeSetW(0x80000001,10) run
  // concurrently from black to clear; FadeKill(2) removes the room fade.
  r100RadioFadeFrame=0;r100EventPhase=0;eventAccumulator=0;eventUiActive.store(false);
  if(leon)re4::SceneFollowHeading(leon->heading());
  flowInfo.clear();suspendSimulationInput();
  if(entrySave && leon && leon->alive()) {
    entryCheckpoint=captureNativeGame();entryCheckpoint->saveKind=-1;
    checkpointBasis=re4::EncodeNativeSave(*entryCheckpoint,originalSaveBasis?&*originalSaveBasis:nullptr);
  }
}
void beginR100Event(unsigned phase) {
  if(playerProjectiles)playerProjectiles->clear();
  activeEvent=phase==1?r100FirstEvent.get():phase==2?r100TruckEvent.get():
      phase>=3 && phase<=5?openingEvents[phase-3].get():
      phase>=6 && phase<=9?secondaryEvents[phase-6].get():nullptr;
  if(!activeEvent)throw std::runtime_error("r100: Prepared original event missing");
  r100EventPhase=phase;eventAccumulator=0;activeEvent->reset();
  if(r100Secondary)r100Secondary->beginSourceEvent(phase);
  eventUiActive.store(true);
  suspendSimulationInput();
  if(phase==1)re4::BeginR100FirstEncounter(scenarioState.room(0x100));
  else if(phase==2) {
    r100DeathPending=false;soundMixer.installStream(nullptr);
    if(enemyGroup)for(const auto &enemy:enemyGroup->actors())if(!enemy->r100CliffEncounter()) {
      enemy->setVisible(true);re4::CreateEnemyGl(enemy->actor());
    }
  }
  re4::SyncEventActorsGl(*activeEvent);drainEventAudio();
  flowInfo=phase==1?"Inside house":phase==2?"Ambush":phase>=6?"Examine":"Opening sequence";
  __android_log_print(ANDROID_LOG_INFO,"RE4Event","%s",activeEvent->describe().c_str());
}
void finishR100Event(bool canceled=false) {
  if(villageEvent){finishVillageEvent(canceled);return;}
  const auto phase=r100EventPhase;
  if(r100Secondary)r100Secondary->endSourceEvent(phase);
  const auto finalPlacement=activeEvent?activeEvent->endPlayerPlacement():std::optional<re4::SmdPlacement>{};
  if((phase==3 && re4::OriginalOpeningCanceled(scenarioState)) || phase==4) {
    // Hold the final source frame while the ordinary room-loader transaction
    // prepares r100. The event/room owners survive until commit succeeds.
    for(const auto handle:eventVoiceHandles)soundMixer.stop(handle);
    eventVoiceHandles.clear();eventMixer.installStream(nullptr);
    requestRoom(0x100,0,re4::R120RoadJumpPosition(),0);
    suspendSimulationInput();return;
  }
  re4::DestroyEventActorsGl();
  activeEvent=nullptr;eventUiActive.store(false);r100EventPhase=0;eventAccumulator=0;
  if(canceled)for(const auto handle:eventVoiceHandles)soundMixer.stop(handle);
  eventVoiceHandles.clear();if(canceled)eventMixer.installStream(nullptr);
  if(phase==1) {
    // ExePacket_Str(0,0) is room combat music, not an event voice. It must
    // survive both natural completion and a cancel before the final packet.
    re4::EventAudioCue music;music.stream=true;music.block=0;music.number=0;
    if(r100FirstEvent)if(auto stream=r100FirstEvent->audioStream(music))soundMixer.installStream(std::move(stream));
    if(!walkProbe || !leon || !preparedFirstEnemy || !walkProbe->place(re4::R100FirstPlayerPosition()))
      throw std::runtime_error("r100: Original placement after s03 missing");
    leon->setPlacement(walkProbe->position(),re4::R100FirstPlayerHeading);
    encounter=std::move(preparedFirstEnemy);
    re4::CreateEnemyGl(encounter->actor());
    if(enemyEffectsData)re4::CreateEnemyEffectsGl(*enemyEffectsData);
    re4::SceneFollowHeading(leon->heading());
    flowInfo="First Ganado";
  } else if(phase==2) {
    re4::CompleteR100FirstEncounter(scenarioState);
    if(scenarioCollision)scenarioCollision->sync(scenarioState.room(0x100),collisionWorld.get(),effectWorld.get());
    if(enemyGroup) {enemyGroup->releaseR100Ambush();enemyGroup->releaseR100CliffActors();}
    if(dogTraps)dogTraps->releaseAfterTruck();
    campaignSceneDirty=true;flowInfo="Ambush";
    // R100Event_Truck keeps ScreenStop through OpeSetOpenTerm. Commit the
    // radio placement before any ordinary room frame can show the saved
    // pre-event player at the window. The radio still restores that saved
    // gameplay placement on completion, as sscrn.cpp does.
    beginR100Radio(1);
  } else if(phase==3) {
    beginR100Event(4);return;
  } else if(phase==5) {
    if(!walkProbe || !leon || !walkProbe->place(re4::R100EntryPlayerPosition()))
      throw std::runtime_error("r100: Original placement after entry missing");
    leon->setPlacement(walkProbe->position(),re4::R100EntryPlayerHeading);
    re4::SceneFollowHeading(leon->heading());flowInfo.clear();
    if(!re4::OriginalOpeningCanceled(scenarioState))beginR100Radio(0);
    else selectR100RadioMessage(0);
  } else if(phase>=6 && phase<=9 && leon && walkProbe) {
    if(secondaryReturnPosition) {
      walkProbe->setScriptPosition(*secondaryReturnPosition);
      leon->setPlacement(*secondaryReturnPosition,secondaryReturnHeading);
    } else if(finalPlacement) {
      const auto &p=finalPlacement->position;
      // GetZeroPartsWorldPos uses SatMgr::getFloor(600,100000,mask0),
      // then zeroPartsPosInit sets that position directly. It does not
      // require an already-grounded root or the preview spawn capsule.
      re4::CollisionVec target{p[0],p[1],p[2]},top=target,bottom=target;
      top[1]+=600;bottom[1]-=100000;
      const auto floor=collisionWorld?collisionWorld->line(top,bottom,re4::SatSubset::All,0x40):std::optional<re4::SatHit>{};
      target[1]=floor?floor->point[1]:-100000;
      walkProbe->setScriptPosition(target);leon->setPlacement(target,finalPlacement->rotation[1]);
    }
    secondaryReturnPosition.reset();re4::SceneFollowHeading(leon->heading());flowInfo.clear();
  }
  suspendSimulationInput();
}
void finishR100RoomScript() {
  if(r100EventPhase==16) {finishVillageBinoculars();return;}
  if(r100EventPhase==15) {finishVillageWave();return;}
  if(r100EventPhase>=12 && r100EventPhase<=14) {finishR100Radio();return;}
  if(r100Secondary)r100Secondary->endSourceEvent(r100EventPhase);
  if(r100EventPhase==10) {
    if(r100Secondary && leon)r100Secondary->finishHouse(*leon);
    if(r100HouseEnemy) {
      const auto voices=groupVoiceHandles.find(r100HouseEnemy.get());
      if(voices!=groupVoiceHandles.end()){for(const auto handle:voices->second)soundMixer.stop(handle);groupVoiceHandles.erase(voices);}
      re4::DestroyEnemyGl(r100HouseEnemy->actor());
    }
    r100HouseEnemy.reset();eventMixer.installStream(nullptr);
  } else if(r100Secondary)r100Secondary->endCamera();
  if(r100EventPhase==11 && r100RoomMessage && r100RoomMessage->message().number==0x2f) {
    soundMixer.requestStream(0,0x12,8);
    if(r100RoomStream)soundMixer.requestStream(0,r100RoomStream->number,4,200,0x2d);
  }
  r100RoomMessage.reset();r100EventPhase=0;eventAccumulator=0;eventUiActive.store(false);
  if(leon)re4::SceneFollowHeading(leon->heading());
  flowInfo.clear();suspendSimulationInput();
}
void beginR100Secondary(const re4::R100SecondaryOffer &offer) {
  if(!r100Secondary || !leon || !walkProbe || !r100Scenario)return;
  if(!r100Scenario->beginSecondary(scenarioState.room(0x100),offer))return;
  secondaryReturnPosition.reset();
  if(offer.preservePlayerPosition) {secondaryReturnPosition=leon->feet();secondaryReturnHeading=leon->heading();}
  if(offer.playerPosition) {walkProbe->setScriptPosition(*offer.playerPosition);leon->setPlacement(*offer.playerPosition,leon->heading());}
  if(offer.kind==re4::R100SecondaryOffer::EngineEvent) {
    if(offer.phase==9) {
      eventMixer.requestStream(1,0xa,8);
      if(r100RoomStream)soundMixer.requestStream(0,r100RoomStream->number,4,600,0);
    }
    beginR100Event(offer.phase);return;
  }
  if(offer.kind==re4::R100SecondaryOffer::HouseScare) {
    if(!r100HouseEnemy)throw std::runtime_error("r100: Original temporary house Ganado missing");
    r100Secondary->beginHouse(*leon,*walkProbe);r100HouseEnemy->beginR100HouseScare();
    re4::CreateEnemyGl(r100HouseEnemy->actor());eventMixer.installStream(r100HouseStream,true);
    r100EventPhase=10;
  } else {
    if(offer.area==0xb)soundMixer.play(6,0x29); // r100_MesDoor: unpositioned knock.
    const unsigned y=0x150-(subscreenLanguage?32:27)-(subscreenLanguage?25:27)-1;
    const unsigned attr=offer.cameraCut==~0u && offer.kind!=re4::R100SecondaryOffer::CorpseMessage?0x1002:0x1012;
    r100RoomMessage=std::make_unique<re4::R100MessageRuntime>(r100Secondary->roomMessage(offer.message,attr,100,y));
    if(offer.cameraCut!=~0u)r100Secondary->beginCamera(offer.cameraCut);
    else if(offer.kind==re4::R100SecondaryOffer::CorpseMessage) {
      if(!encounter)throw std::runtime_error("r100: Original examinable Ganado missing");
      r100Secondary->beginCorpseCamera(*leon,*encounter);
    }
    if(offer.message==0x2f) {
      if(r100RoomStream)soundMixer.requestStream(0,r100RoomStream->number,4,200,1);
      soundMixer.requestStream(0,0x12,3);
    }
    r100EventPhase=11;
  }
  r100Secondary->beginSourceEvent(r100EventPhase,offer.cameraCut);
  eventAccumulator=0;eventUiActive.store(true);suspendSimulationInput();flowInfo="Examine";
}
void tickRoomAreaEffects(const re4::EventCamera *eventCamera=nullptr) {
  if(!roomAreaEffects || !leon)return;
  // Event::ExeBeginEvt sets STA_EFFAREA_USE_CAM; ordinary SceCam and
  // radio messages retain the player+100 area probe. Both use their camera
  // basis for source Esp06/Esp0b/world billboard geometry.
  const auto *camera=eventCamera;
  if(!camera && r100Secondary && r100Secondary->cameraActive())camera=&r100Secondary->camera();
  const auto pose=camera && camera->valid?re4::EventCameraPose(*camera):re4::SceneFollowPose();
  re4::WorldEffectCamera view;
  for(unsigned k=0;k<3;++k) {
    view.eye[k]=float(pose.eye[k]);view.right[k]=float(pose.right[k]);
    view.up[k]=float(pose.up[k]);view.forward[k]=float(pose.look[k]);
  }
  const std::array<float,3> feet{float(leon->feet()[0]),float(leon->feet()[1]),float(leon->feet()[2])};
  roomAreaEffects->tick(feet,view,r100Secondary?r100Secondary->effectDisplayMask():0xffffffffu,0,eventCamera!=nullptr);
}
void advanceR100RoomScript(double seconds,bool confirm,bool skipDialogue=false,bool close=false,const re4::InputFrame *binocularInput=nullptr) {
  if(r100EventPhase==16 && (close || skipDialogue || (binocularInput && (binocularInput->pressed&re4::Run)))) {finishVillageBinoculars();return;}
  if(r100EventPhase==11)r100MessageConfirm|=confirm || skipDialogue;
  if(r100EventPhase==13) {
    r100RadioControls[0]|=confirm;r100RadioControls[1]|=skipDialogue;r100RadioControls[2]|=close;
  }
  eventAccumulator+=std::clamp(seconds,0.,.05)*30;
  while(r100EventPhase>=10 && eventAccumulator>=1) {
    eventAccumulator-=1;
    if(r100Secondary)r100Secondary->tickSuspended30Hz();
    if(r100EventPhase==16) {
      const re4::InputFrame empty{};const auto &input=binocularInput?*binocularInput:empty;
      villageBinocular->tick(input.lx,input.ly,input.ry,unsigned(input.held));
      enemyGroup->tickScriptActors(*leon,*walkProbe);afterWorldTick(nullptr);
      tickRoomAreaEffects(&villageBinocular->camera());
      if(enemyGroup->villageFoundPlayer() || scenarioState.room(0x101).room.test(6))finishVillageBinoculars();
      continue;
    }
    if(r100EventPhase==15) {
      auto &camera=*villageCameras.at(*villageWaveCamera);camera.tick30Hz();
      enemyGroup->tickScriptActors(*leon,*walkProbe,villageWaveActors);
      for(auto index:villageWaveActors)if(auto *actor=enemyGroup->find(index))drainEnemyFeedback(*actor,groupVoiceHandles[actor]);
      tickRoomAreaEffects(&camera.camera());
      if(camera.finished() || close || skipDialogue)finishVillageWave();
      continue;
    }
    if(r100EventPhase==14) {tickRoomAreaEffects();finishR100Radio();break;}
    else if(r100EventPhase==12) {
      if(close) {tickRoomAreaEffects();finishR100Radio();break;}
      if(villageRadioIntro)villageRadioIntro->tick30Hz(*leon,*walkProbe);
      else r100Secondary->tickRadioIntro30Hz(*leon,*walkProbe);
      if(villageRadioIntro?villageRadioIntro->finished(*leon):r100Secondary->radioIntroFinished(*leon)) {
        r100RadioTerm=std::make_unique<re4::RadioTermRuntime>(r100RadioData.at(r100RadioCall));
        re4::CreateRadioTermGl(*r100RadioTerm);drainR100RadioAudio();r100EventPhase=13;
      }
    } else if(r100EventPhase==13) {
      r100RadioTerm->tick30Hz(r100RadioControls[0],r100RadioControls[1],r100RadioControls[2],true,
          eventMixer.streamReady(1,0x8c+r100RadioCall));
      r100RadioControls={};drainR100RadioAudio();
      if(r100RadioTerm->finished())r100EventPhase=14; // SubScreenWait(0); SceSleep(1).
    } else if(r100EventPhase==10) {
      if(r100HouseEnemy && leon) {
        r100HouseEnemy->tick(leon->feet());
        drainEnemyFeedback(*r100HouseEnemy,groupVoiceHandles[r100HouseEnemy.get()]);
      }
      if(leon && walkProbe)leon->step(*walkProbe,0,0,false,false,1./30);
      r100Secondary->tickCamera30Hz();
      if(r100Secondary->cameraFinished())finishR100RoomScript();
    } else {
      if(r100RoomMessage)r100RoomMessage->tick30Hz(r100MessageConfirm);
      r100MessageConfirm=false;
      r100Secondary->tickCamera30Hz();
      if((!r100RoomMessage || r100RoomMessage->finished()) &&
          (!r100Secondary->cameraActive() || r100Secondary->cameraFinished()))finishR100RoomScript();
    }
    tickRoomAreaEffects();confirm=false;skipDialogue=false;close=false;
  }
}
void beginCampaignEntry() {
  if(!campaignMode)return;
  if(activeRoomId==0x101 && villageRadioIntro && !scenarioState.scenario.test(18)) {
    scenarioState.scenario.set(18);beginR100Radio(12);
  } else if(activeRoomId==0x101 && villageRadioIntro && scenarioState.room(0x101).room.test(7) && !scenarioState.room(0x101).room.test(10)) {
    scenarioState.room(0x101).room.set(10);beginR100Radio(2);
  } else if(activeRoomId==0x120)beginR100Event(3);
  else if(activeRoomId==0x100 && re4::BeginR100Entry(scenarioState.room(0x100))) {
    if(!re4::OriginalOpeningCanceled(scenarioState))beginR100Event(5);
    else if(leon && walkProbe && walkProbe->place(re4::R100EntryPlayerPosition())) {
      leon->setPlacement(walkProbe->position(),re4::R100EntryPlayerHeading);
      re4::SceneFollowHeading(leon->heading());
      // r100_RoomStart still executes OpeSetMdtNo(0) on opening cancel.
      selectR100RadioMessage(0);
    }
  }
}
// Source event clock is independent of render refresh. Background/subscreen
// pauses preserve the EVD frame, actor pose, camera and mixer playback.
void advanceR100Event(double seconds) {
  eventAccumulator+=std::clamp(seconds,0.,.05)*30;
  while(activeEvent && eventAccumulator>=1) {
    eventAccumulator-=1;activeEvent->tick30Hz();drainEventAudio();
    if(r100Secondary)r100Secondary->tickSuspended30Hz();
    if(r100EventPhase==2 && activeEvent->sourceCut()>=2 && enemyGroup && leon && walkProbe) {
      for(unsigned index:{4u,5u})if(auto *enemy=enemyGroup->find(index))enemy->setScriptSuspended(false);
      enemyGroup->tickScriptActors(*leon,*walkProbe);
    }
    tickRoomAreaEffects(&activeEvent->camera());
    if(villageEvent==re4::VillageEvent::House && activeEvent->sourceCut()==0xa && activeEvent->cutFrame()==0x20 && roomInteractions)
      roomInteractions->breakVillageWindow(0,*walkProbe);
    if(activeEvent->finished())finishR100Event();
  }
}
void beforeWorldTick(double forward = 0, bool allowPush = true) {
  if(r100RadioFadeFrame) {
    if(*r100RadioFadeFrame>=10)r100RadioFadeFrame.reset();
    else ++*r100RadioFadeFrame;
  }
  const auto pose=re4::SceneFollowPose();
  if(playerProjectiles && leon && effectWorld) {
    re4::WorldEffectCamera camera;
    for(unsigned k=0;k<3;++k){camera.eye[k]=float(pose.eye[k]);camera.right[k]=float(pose.right[k]);camera.up[k]=float(pose.up[k]);camera.forward[k]=float(pose.look[k]);}
    playerProjectiles->tick(*effectWorld,camera,[&](const re4::CollisionVec &from,const re4::CollisionVec &to,unsigned number) {
      std::optional<re4::AuthoredHitResult> nearest;
      const auto accept=[&](const std::optional<re4::AuthoredHitResult> &hit){if(hit && (!nearest || hit->lenSq<nearest->lenSq))nearest=hit;};
      if(encounter)accept(encounter->target(from,to,number));
      if(enemyGroup)accept(enemyGroup->target(from,to,number,encounter.get()));
      if(objectActions)if(const auto hit=objectActions->target(from,to,number))accept(hit->hit);
      if(roomInteractions)if(const auto hit=roomInteractions->target(from,to,number))accept(hit->hit);
      if(forestAnimals)accept(forestAnimals->target(from,to,number));
      if(villageAnimals)if(const auto hit=villageAnimals->target(from,to,number))accept(hit->hit);
      if(dogTraps)if(const auto hit=dogTraps->target(from,to,number))accept(hit->hit);
      return nearest;
    },[&](const re4::PlayerShot &impact){
      if(enemyGroup)enemyGroup->queueShot(impact,encounter.get());
      else if(encounter)encounter->queueProjectileImpact(impact);
      routeRoomWeaponHits(&impact);
    });
    for(const auto &cue:playerProjectiles->drainCues())playRoomSound(cue.block,cue.number,cue.position);
  }
  if(roomHitEffects) {
    re4::WorldEffectCamera camera;
    for(unsigned k=0;k<3;++k) {
      camera.eye[k]=float(pose.eye[k]);camera.right[k]=float(pose.right[k]);
      camera.up[k]=float(pose.up[k]);camera.forward[k]=float(pose.look[k]);
    }
    roomHitEffects->tick(camera,previewEnvironment.get(),collisionWorld.get());
  }
  tickRoomAreaEffects();
  if(villageBottles && leon) {
    re4::WorldEffectCamera camera;
    for(unsigned k=0;k<3;++k){camera.eye[k]=float(pose.eye[k]);camera.right[k]=float(pose.right[k]);camera.up[k]=float(pose.up[k]);camera.forward[k]=float(pose.look[k]);}
    villageBottles->tick(*leon,*effectWorld,camera);
    if(villageBottles->takeExplosion()){playRoomSound(1,0x15,leon->feet());playRoomSound(1,0x16,leon->feet());}
  }
  if(roomPickups)roomPickups->tick();
  if(itemEffects) {
    syncItemEffects();re4::WorldEffectCamera camera;
    for(unsigned k=0;k<3;++k) {
      camera.eye[k]=float(pose.eye[k]);camera.up[k]=float(pose.up[k]);
      camera.right[k]=float(pose.right[k]);camera.forward[k]=float(pose.look[k]);
    }
    itemEffects->tick(&camera,collisionWorld.get());
  }
  if(encounter)encounter->setAwarenessInputs(r100PlayerFootstep,forestGunFired);
  if(enemyGroup)for(const auto &enemy:enemyGroup->actors())
    enemy->setAwarenessInputs(r100PlayerFootstep,forestGunFired);
  if(forestAnimals && leon)forestAnimals->tick(leon->feet(),forestGunFired);
  if(villageAnimals && leon)villageAnimals->tick(leon->feet(),forestGunFired,
      activeRoomId==0x101 && scenarioState.room(0x101).room.test(6),activeRoomId==0x101 && scenarioState.room(0x101).room.test(7),leon.get());
  forestGunFired=false;
  if(r100Secondary && leon) {
    std::vector<re4::R100Threat> threats;
    if(enemyGroup)for(const auto &enemy:enemyGroup->actors())if(!enemy->r100CliffEncounter()) {
      double squared=0;for(unsigned k=0;k<3;++k) {const auto d=enemy->feet()[k]-leon->feet()[k];squared+=d*d;}
      threats.push_back({enemy->foundPlayer(),squared});
    }
    r100Secondary->tick30Hz(scenarioState.room(0x100),leon->feet(),leon->heading(),
        r100OfficerPressed,r100WeaponDontFire,activeEvent!=nullptr,threats);
    r100OfficerPressed=false;
    if(r100Secondary->takeOfficerMessage())r100RoomMessage=std::make_unique<re4::R100MessageRuntime>(r100Secondary->roomMessage(0x33));
    if(r100RoomMessage && !r100EventPhase) {
      r100RoomMessage->tick30Hz(r100MessageConfirm);r100MessageConfirm=false;
      if(r100RoomMessage->finished())r100RoomMessage.reset();
    }
    for(const auto &cue:r100Secondary->takeSounds()) {
      if(cue.kind==re4::R100RoomSound::Sample)playRoomSound(cue.block,cue.number,cue.position);
      else if(cue.kind==re4::R100RoomSound::RoomStreamStart && r100RoomStream) {
        if(soundMixer.streamStopping(0,r100RoomStream->number))soundMixer.requestStream(0,r100RoomStream->number,4,600,r100RoomStream->volume);
        else soundMixer.requestStream(0,r100RoomStream->number,0x80000003);
      } else if(cue.kind==re4::R100RoomSound::RoomStreamStop && r100RoomStream)
        soundMixer.requestStream(0,r100RoomStream->number,4,cue.number*200,0);
      else if(cue.kind==re4::R100RoomSound::StreamFade)eventMixer.requestStream(cue.block,cue.number,4,cue.frames,cue.volume);
      else if(cue.kind==re4::R100RoomSound::RoomStreamVolume && r100RoomStream)soundMixer.requestStream(0,r100RoomStream->number,4,cue.frames,cue.volume);
      else if(cue.kind==re4::R100RoomSound::RoomStreamReset && r100RoomStream)soundMixer.requestStream(0,r100RoomStream->number,4,cue.frames,r100RoomStream->volume);
    }
  }
  if(encounter)encounter->setFeedbackCamera(pose.eye);
  if(enemyGroup)enemyGroup->setFeedbackCamera(pose.eye);
  if(enemyEffects) {
    re4::WorldEffectCamera camera;
    for(unsigned k=0;k<3;++k) {
      camera.eye[k]=float(pose.eye[k]);camera.right[k]=float(pose.right[k]);
      camera.up[k]=float(pose.up[k]);camera.forward[k]=float(pose.look[k]);
    }
    enemyEffects->tick(&camera,collisionWorld.get());
  }
  if(dogTraps && leon && walkProbe) {
    std::vector<re4::CollisionVec> living;
    if(encounter && encounter->alive())living.push_back(encounter->feet());
    if(enemyGroup)for(const auto &enemy:enemyGroup->actors())
      if(enemy->alive() && !enemy->scriptSuspended())living.push_back(enemy->feet());
    dogTraps->tick(*leon,*walkProbe,living);
  }
  if(trapEffects && dogTraps && leon) {
    for(auto at=trapEffectParents.begin();at!=trapEffectParents.end();) {
      const auto &event=at->second;
      const bool armed=event.sequence==3 || event.sequence==5;
      const bool visible=event.actor<dogTraps->actors().size() && dogTraps->actors()[event.actor]->visible();
      if(armed && !visible) {trapEffects->stopInstance(at->first);at=trapEffectParents.erase(at);continue;}
      if(!event.detached)trapEffects->updateInstanceParts(at->first,dogTraps->effectParents(event,*leon));
      ++at;
    }
    re4::WorldEffectCamera camera;
    for(unsigned k=0;k<3;++k) {
      camera.eye[k]=float(pose.eye[k]);camera.right[k]=float(pose.right[k]);
      camera.up[k]=float(pose.up[k]);camera.forward[k]=float(pose.look[k]);
    }
    trapEffects->tick(&camera,collisionWorld.get());drainTrapFragments();
    const auto &instances=trapEffects->state().instances;
    for(auto at=trapEffectParents.begin();at!=trapEffectParents.end();) {
      if(std::none_of(instances.begin(),instances.end(),[&](const auto &instance){return instance.eventSerial==at->first;}))
        at=trapEffectParents.erase(at);
      else ++at;
    }
  }
  ladderWindowSweep = leon && leon->objectLadderActive();
  if (ladderWindowSweep) ladderWindowFrom = playerPartPosition(0);
  scenarioState.transient.clearFrameWords();
  if(campaignMode && activeRoomId==0x100 && r100Scenario && roomInteractions && leon) {
    // sceAtCheck_main's original area9 is per-frame; area3 is a latched
    // Room_flg0 flag and has targetType3 (player plus active enemies).
    scenarioState.transient.set(64,r100Scenario->houseInsideHit(leon->feet(),leon->heading()));
    if(r100Scenario->doorClosureFlagHit(leon->feet(),leon->heading()))scenarioState.transient.set(0);
    const auto latchEnemy=[&](const re4::EnemyEncounter &enemy) {
      if(enemy.alive() && !enemy.scriptSuspended() &&
          r100Scenario->doorClosureFlagHit(enemy.feet(),enemy.heading()))scenarioState.transient.set(0);
    };
    if(encounter)latchEnemy(*encounter);
    std::array<bool,3> found{};
    if(enemyGroup) {
      for(const auto &enemy:enemyGroup->actors())latchEnemy(*enemy);
      for(unsigned i=0;i<3;++i)if(const auto *enemy=enemyGroup->find(i+3))
        found[i]=enemy->alive() && !enemy->scriptSuspended() && enemy->foundPlayer();
    }
    if(roomInteractions->tickR100Scripts(scenarioState.room(0x100),scenarioState.transient,found) && enemyGroup)
      enemyGroup->releaseR100HouseAmbush();
  }
  if (roomAmbient) roomAmbient->tick([](unsigned block,unsigned cue,const re4::SoundPoint &position,bool positional) {
    return positional ? playRoomSound(block,cue,position) : soundMixer.play(block,cue);
  });
  if (roomLadders && walkProbe) roomLadders->tick(*walkProbe,roomInteractions.get());
  if (roomInteractions && leon && walkProbe)
    roomInteractions->tick(*leon, *walkProbe, forward, allowPush);
  updateDropContext();
  if (destruction) destruction->tick();
  if (!objectActions || !leon || !walkProbe) return;
  auto blasts=objectActions->tick();
  if(playerProjectiles) {
    auto extra=playerProjectiles->drainBlasts();
    for(const auto &blast:extra)objectActions->applyExplosion(blast);
    blasts.insert(blasts.end(),extra.begin(),extra.end());
  }
  if(dogTraps) {auto extra=dogTraps->drainBlasts();blasts.insert(blasts.end(),extra.begin(),extra.end());}
  for (const auto &blast : blasts) {
    if(forestAnimals && blast.weaponNo!=23)forestAnimals->queueExplosion(blast.position,blast.radius);
    if(villageAnimals)villageAnimals->queueExplosion(blast.position,blast.radius,blast.weaponNo);
    if(dogTraps && blast.weaponNo==19)dogTraps->applyBlast(blast);
    // Original combat module resolves player/enemy radial shapes and EAT.
    if(blast.weaponNo==18 || blast.weaponNo==19) {
      if(const auto hit=objectActions->playerBlastHit(blast,leon->parts0World(),leon->equippedWeaponId()==0x6d))leon->applyBlast(*hit,blast.position,*walkProbe);
    } else if(blast.weaponNo==20) {
      const auto point=leon->parts0World();const double x=point[0]-blast.position[0],z=point[2]-blast.position[2],y=point[1]-blast.position[1];
      if(x*x+z*z<2500.*2500. && y>=0 && y<1500)leon->receiveContinuousFlame();
    }
    if (encounter) encounter->applyBlast(blast, *leon, *walkProbe);
    if(enemyGroup)for(const auto &enemy:enemyGroup->actors())
      if(!enemy->scriptSuspended())enemy->applyBlast(blast,*leon,*walkProbe);
  }
}
GLuint shader(GLenum type, const char *code) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &code, nullptr);
  glCompileShader(s);
  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024]{};
    glGetShaderInfoLog(s, sizeof log, nullptr, log);
    glDeleteShader(s);
    throw std::runtime_error(log);
  }
  return s;
}
void makeProgram() {
  const char *vs = "#version 300 es\nlayout(location=0) in vec2 "
                   "p;layout(location=1) in vec2 uv;out vec2 v;uniform vec2 "
                   "scale;void main(){v=uv;gl_Position=vec4(p*scale,0,1);}";
  const char *fs =
      "#version 300 es\nprecision mediump float;in vec2 v;uniform sampler2D "
      "image;out vec4 color;void main(){vec4 t=texture(image,v);vec3 "
      "bg=mix(vec3(.10),vec3(.16),mod(floor(v.x*16.)+floor(v.y*16.),2.));color="
      "vec4(mix(bg,t.rgb,t.a),1);}";
  GLuint a = shader(GL_VERTEX_SHADER, vs), b = shader(GL_FRAGMENT_SHADER, fs);
  program = glCreateProgram();
  glAttachShader(program, a);
  glAttachShader(program, b);
  glLinkProgram(program);
  glDeleteShader(a);
  glDeleteShader(b);
  GLint ok;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok)
    throw std::runtime_error("Could not link GLES program");
  const float quad[] = {-1, -1, 0, 1, 1, -1, 1, 1, -1, 1, 0, 0, 1, 1, 1, 0};
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<void *>(2 * sizeof(float)));
  glEnableVertexAttribArray(1);
}
} // namespace
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_resetInput(JNIEnv *, jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  frame = {};
  input.reset();
  ++inputGeneration;
}
extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_NativeBridge_input(
    JNIEnv *, jclass, jint b, jfloat lx, jfloat ly, jfloat rx, jfloat ry,
    jfloat lt, jfloat rt) {
  std::lock_guard<std::mutex> lock(stateMutex);
  input.submit({static_cast<std::uint32_t>(b), lx, ly, rx, ry, lt, rt});
}
extern "C" JNIEXPORT void JNICALL Java_com_ast_re4decomp_NativeBridge_create(
    JNIEnv *env, jclass, jstring path, jint imageIndex) {
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    normalView = false;
    sceneStatus = false;
  }
  // Previous GL names belong to the lost context and must not be deleted in the
  // new context.
  program = vao = vbo = texture = 0;
  hasTexture = false;
  sceneMode = false;
  sceneSession = 0;
  roomInfo.reset();
  roomFlow.reset();
  flowInfo.clear();
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    roomRequest = {};
    roomCompletion = {};
  }
  resetWalk();
  collisionView = false;
  previewSat.reset();
  leon.reset();
  playerInfo.clear();
  previewSmd.reset();
  previewLighting.reset();previewShadows.reset();
  previewCommon.reset();
  previewSmx.reset();
  previewEnvironment.reset();
  previewWorldEffects.reset();
  previewObjects.reset();
  previewPlacements.clear();
  re4::ForgetSceneGl();
  re4::ForgetDisplayBrightnessGl();
  re4::ForgetGraphicsOptionsGl();
  try {
    makeProgram();
    RndInit(0x0d37);
    const auto *version = glGetString(GL_VERSION);
    if (!version)
      throw std::runtime_error("No valid GLES context");
    std::string info = reinterpret_cast<const char *>(version);
    const char *chars = env->GetStringUTFChars(path, nullptr);
    if (!chars)
      return;
    const std::string filename(chars);
    env->ReleaseStringUTFChars(path, chars);
    try {
      if (imageIndex < 0)
        throw std::runtime_error("Invalid TPL image index");
      auto t = re4::LoadTpl(filename, static_cast<unsigned>(imageIndex));
      texWidth = static_cast<int>(t.width);
      texHeight = static_cast<int>(t.height);
      glGenTextures(1, &texture);
      glBindTexture(GL_TEXTURE_2D, texture);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texWidth, texHeight, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, t.rgba.data());
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      if (glGetError() != GL_NO_ERROR)
        throw std::runtime_error("Could not create GPU texture");
      hasTexture = true;
      info += "\nOriginal-TPL " + std::to_string(texWidth) + "×" +
              std::to_string(texHeight) + " · GX-Format " +
              std::to_string(t.format);
    } catch (const std::exception &e) {
      info += "\n";
      info += e.what();
    }
    std::lock_guard<std::mutex> lock(stateMutex);
    graphics = info;
  } catch (const std::exception &e) {
    std::lock_guard<std::mutex> lock(stateMutex);
    graphics = std::string("Graphics error: ") + e.what();
    program = 0;
  }
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_resize(JNIEnv *, jclass, jint w, jint h) {
  width = w > 0 ? w : 1;
  height = h > 0 ? h : 1;
  glViewport(0, 0, width, height);
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_draw(JNIEnv *, jclass) {
  // All returns finish the already-rendered frame before the UI overlay.
  // The default is a no-op, so graphics settings never advance simulation.
  struct DisplayPass {
    bool confirmed{};
    ~DisplayPass() {
      try {re4::ApplyDisplayBrightnessGl(width,height,displayBrightness.load());}
      catch(const std::exception &e) {
        __android_log_print(ANDROID_LOG_WARN,"RE4Display","Brightness: %s",e.what());
      }
      if(deathRequested.load()) {
        std::lock_guard<std::mutex> lock(stateMutex);
        if(confirmed!=continueConfirmed.load())return;
        if(deathPicture.empty() && width>0 && height>0 && std::uint64_t(width)*height<=16777216) {
          std::vector<std::uint8_t> rgba(std::size_t(width)*height*4);
          glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
          if(glGetError()!=GL_NO_ERROR)return;
          deathPicture.resize(2+640*480);deathPicture[0]=640;deathPicture[1]=480;
          for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x) {
            const auto p=((height-1-y*unsigned(height)/480)*std::size_t(width)+x*unsigned(width)/640)*4;
            deathPicture[2+y*640+x]=jint(0xff000000u|(unsigned(rgba[p])<<16)|(unsigned(rgba[p+1])<<8)|rgba[p+2]);
          }
        }
      }
    }
  } displayPass;
  re4::MultisampleFrameGl multisampleFrame(width,height,re4::userDepthOfField);
  try {
  re4::InputFrame f;
  bool showNormals;
  bool inspector;
  bool useGreen;
  std::uint64_t pickupAnswer{};
  bool acceptPickup{};
  bool usePickup{};
  std::uint64_t generation;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    frame = input.consume();
    f = frame;
    showNormals = normalView;
    generation = inputGeneration;
    inspector = inspectorRequested;
    inspectorRequested = false;
    useGreen = useGreenRequested;
    useGreenRequested = false;
    pickupAnswer = pickupAnswerToken;
    acceptPickup = pickupAnswerAccepted;
    usePickup = pickupAnswerUse;
    pickupAnswerToken = 0;
  }
  if (sceneMode) {
    applyRoomCompletion();
    setSaveClockRunning(campaignMode && audioUiActive.load() && (!paused || pendingPickup.token));
    bool loading = transferPending();
    if(subscreenItemDirty)uploadSubscreenItem();
    if (pickupAnswer && !loading) completePickup(pickupAnswer, acceptPickup,usePickup);
    if (weaponGpuDirty && actorGpuReady && leon && leon->weapon()) {
      try {
        re4::CreateActorGl(*leon);
        re4::CreateWeaponGl(*leon->weapon());
        re4::DestroyMuzzleEffectsGl();
        muzzleEffects = (coreEnvironmentEff.empty() || !leon->weapon()->hasMuzzleEffects()) ? nullptr : std::make_unique<re4::MuzzleEffects>(
            re4::BuildWeaponWorldEffects(leon->weapon()->effects(),coreEnvironmentEff));
        muzzleWeaponId = leon->equippedWeaponId();
        if (muzzleEffects) re4::CreateMuzzleEffectsGl(muzzleEffects->data());
        weaponGpuDirty = false;
      }
      catch (const std::exception &e) {
        inventoryMessage = std::string("Weapon GPU: ") + e.what();
        __android_log_print(ANDROID_LOG_ERROR,"RE4Weapon","%s",inventoryMessage.c_str());
      }
    }
    if (useGreen) {
      if (loading) {
        std::lock_guard<std::mutex> lock(stateMutex);
        useGreenRequested = true;
      } else {
        // This is an explicit menu command. It works while Start has (paused || subscreenOpen)
        // simulation as well; no animation or enemy tick is advanced.
        useGreenHerb();
      }
    }
    // Diagnostics have an explicit UI command. X/Quadrat is now reload.
    if (!loading && !activeEvent && inspector) {
      const bool previous = walking;
      if (walking)
        walking = false;
      else if (walkProbe && walkProbe->ready())
        walking = true;
      else
        startWalking();
      try {
        uploadPreview();
      } catch (const std::exception &e) {
        walking = previous;
        flowInfo = std::string("View failed: ") + e.what();
        try {
          uploadPreview();
        } catch (...) {
          re4::DestroySceneGl();
        }
      }
      walkFrame = {};
      suspendSimulationInput();
    }
    if (!walking && !collisionView && previewSmd &&
        !previewPlacements.empty() && !loading &&
        (f.pressed & (re4::Knife | re4::Ashley | re4::Pause))) {
      try {
        if (f.pressed & re4::Pause)
          selectedPlacement = -1;
        else {
          auto it = std::find(previewPlacements.begin(),
                              previewPlacements.end(), selectedPlacement);
          int index = it == previewPlacements.end()
                          ? ((f.pressed & re4::Ashley) ? -1 : 0)
                          : int(it - previewPlacements.begin());
          index = (index + ((f.pressed & re4::Ashley) ? 1 : -1) +
                   int(previewPlacements.size())) %
                  int(previewPlacements.size());
          selectedPlacement = previewPlacements[index];
        }
        uploadPreview();
      } catch (const std::exception &e) {
        re4::DestroySceneGl();
        std::lock_guard<std::mutex> lock(stateMutex);
        graphics = std::string("3D error: ") + e.what();
      }
    }
    auto now = std::chrono::steady_clock::now();
    if (walking && walkProbe && walkProbe->ready()) {
      if(!loading && r100EventPhase>=10 && r100EventPhase!=13 && (f.pressed & re4::Pause)) {
        finishR100RoomScript();f.pressed=0;f.held=0;
      }
      const bool radioClose=r100EventPhase==13 && (f.pressed & re4::Pause);
      if(radioClose)f.pressed&=~re4::Pause;
      if(!loading && villageWaveCamera && (f.pressed & re4::Pause)) {finishVillageWave();f.pressed=0;f.held=0;}
      if(!loading && activeEvent && (f.pressed & re4::Pause)) {
        if(r100EventPhase>=3 && r100EventPhase<=5)re4::CancelOriginalOpening(scenarioState);
        activeEvent->finish();finishR100Event(true);f.pressed=0;f.held=0;
      }
      if (!loading && (f.pressed & re4::Pause)) {
        paused = !paused;
        suspendSimulationInput();
        __android_log_print(ANDROID_LOG_INFO, "RE4Player", "Pause: %s",
                            (paused || subscreenOpen) ? "ON" : "OFF");
      }
      setSaveClockRunning(campaignMode && audioUiActive.load() && (!paused || pendingPickup.token));
      double dt = walkFrame.time_since_epoch().count() == 0 ||
                          generation != seenInputGeneration || loading || (paused || subscreenOpen)
                      ? 0
                      : std::chrono::duration<double>(now - walkFrame).count();
      dt = std::clamp(dt, 0., .05);
      // ss_main.cpp calls Cckpt.move() while the inventory pauses the world.
      // Advance only its original 30-Hz clock; healing/pulsing must not depend
      // on display refresh or resume player/enemy simulation behind the menu.
      if(subscreenOpen && !loading && audioUiActive.load() &&
          walkFrame.time_since_epoch().count()!=0 && generation==seenInputGeneration) {
        cockpitUiRemainder+=std::clamp(std::chrono::duration<double>(now-walkFrame).count(),0.,.25)*30.;
        const auto uiTicks=unsigned(cockpitUiRemainder);
        cockpitTicks+=uiTicks;cockpitUiRemainder-=uiTicks;
        prepareSubscreenPortrait(uiTicks,cockpitUiRemainder);
      } else cockpitUiRemainder=0;
      if (loading || (paused || subscreenOpen) || generation != seenInputGeneration)
        suspendSimulationInput();
      bool eventStarted=false;
      if(campaignMode && activeRoomId==0x100 && !activeEvent && r100EventPhase<10 && !loading && !(paused || subscreenOpen) && leon && r100Scenario) {
        if(r100RadioQueued && leon->action()==re4::PlayerAction::Footwork) {beginR100Radio(*r100RadioQueued);eventStarted=true;}
        else if(r100DeathPending) {beginR100Event(2);eventStarted=true;}
        else if(!encounter && !scenarioState.room(0x100).room.test(3) &&
            !scenarioState.room(0x100).room.test(10) && leon->action()==re4::PlayerAction::Footwork &&
            r100Scenario->firstLookHit(leon->feet(),leon->heading())) {beginR100Event(1);eventStarted=true;}
        else if(r100Secondary && leon->action()==re4::PlayerAction::Footwork) {
          auto center=leon->feet();center[1]+=250;auto front=center;
          front[0]+=std::sin(leon->heading())*550;front[2]+=std::cos(leon->heading())*550;
          if(collisionWorld)if(const auto hit=collisionWorld->line(center,front,re4::SatSubset::All,0))front=hit->point;
          auto offer=r100Scenario->secondaryOffer(scenarioState.room(0x100),leon->feet(),leon->heading(),
              (f.pressed&re4::Interact)!=0,(f.held&re4::Interact)!=0,front,
              roomInteractions && roomInteractions->r100DoorMessageEnabled());
          if(encounter)if(const auto corpse=r100Secondary->corpseOffer(scenarioState.room(0x100),*encounter,
              leon->feet(),leon->heading(),(f.pressed&re4::Interact)!=0,front))
            if(!offer || r100Scenario->authoredArea(offer->area).priority<=1)offer=corpse;
          if(offer) {beginR100Secondary(*offer);eventStarted=true;}
        }
      }
      if(activeRoomId==0x101 && r100RadioQueued && !activeEvent && r100EventPhase<10 && !loading && !(paused || subscreenOpen) && leon && leon->action()==re4::PlayerAction::Footwork) {
        beginR100Radio(*r100RadioQueued);eventStarted=true;
      }
      if(villageScenario && !activeEvent && r100EventPhase<10 && !loading && !(paused || subscreenOpen) && leon && leon->action()==re4::PlayerAction::Footwork) {
        auto center=leon->feet();center[1]+=250;auto front=center;
        front[0]+=std::sin(leon->heading())*550;front[2]+=std::cos(leon->heading())*550;
        if(const auto hit=collisionWorld->line(center,front,re4::SatSubset::All,0))front=hit->point;
        if(const auto offer=villageScenario->offer(scenarioState,leon->feet(),leon->heading(),(f.pressed&re4::Interact)!=0,(f.held&re4::Interact)!=0,front)) {
          beginVillageEvent(*offer);eventStarted=true;
        }
      }
      std::optional<std::size_t> shelfOffer;
      bool shelfStarted=false;
      if(roomItemEvents && leon && leon->alive() && !activeEvent && r100EventPhase<10 && !loading && !(paused || subscreenOpen) &&
          leon->action()==re4::PlayerAction::Footwork && !(f.held & (re4::Aim|re4::Knife))) {
        auto center=leon->feet();center[1]+=250;auto front=center;
        front[0]+=std::sin(leon->heading())*550;front[2]+=std::cos(leon->heading())*550;
        if(const auto hit=collisionWorld->line(center,front,re4::SatSubset::All,0))front=hit->point;
        shelfOffer=roomItemEvents->offer(leon->feet(),leon->heading(),front);
        if(shelfOffer && (f.pressed & re4::Interact)) {
          shelfStarted=roomItemEvents->begin(*shelfOffer);shelfOffer.reset();
          if(shelfStarted){roomItemEventClock=0;eventUiActive.store(true);suspendSimulationInput();}
        }
      }
      const bool shelfFrame=roomItemEvents && roomItemEvents->active();
      const bool eventFrame=activeEvent!=nullptr || r100EventPhase>=10 || shelfFrame;
      if(shelfFrame && !shelfStarted) {
        roomItemEventClock+=dt*30.;
        while(roomItemEventClock>=1.) {
          roomItemEventClock-=1.;tickRoomAreaEffects(roomItemEvents->camera());roomItemEvents->tick30Hz();
          if(!roomItemEvents->active()) {roomItemEventClock=0;eventUiActive.store(false);suspendSimulationInput();break;}
        }
      }
      if(roomItemEvents && roomItemEvents->takeOpenSound())soundMixer.play(6,0x1a);

      if(r100RoomMessage && !r100EventPhase)
        r100MessageConfirm|=(f.pressed&(re4::Confirm|re4::Cancel))!=0;
      if(activeEvent && !eventStarted)advanceR100Event(dt);
      if(r100EventPhase>=10 && !eventStarted)advanceR100RoomScript(dt,
          (f.pressed&(r100EventPhase==13?re4::Confirm:(re4::Confirm|re4::Interact|re4::Fire)))!=0,
          (f.pressed & re4::Cancel)!=0,radioClose,&f);
      const bool kickContext = leon && (enemyGroup?enemyGroup->kickAvailable(*leon,encounter.get()):encounter && encounter->kickAvailable(*leon));
      double turn = f.lx, forward = f.ly;
      if (turn == 0)
        turn = ((f.held & re4::MenuRight) ? 1 : 0) -
               ((f.held & re4::MenuLeft) ? 1 : 0);
      if (forward == 0)
        forward = ((f.held & re4::MenuUp) ? 1 : 0) -
                  ((f.held & re4::MenuDown) ? 1 : 0);
      // Sample this frame's bounded C-stick look before the four-tick weapon
      // draw transfers it to Leon. Rendering must not advance it a second time.
      if (!eventFrame && !loading && !(paused || subscreenOpen) &&
          (!leon || leon->action() == re4::PlayerAction::Footwork))
        re4::SceneAdvanceFollowInput(f.rx, f.ry, dt);
      if (leon && !eventFrame && !loading && !(paused || subscreenOpen)) {
        r100WeaponDontFire=false;r100OfficerPressed=(f.pressed&(re4::Interact|re4::Fire))!=0;
        if(r100Secondary && leon->laserReady() && laserSightData) {
          const auto sight=re4::ResolveLaserSight(leon->laserRay(),effectWorld.get(),encounter.get(),objectActions.get(),roomInteractions.get(),laserSightData->maxLineLength,enemyGroup.get(),forestAnimals.get(),dogTraps.get(),villageAnimals.get());
          r100WeaponDontFire=re4::OriginalR100DontFire(sight.hitResult,sight.attributes,sight.targetDontFire,sight.distanceSquared);
        }
        if(!leon->weaponAttackImplemented() && (f.pressed&(re4::Aim|re4::Fire)))inventoryMessage="Weapon equipped. Firing is not available yet.";
        auto previous = leon->state();
        try {
          const auto oldAction = leon->action();
          // Original object ladders occupy ActBtn slot5. Window/fence
          // requests use slot2, so nearby windows must not mask a valid tip.
          const bool rescueStarted=dogTraps && (f.pressed & re4::Interact) && !(f.held & (re4::Aim|re4::Knife)) && dogTraps->beginRescue(*leon,*walkProbe);
          if(rescueStarted) {flowInfo="Free dog";re4::SceneSeedFollowView(0,0);}
          bool ladderStarted=false,windowStarted=false;
          const bool contextRequested=!rescueStarted && !kickContext && (f.pressed & re4::Interact) &&
              !(f.held & (re4::Aim | re4::Knife));
          std::optional<re4::LadderOffer> contextLadder;
          auto ladderFront=walkProbe->position();
          if(contextRequested && roomLadders) {
            auto center=walkProbe->position();center[1]+=250;ladderFront=center;
            ladderFront[0]+=std::sin(leon->heading())*550;
            ladderFront[2]+=std::cos(leon->heading())*550;
            if(const auto hit=collisionWorld->line(center,ladderFront,re4::SatSubset::All,0))
              ladderFront=hit->point;
            contextLadder=roomLadders->offer(*leon,*walkProbe,ladderFront);
          }
          const auto startLadder=[&] {
            ladderStarted=roomLadders->interact(*leon,*walkProbe,ladderFront);
            if(ladderStarted) {
              flowInfo=re4::PlayerActionLabel(leon->action());
              re4::SceneSeedFollowView(0,0);
              __android_log_print(ANDROID_LOG_INFO,"RE4Ladder","%s · %s",
                  roomName(activeRoomId).c_str(),flowInfo.c_str());
            }
          };
          if(contextLadder && contextLadder->object)startLadder();
          if(!ladderStarted && roomInteractions && contextRequested)
            if(const auto window=roomInteractions->windowOffer(*leon,*walkProbe)) {
              if (window->brokenFence)
                leon->beginBrokenWindow(*walkProbe,*window->brokenFence);
              else {
                leon->installWindowMotions(roomInteractions->windowMotions());
                leon->beginWindow(*walkProbe,*window);
              }
              windowStarted=true;
              flowInfo="Jump through window";re4::SceneSeedFollowView(0,0);
            }
          if(!windowStarted && !ladderStarted && contextLadder)startLadder();
          bool fenceStarted = false;
          if(!windowStarted && !ladderStarted && !kickContext && (f.pressed & re4::Interact) &&
              !(f.held & (re4::Aim | re4::Knife)))
            if(const auto fence=leon->fenceOffer(*walkProbe)) {
              leon->beginFence(*walkProbe,*fence);fenceStarted=true;flowInfo="Jump over obstacle";
              re4::SceneSeedFollowView(0,0);
            }
          bool jumpStarted = false;
          if (!windowStarted && !ladderStarted && !fenceStarted && !kickContext && (f.pressed & re4::Interact) && !(f.held & (re4::Aim | re4::Knife)))
            if(const auto jump=leon->jumpFallOffer(*walkProbe)) {
              leon->beginJumpFall(*walkProbe,*jump);jumpStarted=true;
              flowInfo="Jump over gap";re4::SceneSeedFollowView(0,0);
              __android_log_print(ANDROID_LOG_INFO,"RE4Jump","%s · Original long jump · SAT %zu",
                  roomName(activeRoomId).c_str(),jump->wall.polygon);
            }
          if (!windowStarted && !ladderStarted && !fenceStarted && !jumpStarted && !kickContext && (f.pressed & re4::Interact) && !(f.held & (re4::Aim | re4::Knife)))
            if (const auto jump=leon->jumpOffer(*walkProbe)) {
              const auto camera=re4::SceneFollowPose();
              leon->beginJump(*walkProbe,*jump,&camera); jumpStarted=true;
              flowInfo="Jump down";
              __android_log_print(ANDROID_LOG_INFO,"RE4Jump","%s · SAT edge %zu · Target Y %.1f",
                                  roomName(activeRoomId).c_str(),jump->wall.polygon,jump->landing[1]);
            }
          if (!windowStarted && !ladderStarted && !fenceStarted && !jumpStarted && roomInteractions && !kickContext &&
              (f.pressed & re4::Interact) && !(f.held & (re4::Aim | re4::Knife)))
            roomInteractions->interact(*leon, *walkProbe);
          const bool oldWeaponView = oldAction == re4::PlayerAction::Draw ||
              oldAction == re4::PlayerAction::Aim ||
              oldAction == re4::PlayerAction::Fire ||
              oldAction == re4::PlayerAction::Reload ||
              oldAction == re4::PlayerAction::Holster ||
              oldAction == re4::PlayerAction::KnifeDraw ||
              oldAction == re4::PlayerAction::KnifeReady ||
              oldAction == re4::PlayerAction::KnifeSlash ||
              oldAction == re4::PlayerAction::KnifeHolster;
          const auto oldLook = re4::SceneFollowState();
          double oldViewHeading = leon->heading();
          if (oldAction == re4::PlayerAction::Draw)
            oldViewHeading += leon->drawCameraYawRemaining();
          else if (oldAction != re4::PlayerAction::Footwork) {
            oldViewHeading += leon->waistYaw();
            if (oldAction == re4::PlayerAction::KnifeDraw ||
                oldAction == re4::PlayerAction::KnifeReady ||
                oldAction == re4::PlayerAction::KnifeSlash ||
                oldAction == re4::PlayerAction::KnifeHolster)
              oldViewHeading += oldLook.yawRatioRadians + oldLook.releaseYawRadians;
          }
          const double oldViewPitch = leon->aimRate();
          const bool wasLadder = leon->ladderActive();
          const bool wasObjectLadder = leon->objectLadderActive();
          const bool wasJump = leon->jumpActive();
          const bool wasJumpFall = leon->jumpFallActive();
          const auto oldLife = leon->life();
          const auto oldShots = leon->shots(), oldReserve = leon->reserve();
          re4::PlayerActionInput action;
          action.aimHeld = (f.held & re4::Aim) != 0;
          action.aimPressed = (f.pressed & re4::Aim) != 0;
          action.fireHeld = !r100WeaponDontFire && (f.held & re4::Fire) != 0;
          action.firePressed = !r100WeaponDontFire && (f.pressed & re4::Fire) != 0;
          action.reloadPressed = (f.pressed & re4::Reload) != 0;
          action.knifeHeld = (f.held & re4::Knife) != 0;
          action.knifePressed = (f.pressed & re4::Knife) != 0;
          action.interactPressed = !rescueStarted && (f.pressed & re4::Interact) != 0;
          const auto look = re4::SceneFollowState();
          action.hasCameraLook = true;
          action.cameraYaw = look.yawRatioRadians + look.releaseYawRadians;
          action.cameraPitch = look.verticalRatio;
          action.yaw = f.rx;
          action.pitch = aimInverted.load() ? -f.ry : f.ry;
          if(gyroEnabled.load() && action.aimHeld && leon->action()==re4::PlayerAction::Aim) {
            std::lock_guard<std::mutex> lock(gyroMutex);
            if(std::chrono::steady_clock::now()-gyroSample<std::chrono::milliseconds(150)) {
              action.gyroYaw=gyroYawRate;action.gyroPitch=gyroPitchRate;
            }
          }
          action.scopeZoom=double(bool(f.held&re4::MenuUp))-double(bool(f.held&re4::MenuDown));
          action.resistancePresses = f.resistancePresses;
          const auto worldBefore = [&] {
            beforeWorldTick(forward, !action.aimHeld && !action.knifeHeld);
          };
          if(enemyGroup) {
            enemyGroup->stepPlayer(*leon,*walkProbe,turn,forward,(f.held&re4::Run)!=0,
                (f.pressed&re4::Run)!=0,dt,action,worldBefore,afterWorldTick,encounter.get());
          } else if (encounter) {
            const auto oldHits = encounter->hits(), oldHp = encounter->hp();
            const auto oldGrabs = encounter->grabs(),
                       oldEscapes = encounter->escapes();
            const auto oldKnifeHits = encounter->knifeHits();
            const auto oldKickHits = encounter->kickHits();
            const auto oldEnemyMotion = encounter->actor().motion();
            const bool oldKickAvailable = encounter->kickAvailable(*leon);
            encounter->stepPlayer(*leon, *walkProbe, turn, forward,
                                  (f.held & re4::Run) != 0,
                                  (f.pressed & re4::Run) != 0, dt, action,
                                  worldBefore, afterWorldTick);
            if (oldHits != encounter->hits() || oldHp != encounter->hp() ||
                oldGrabs != encounter->grabs() ||
                oldEscapes != encounter->escapes() ||
                oldKnifeHits != encounter->knifeHits() ||
                oldKickHits != encounter->kickHits() ||
                oldEnemyMotion != encounter->actor().motion() ||
                oldKickAvailable != encounter->kickAvailable(*leon))
              __android_log_print(
                  ANDROID_LOG_INFO, "RE4Enemy", "%s · Kick prompt %s",
                  encounter->status().c_str(),
                  encounter->kickAvailable(*leon) ? "ON" : "OFF");
          } else
            leon->step(*walkProbe, turn, forward, (f.held & re4::Run) != 0,
                       (f.pressed & re4::Run) != 0, dt, action,
                       [&](const re4::PlayerShot *shot) {
                         afterWorldTick(shot);
                       }, worldBefore);
          if (previous != leon->state())
            __android_log_print(ANDROID_LOG_INFO, "RE4Player", "Animation: %s",
                                re4::LocomotionLabel(leon->state()));
          if (wasJump && !leon->jumpActive()) {
            const auto failed=wasJumpFall?leon->jumpFallState().failed:leon->jumpState().failed;
            const auto tickCount=wasJumpFall?leon->jumpFallState().ticks:leon->jumpState().ticks;
            flowInfo=failed?"Landing blocked · Returning to start":"Jump complete";
            __android_log_print(ANDROID_LOG_INFO,"RE4Jump","%s · %s · Ticks %u",
                                roomName(activeRoomId).c_str(),flowInfo.c_str(),tickCount);
          }
          if (wasLadder && !leon->ladderActive()) {
            flowInfo = leon->ladderLandingFailed() ? "Ladder destination blocked · Safely returning to start"
                                                 : "Ladder finished · Actual SAT floor confirmed";
            __android_log_print(ANDROID_LOG_INFO, "RE4Ladder", "%s · %s · Ticks %u",
                                roomName(activeRoomId).c_str(), flowInfo.c_str(),
                                wasObjectLadder ? leon->objectLadderState().ticks : leon->ladderState().ticks);
          }
          if (oldWeaponView &&
              leon->action() == re4::PlayerAction::Footwork)
            re4::SceneSeedFollowView(oldViewHeading - leon->heading(), oldViewPitch);
          if (oldLife != leon->life() || oldAction != leon->action())
            __android_log_print(ANDROID_LOG_INFO, "RE4Player", "HP %u/%u · %s",
                                leon->life(), re4::LeonPreview::MaxLife,
                                re4::PlayerActionLabel(leon->action()));
          if (oldAction != leon->action() || oldShots != leon->shots() ||
              oldReserve != leon->reserve())
            __android_log_print(
                ANDROID_LOG_INFO, "RE4Weapon",
                "%s · Ammo %u/%u + %u · Shots %u · FCV %.0f",
                re4::PlayerActionLabel(leon->action()), leon->magazine(),
                leon->weapon()->capacity(), leon->reserve(), leon->shots(), leon->rig().curveFrame());
        } catch (const std::exception &e) {
          __android_log_print(ANDROID_LOG_ERROR, "RE4Player", "%s", e.what());
          playerInfo = "Simulation paused: " + std::string(e.what());
          flowInfo=playerInfo;
          paused=true;
          suspendSimulationInput();
        }
      } else if (!leon && !loading && !(paused || subscreenOpen))
        walkProbe->step(turn, forward, re4::SceneCameraYaw(),
                        (f.held & re4::Run) != 0, dt);
      re4::DoorSelection door;
      if (leon && !eventFrame && roomFlow && !loading && !(paused || subscreenOpen) &&
          leon->action() == re4::PlayerAction::Footwork) {
        auto center = walkProbe->position();
        center[1] += 250;
        auto front = center;
        front[0] += std::sin(leon->heading()) * 550;
        front[2] += std::cos(leon->heading()) * 550;
        if (auto hit =
                collisionWorld->line(center, front, re4::SatSubset::All, 0))
          front = hit->point;
        door = roomFlow->select(walkProbe->position(), leon->heading(),
                                (f.pressed & re4::Interact) != 0,
                                (f.held & re4::Interact) != 0, front,&scenarioState,activePart,roomEntryTicks);
        if(door.door && (f.pressed&re4::Interact) && re4::UnlockVillageDoor(*door.door,scenarioState)) {
          if(door.door->sourceRoom==0x11e && door.door->no==1)inventoryCase.consume(0x8b,1);
          flowInfo="You unlocked the door.";door.activated=false;door.available=true;
        }
        if (door.activated && door.available && door.door) {
          requestRoom(door.door->destinationRoom, door.door->destinationPart,
                      door.door->sourceRoom==door.door->destinationRoom && door.door->destinationPart?leon->feet():door.door->destination,
                      door.door->sourceRoom==door.door->destinationRoom && door.door->destinationPart?leon->heading():static_cast<float>(door.door->destinationHeading));
          loading = true;
          suspendSimulationInput();
        } else if (door.door && !door.available &&
                   (f.pressed & re4::Interact)) {
          flowInfo = door.reason;
          __android_log_print(ANDROID_LOG_INFO, "RE4Flow", "%s door %u: %s",
                              roomName(activeRoomId).c_str(), door.door->no,
                              flowInfo.c_str());
        }
      }
      std::optional<re4::FenceOffer> fenceOffer;
      if(leon && !eventFrame && !loading && !(paused || subscreenOpen) && !kickContext)fenceOffer=leon->fenceOffer(*walkProbe);
      if(campaignSceneDirty) {
        if(scenarioCollision)scenarioCollision->sync(scenarioState.room(activeRoomId),collisionWorld.get(),effectWorld.get());
        if(roomPickups && pickupArchive) {
          previewLooseItems=std::make_unique<re4::RoomItemLayer>(re4::BuildRoomItems(*pickupArchive,roomPickups->renderPlacements()));
          for(auto &draw:previewLooseItems->mesh.draws) {draw.roomPickupIndex=draw.roomItemIndex;draw.roomItemIndex=-1;}
        }
        campaignSceneDirty=false;
        uploadPreview(false,true);
      }
      std::optional<re4::JumpDownOffer> jumpOffer;
      std::optional<re4::JumpFallOffer> jumpFallOffer;
      if (leon && !eventFrame && !loading && !(paused || subscreenOpen) && !kickContext) jumpOffer=leon->jumpOffer(*walkProbe);
      if (leon && !eventFrame && !loading && !(paused || subscreenOpen) && !kickContext) jumpFallOffer=leon->jumpFallOffer(*walkProbe);
      std::optional<re4::LadderOffer> ladderOffer;
      if (roomLadders && leon && !eventFrame && !loading && !(paused || subscreenOpen) && !kickContext &&
          leon->action() == re4::PlayerAction::Footwork) {
        auto center = walkProbe->position(); center[1] += 250;
        auto front = center;
        front[0] += std::sin(leon->heading()) * 550;
        front[2] += std::cos(leon->heading()) * 550;
        if (const auto hit = collisionWorld->line(center, front, re4::SatSubset::All, 0))
          front = hit->point;
        ladderOffer = roomLadders->offer(*leon, *walkProbe, front);
      }
      std::optional<re4::InteriorDoorOffer> interiorDoor;
      std::optional<re4::InteriorPushOffer> interiorPush;
      std::optional<re4::InteriorWindowOffer> windowOffer;
      if (roomInteractions && leon && !eventFrame && !loading && !(paused || subscreenOpen) && !kickContext)
        windowOffer = roomInteractions->windowOffer(*leon,*walkProbe);
      if (roomInteractions && leon && !eventFrame && !loading && !(paused || subscreenOpen) &&
          leon->action() == re4::PlayerAction::Footwork)
        interiorDoor = roomInteractions->doorOffer(leon->feet(), leon->heading());
      if (roomInteractions && leon && !eventFrame && !loading && !(paused || subscreenOpen) &&
          !(f.held & (re4::Aim | re4::Knife)) &&
          leon->action() == re4::PlayerAction::Footwork)
        interiorPush = roomInteractions->pushOffer(*leon, *walkProbe);
      constexpr std::size_t NoPickup = std::size_t(-1);
      std::size_t pickupIndex = NoPickup, loosePickupIndex = NoPickup;
      bool atTypewriter=false;
      if ((objectActions || roomPickups) && leon && leon->alive() && !eventFrame && !loading && !(paused || subscreenOpen) &&
          leon->action() == re4::PlayerAction::Footwork && !door.door &&
          !interiorDoor && !ladderOffer && !fenceOffer && !jumpOffer && !jumpFallOffer && !windowOffer && !kickContext) {
        auto center = walkProbe->position();
        center[1] += 250;
        auto front = center;
        front[0] += std::sin(leon->heading()) * 550;
        front[2] += std::cos(leon->heading()) * 550;
        if (auto hit = collisionWorld->line(center, front, re4::SatSubset::All, 0))
          front = hit->point;
        if (objectActions) pickupIndex = objectActions->pickupTarget(
            walkProbe->position(), leon->heading(), front).value_or(NoPickup);
        if (roomPickups) loosePickupIndex = roomPickups->target(
            walkProbe->position(), leon->heading(), front).value_or(NoPickup);
        if (pickupIndex != NoPickup && loosePickupIndex != NoPickup) {
          const auto &boxItem = objectActions->objects()[pickupIndex].item;
          const auto &looseItem = roomPickups->items()[loosePickupIndex].trigger;
          if (looseItem.priority > boxItem.raw[0x44] ||
              (looseItem.priority == boxItem.raw[0x44] && looseItem.recordIndex < boxItem.recordIndex))
            pickupIndex = NoPickup;
          else loosePickupIndex = NoPickup;
        }
        if(campaignMode && roomFlow && !(f.held&(re4::Aim|re4::Knife))) {
          const re4::RoomDoor *selected=nullptr;
          for(const auto &area:roomFlow->typewriters()) {
            if(area.parentPointer || area.functionPointer || area.parameterPointer || area.waitingType || area.country ||
                !re4::RoomFlow::hit(area,walkProbe->position(),leon->heading(),front))continue;
            if(!selected || area.priority>selected->priority || (area.priority==selected->priority && area.recordIndex<selected->recordIndex))selected=&area;
          }
          const re4::RoomDoor saveArea=selected?*selected:re4::RoomDoor{};
          atTypewriter=selected!=nullptr;
          if(atTypewriter && loosePickupIndex!=NoPickup) {
            const auto &area=roomPickups->items()[loosePickupIndex].trigger;
            if(area.priority>saveArea.priority || (area.priority==saveArea.priority && area.recordIndex<saveArea.recordIndex))atTypewriter=false;
          }
          if(atTypewriter && pickupIndex!=NoPickup) {
            const auto &area=objectActions->objects()[pickupIndex].item;
            if(area.raw[0x44]>saveArea.priority || (area.raw[0x44]==saveArea.priority && area.recordIndex<saveArea.recordIndex))atTypewriter=false;
          }
        }
        if(atTypewriter && (f.pressed & re4::Interact)) {
          saveMenuRequested.store(true);subscreenOpen=true;suspendSimulationInput();
          pickupIndex=loosePickupIndex=NoPickup;
        } else if (loosePickupIndex != NoPickup && (f.pressed & re4::Interact)) {
          const auto &item = roomPickups->items().at(loosePickupIndex);
          beginPickup(true, loosePickupIndex, item.id, item.count);
          loosePickupIndex = NoPickup;
        } else if (pickupIndex != NoPickup && (f.pressed & re4::Interact)) {
          const auto &item = objectActions->states().at(pickupIndex);
          beginPickup(false, pickupIndex, item.itemId, item.itemCount);
          pickupIndex = NoPickup;
        }
      }
      re4::SceneProbe visual;
      visual.objects = objectActions.get();
      visual.interactions = roomInteractions.get();
      visual.ladders = roomLadders.get();
      visual.enemyEffectsData = enemyEffectsData.get();
      visual.enemyEffects = enemyEffects.get();
      visual.trapEffects = trapEffectsData.get();
      visual.trapState = trapEffects ? &trapEffects->state() : nullptr;
      visual.roomAreaEffects = roomAreaEffects.get();
      visual.roomHitData=roomHitEffects?&roomHitEffects->data():nullptr;
      visual.roomHitState=roomHitEffects?&roomHitEffects->state():nullptr;
      visual.pickups = roomPickups.get();visual.itemEvents=roomItemEvents.get();
      syncItemEffects();visual.itemEffects=itemEffects.get();
      visual.destruction = destruction.get();
      visual.muzzle = muzzleEffects.get();
      visual.coreLighting = coreLighting.get();
      visual.followInputAdvanced = true;
      visual.enemy = actorGpuReady ? encounter.get() : nullptr;
      visual.event=activeEvent;
      visual.r100Secondary=r100Secondary.get();
      visual.roomMessage=r100RoomMessage?&r100RoomMessage->message():nullptr;
      messageUiActive.store(r100RoomMessage && !r100RoomMessage->finished());
      if(r100EventPhase==12 && villageRadioIntro && villageRadioIntro->visible()) {
        visual.radioActor=&villageRadioIntro->actor();visual.radioMatrix=villageRadioIntro->matrix(*leon);
      }
      visual.roomCamera=r100Secondary && r100Secondary->cameraActive()?&r100Secondary->camera():nullptr;
      if(villageBinocular)visual.roomCamera=&villageBinocular->camera();
      visual.villageBottles=villageBottles.get();
      visual.projectiles=playerProjectiles.get();
      if(playerProjectiles && projectileGpuGeneration!=playerProjectiles->modelGeneration()) {
        for(const auto &entry:playerProjectiles->models()) {
          const auto &model=*entry.second;re4::CreateGenericActorGl(&model,model.mesh(),model.skin());
        }
        re4::CreateProjectileEffectsGl(playerProjectiles->effectsData());
        projectileGpuGeneration=playerProjectiles->modelGeneration();
      }
      if(villageWaveCamera) {
        visual.roomCamera=&villageCameras.at(*villageWaveCamera)->camera();visual.roomLightCut=*villageWaveCamera+4;
        visual.noSuspendEnemies=villageWaveActors;
      }
      if(r100Secondary)visual.roomLightCut=r100Secondary->cameraCut();
      visual.houseEnemy=r100EventPhase==10?r100HouseEnemy.get():nullptr;
      visual.sourceSuspended=(r100EventPhase>=10 && r100EventPhase!=16) || shelfFrame;
      if(roomItemEvents && roomItemEvents->active()) {
        visual.roomCamera=roomItemEvents->camera();visual.roomLightCut=roomItemEvents->lightArea();
      }
      visual.radioHand=r100EventPhase==12 && r100Secondary && r100Secondary->radioHandVisible();
      // Ordinary forced room cameras do not give Leon NoSuspend; source
      // ModelTrans omits him while these scenario tasks own the camera.
      visual.hidePlayer=r100EventPhase==10 || visual.roomCamera;
      visual.enemies=actorGpuReady && enemyGroup ? enemyGroup.get():nullptr;
      visual.collision = collisionWorld.get();
      visual.feet = walkProbe->position();
      if(previewBlocks)visual.roomBlocks=previewBlocks->visibleMask(visual.feet);
      visual.radius = walkProbe->config().radius;
      visual.height = walkProbe->config().height;
      visual.blocked =
          walkProbe->wallHit().has_value() || walkProbe->edgeBlocked();
      visual.showBody = !leon || !actorGpuReady || collisionView;
      visual.groundTriangle =
          collisionWorld->triangle(walkProbe->floorHit()->polygon);
      const bool weaponActive =
          leon && leon->action() != re4::PlayerAction::Footwork;
      // Original handgun ready requests the laser; fire does not renew that
      // per-frame request. Never draw the recoiling hand's upward ray as an
      // ongoing aiming laser. Real shot events still use the solved fire pose.
      if (leon && leon->laserReady() && laserSightData) {
        const auto sight = re4::ResolveLaserSight(
            leon->laserRay(), effectWorld.get(), encounter.get(), objectActions.get(),
            roomInteractions.get(), laserSightData->maxLineLength, enemyGroup.get(),forestAnimals.get(),dogTraps.get(),villageAnimals.get());
        visual.showAimRay = sight.line;
        visual.rayStart = sight.lineStart;
        visual.rayEnd = sight.lineEnd;
        visual.showAimPoint = sight.point;
        visual.aimPoint = sight.target;
      }
      visual.animals = actorGpuReady ? ambientAnimals.get() : nullptr;
      visual.forest=actorGpuReady?forestAnimals.get():nullptr;
      visual.villageAnimals=actorGpuReady?villageAnimals.get():nullptr;
      visual.dogTraps=actorGpuReady?dogTraps.get():nullptr;
      if (ambientAnimals && !eventFrame && !(paused || subscreenOpen) && !loading)
        ambientAnimals->step(dt);
      audioGameActive.store((deathRequested.load() || !(paused || subscreenOpen)) && !loading);
      syncAudioPause();
      displayPass.confirmed=continueConfirmed.load();
      if(displayPass.confirmed) {
        // gameDiedemo step4 sets DPF_PL and DPF_OBJ; terrain/enemies remain.
        visual.hidePlayer=true;visual.objects=nullptr;visual.interactions=nullptr;
        visual.ladders=nullptr;visual.pickups=nullptr;visual.itemEffects=nullptr;
      }
      visual.gameplayDepthOfField=!eventFrame && !(paused || subscreenOpen) && !loading && !deathRequested.load();
      re4::DrawSceneGl(
          width, height, ((paused || subscreenOpen) || loading || weaponActive) ? 0 : f.rx,
          ((paused || subscreenOpen) || loading || weaponActive) ? 0 : f.ry, 0, showNormals,
          &visual, actorGpuReady ? leon.get() : nullptr, dt);
      if(r100EventPhase==13 || r100EventPhase==14) {
        if(r100RadioTerm)re4::DrawRadioTermGl(width,height,*r100RadioTerm);
      }
      if(r100RadioFadeFrame) {
        const auto frame=*r100RadioFadeFrame;
        if(frame<3)re4::DrawEventFadeGl(width,height,255*(3-frame)/3);
        if(frame<10)re4::DrawEventFadeGl(width,height,255*(10-frame)/10);
      }
      updatePyreAudio();
      // SndEventInit sets SPF_SE_CALC: SndWatcher holds the previous
      // spatial parameters through these EVDs. Re-evaluating a dying voice
      // against the truck camera made its remaining samples inaudible.
      const bool spatialAudio=!subscreenOpen &&
          (!activeEvent || (activeEvent->data().soundFlags()&0x80000000u));
      if(spatialAudio && encounter && roomAcoustics && leon) {
        const auto camera=re4::SceneFollowPose();
        for(unsigned channel=0;channel<enemyVoiceHandles.size();++channel)if(const auto handle=enemyVoiceHandles[channel])
          soundMixer.updateSpatial(handle,*roomAcoustics,encounter->soundPosition(re4::EnemySoundChannel(channel)),leon->feet(),camera.eye,camera.right);
      }
      if(spatialAudio && enemyGroup && roomAcoustics && leon) {
        const auto camera=re4::SceneFollowPose();
        for(const auto &enemy:enemyGroup->actors()) {
          const auto owner=groupVoiceHandles.find(enemy.get());
          if(owner!=groupVoiceHandles.end())for(unsigned channel=0;channel<owner->second.size();++channel)if(const auto handle=owner->second[channel])
            soundMixer.updateSpatial(handle,*roomAcoustics,enemy->soundPosition(re4::EnemySoundChannel(channel)),leon->feet(),camera.eye,camera.right);
        }
      }
      if(subscreenOpen && subscreenItemId>=0) {
        const auto elapsed=subscreenFrame.time_since_epoch().count()?std::chrono::duration<double>(now-subscreenFrame).count():0.;
        re4::DrawSubscreenItemGl(width,height,elapsed,f.rx,f.ry);subscreenFrame=now;
      }
      auto pos = walkProbe->position();
      std::ostringstream info;
      info << std::fixed << std::setprecision(0) << "M37 · "
           << roomName(activeRoomId) << ((paused || subscreenOpen) ? " · Pause" : "")
           << " · Floor #" << walkProbe->floorHit()->polygon << " · Y "
           << pos[1] << " · "
           << (walkProbe->edgeBlocked() ? "Edge blocked"
               : walkProbe->wallHit()   ? "Wall contact"
                                        : "clear");
      if (leon)
        info << "\n"
             << (weaponActive ? re4::PlayerActionLabel(leon->action())
                              : re4::LocomotionLabel(leon->state()))
             << " · Ammo " << leon->magazine() << "/" << leon->weapon()->capacity() << " + "
             << leon->reserve() << (leon->unlimitedHealth()?" · Leon HP ∞ (test mode)":" · Leon HP "+std::to_string(leon->life())+"/"+std::to_string(re4::LeonPreview::MaxLife));
      else if (!playerInfo.empty())
        info << "\n" << playerInfo;
      if (encounter)
        info << "\n" << encounter->status();
      if (loading)
        info << "\n" << flowInfo;
      else if (!(paused || subscreenOpen) && leon && encounter && encounter->kickAvailable(*leon))
        info << "\nA/Cross: Kick";
      else if (windowOffer && !(ladderOffer && ladderOffer->object))
        info << "\nA/Cross: Jump through window";
      else if (ladderOffer)
        info << (ladderOffer->action==re4::LadderAction::Tip?"\nA/Cross: Topple ladder":
            ladderOffer->action==re4::LadderAction::Reset?"\nA/Cross: Raise ladder":
            ladderOffer->down?"\nA/Cross: Climb down ladder":"\nA/Cross: Climb ladder");
      else if (jumpFallOffer)
        info << "\nA/Cross: Jump over gap";
      else if (jumpOffer)
        info << "\nA/Cross: Jump down";
      else if (interiorDoor)
        info << (interiorDoor->kick ? "\nA/Cross: Kick door" : "\nA/Cross: Open door");
      else if (door.door && door.available)
        info << "\nA/Cross: Door to " << roomName(door.door->destinationRoom);
      else if (door.door)
        info << "\n" << door.reason;
      else if (pickupIndex != NoPickup)
        info << "\nA/Cross: " << itemName(objectActions->states()[pickupIndex].itemId)
             << " pick up";
      else if (loosePickupIndex != NoPickup)
        info << "\nA/Cross: " << itemName(roomPickups->items()[loosePickupIndex].id) << " pick up";
      else if (interiorPush)
        info << "\nHold LS forward: Push furniture";
      else if (!flowInfo.empty())
        info << "\n" << flowInfo;
      else if (leon && leon->knifeActive())
        info << "\nLB/L1: Hold knife · RT/R2: Attack · RS: Aim";
      else if (leon && leon->aiming())
        info << "\nRS: Aim · RT/R2: Fire · X/Square: Reload";
      else if (leon && leon->alive() && !leon->grabbed())
        info << "\nLT/L2: Aim · LB/L1: Hold knife · RT/R2: Attack";
      std::ostringstream hud;
      if (leon)
        hud << (leon->unlimitedHealth()?"HP ∞":"HP "+std::to_string(leon->life())+"/"+std::to_string(re4::LeonPreview::MaxLife))
            << " · Ammo " << leon->magazine() << "/" << leon->weapon()->capacity() << " + "
            << leon->reserve();
      if (loading)
        hud << "\nLoading room";
      else if ((paused || subscreenOpen))
        hud << "\nPause · Start: Resume";
      else if (leon && !leon->alive())
        hud << "\nLeon defeated";
      else if (leon && encounter && encounter->kickAvailable(*leon))
        hud << "\nA/Cross: Kick";
      else if (leon && (leon->doorActive() || leon->ladderActive() || leon->jumpActive() || leon->windowActive() || leon->pushActive() || leon->fenceActive() || leon->ladderPropActive()))
        hud << "\n" << re4::PlayerActionLabel(leon->action());
      else if (windowOffer && !(ladderOffer && ladderOffer->object))
        hud << "\nA/Cross: Jump through window";
      else if (ladderOffer)
        hud << (ladderOffer->action==re4::LadderAction::Tip?"\nA/Cross: Topple ladder":
            ladderOffer->action==re4::LadderAction::Reset?"\nA/Cross: Raise ladder":
            ladderOffer->down?"\nA/Cross: Climb down ladder":"\nA/Cross: Climb ladder");
      else if (fenceOffer)
        hud << "\nA/Cross: Jump over obstacle";
      else if (jumpFallOffer)
        hud << "\nA/Cross: Jump over gap";
      else if (jumpOffer)
        hud << "\nA/Cross: Jump down";
      else if (interiorDoor)
        hud << (interiorDoor->kick ? "\nA/Cross: Kick door" : "\nA/Cross: Open door");
      else if (door.door && door.available)
        hud << "\nA/Cross: Door to " << roomName(door.door->destinationRoom);
      else if (pickupIndex != NoPickup)
        hud << "\nA/Cross: " << itemName(objectActions->states()[pickupIndex].itemId)
            << " pick up";
      else if (loosePickupIndex != NoPickup)
        hud << "\nA/Cross: " << itemName(roomPickups->items()[loosePickupIndex].id) << " pick up";
      else if (interiorPush)
        hud << "\nHold LS forward: Push furniture";
      else if (leon && leon->action() == re4::PlayerAction::Grabbed)
        hud << "\nBreak free: A/Cross, B/Circle or left stick";
      else if (leon && leon->knifeActive())
        hud << "\nLB/L1: Hold knife · RT/R2: Attack";
      else if (leon && leon->aiming())
        hud << "\nRS: Aim · RT/R2: Fire · X/Square: Reload";
      if (!inventoryMessage.empty()) hud << "\n" << inventoryMessage;
      publishCase();
      int actionKind=-1,actionButton=1;
      if(leon && leon->alive() && !loading && !paused && !subscreenOpen && !eventFrame) {
        if(kickContext)actionKind=7;
        else if(leon->action()==re4::PlayerAction::Grabbed){actionKind=43;actionButton=4;}
        else if(leon->action()==re4::PlayerAction::Footwork) {
          if(windowOffer && !(ladderOffer && ladderOffer->object))
            actionKind=windowOffer->brokenFence?5:windowOffer->direction==1?3:2;
          else if(ladderOffer)actionKind=ladderOffer->action==re4::LadderAction::Tip?10:
              ladderOffer->action==re4::LadderAction::Reset?11:ladderOffer->down?9:8;
          else if(fenceOffer || jumpFallOffer)actionKind=5;
          else if(jumpOffer)actionKind=4;
          else if(shelfOffer || interiorDoor || (door.door && door.available))actionKind=16;
          else if(pickupIndex!=NoPickup || loosePickupIndex!=NoPickup)actionKind=40;
          else if(interiorPush){actionKind=6;actionButton=0x10;}
          if(dogTraps && dogTraps->rescueAvailable(*leon))actionKind=21;
          if(r100Scenario && r100Secondary) {
            auto center=leon->feet();center[1]+=250;auto front=center;
            front[0]+=std::sin(leon->heading())*550;front[2]+=std::cos(leon->heading())*550;
            if(collisionWorld)if(const auto hit=collisionWorld->line(center,front,re4::SatSubset::All,0))front=hit->point;
            // Preview the same pure source AEV predicate; it does not accept
            // the action or mutate flags. SceAtRoomSet assigns MDT defaults.
            if(const auto offer=r100Scenario->secondaryOffer(scenarioState.room(0x100),leon->feet(),leon->heading(),true,true,front,
                roomInteractions && roomInteractions->r100DoorMessageEnabled())) {
              const auto &area=r100Scenario->authoredArea(offer->area);
              actionKind=area.raw[0x35]==5 && !(area.raw[0x38]&8)?1:area.raw[0x4a];
              if(actionKind>=48)actionKind=-1;
            }
            if(encounter && r100Secondary->corpseOffer(scenarioState.room(0x100),*encounter,leon->feet(),leon->heading(),true,front))actionKind=1;
          }
          if(atTypewriter)actionKind=47;
          if(villageScenario) {
            auto center=leon->feet();center[1]+=250;auto front=center;
            front[0]+=std::sin(leon->heading())*550;front[2]+=std::cos(leon->heading())*550;
            if(collisionWorld)if(const auto hit=collisionWorld->line(center,front,re4::SatSubset::All,0))front=hit->point;
            if(villageScenario->binocularAvailable(scenarioState,leon->feet(),leon->heading(),front))actionKind=13;
          }
        }
      }
      {
        std::lock_guard<std::mutex> lock(stateMutex);
        graphics = info.str();
        hudRoomName=roomName(activeRoomId);
        eventTitleValues=activeEvent && villageEvent==re4::VillageEvent::Bell
            ?std::array<jint,3>{activeEvent->sourceFrame()>=1100?1:0,
                jint(std::max(0,int(activeEvent->sourceFrame())-1100)),1}:std::array<jint,3>{};
        binocularValues=villageBinocular?std::array<jint,4>{1,jint(std::lround(villageBinocular->fov()*1000)),jint(std::lround(villageBinocular->heading()*1000000)),jint(villageBinocular->ticks())}:std::array<jint,4>{};
        scopeValues=leon && leon->scopeReady() && !loading && !paused && !subscreenOpen && !eventFrame
            ?std::array<jint,4>{jint(leon->equippedWeaponId()),jint(cockpitTicks),jint(std::lround(leon->scopeCamera().fovDegrees*1000)),jint(std::lround(leon->scopeZoom()*1000000))}
            :std::array<jint,4>{};
        hudInfo = hud.str();
        cockpitValues={leon?jint(leon->life()):0,jint(re4::LeonPreview::MaxLife),
            leon?jint(leon->magazine()):0,leon?jint(leon->reserve()):0,
            leon?jint(leon->equippedWeaponId()):0,actionKind,actionButton,jint(cockpitTicks),jint(cockpitEpoch),
            jint(activePartnerId),jint(originalSaveMetadata.ashleyLife),jint(originalSaveMetadata.ashleyMaxLife),
            jint(activePartnerCondition),jint(originalSaveMetadata.playerType)};
        inventoryInfo = suppliesText();
      }
      if (std::chrono::duration<double>(now - walkLog).count() > 1 &&
          (turn != 0 || forward != 0)) {
        logWalk();
        walkLog = now;
      }
    } else {
      audioGameActive.store(false);
      syncAudioPause();
      re4::DrawSceneGl(width, height, f.rx, f.ry, f.lt - f.rt, showNormals);
      std::lock_guard<std::mutex> lock(stateMutex);
      hudInfo.clear();
      inventoryInfo = suppliesText();
    }
    walkFrame = now;
    seenInputGeneration = generation;
    return;
  }
  audioGameActive.store(false);
  syncAudioPause();
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_SCISSOR_TEST);
  glClearColor(.035f, .055f, .075f, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  if (program && hasTexture) {
    glUseProgram(program);
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    const float aspect = static_cast<float>(width) / height,
                ta = static_cast<float>(texWidth) / texHeight;
    const float sx = ta > aspect ? .8f : .8f * ta / aspect,
                sy = ta > aspect ? .8f * aspect / ta : .8f;
    glUniform2f(glGetUniformLocation(program, "scale"), sx, sy);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  // Two stick markers are independent of the texture preview and of game
  // simulation.
  const float axes[] = {f.lx, f.ly, f.rx, f.ry};
  glEnable(GL_SCISSOR_TEST);
  for (int i = 0; i < 2; i++) {
    const int x = static_cast<int>(width * (i == 0 ? .25f : .75f) +
                                   axes[i * 2] * width * .18f);
    const int y =
        static_cast<int>(height * .15f + axes[i * 2 + 1] * height * .1f);
    glScissor(x - 5, y - 5, 10, 10);
    glClearColor(i == 0 ? .95f : .3f, i == 0 ? .65f : .8f, .25f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  glDisable(GL_SCISSOR_TEST);
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    hudInfo.clear();
  }
  } catch(const std::exception &error) {
    __android_log_print(ANDROID_LOG_ERROR,"RE4Scene","Rendering paused: %s",error.what());
    if(activeEvent) {
      if(r100EventPhase==1)scenarioState.room(0x100).room.set(3,false);
      else if(r100EventPhase==2)r100DeathPending=true;
      re4::DestroyEventActorsGl();activeEvent=nullptr;eventUiActive.store(false);
      r100EventPhase=0;eventAccumulator=0;
      for(const auto voice:eventVoiceHandles)soundMixer.stop(voice);
      eventVoiceHandles.clear();eventMixer.installStream(nullptr);
    }
    paused=true;suspendSimulationInput();audioGameActive.store(false);syncAudioPause();
    std::lock_guard<std::mutex> lock(stateMutex);
    graphics="Rendering paused: "+std::string(error.what());hudInfo=graphics;
  }
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_status(JNIEnv *env, jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  const auto s = graphics + (normalView ? " · Normals" : "") +
                 (sceneStatus ? "" : "\n" + re4::Describe(frame));
  return env->NewStringUTF(s.c_str());
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_hud(JNIEnv *env, jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  return env->NewStringUTF(hudInfo.c_str());
}
extern "C" JNIEXPORT jintArray JNICALL
Java_com_ast_re4decomp_NativeBridge_cockpitState(JNIEnv *env,jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  auto result=env->NewIntArray(jsize(cockpitValues.size()));
  if(result)env->SetIntArrayRegion(result,0,jsize(cockpitValues.size()),cockpitValues.data());
  return result;
}
extern "C" JNIEXPORT jintArray JNICALL
Java_com_ast_re4decomp_NativeBridge_binocularState(JNIEnv *env,jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  auto values=binocularValues;if(!binocularUiActive.load())values[0]=0;
  auto result=env->NewIntArray(jsize(values.size()));
  if(result)env->SetIntArrayRegion(result,0,jsize(values.size()),values.data());
  return result;
}
extern "C" JNIEXPORT jintArray JNICALL
Java_com_ast_re4decomp_NativeBridge_scopeState(JNIEnv *env,jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  auto result=env->NewIntArray(jsize(scopeValues.size()));
  if(result)env->SetIntArrayRegion(result,0,jsize(scopeValues.size()),scopeValues.data());
  return result;
}
extern "C" JNIEXPORT jintArray JNICALL
Java_com_ast_re4decomp_NativeBridge_eventTitleState(JNIEnv *env,jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  auto result=env->NewIntArray(jsize(eventTitleValues.size()));
  if(result)env->SetIntArrayRegion(result,0,jsize(eventTitleValues.size()),eventTitleValues.data());
  return result;
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_roomId(JNIEnv *env,jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  return env->NewStringUTF(hudRoomName.c_str());
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_inventory(JNIEnv *env, jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  return env->NewStringUTF(inventoryInfo.c_str());
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_caseSnapshot(JNIEnv *env, jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  return env->NewStringUTF(caseInfo.c_str());
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_configureSubscreen(JNIEnv *env,jclass,jstring path,jint) {
  std::lock_guard<std::mutex> lock(subscreenAssetMutex);
  const auto directory=jstringValue(env,path);
  // Retain the JNI signature for existing callers; the game uses English.
  if(directory!=subscreenAssetRoot) {
    subscreenArt.clear();subscreenImages.clear();subscreenPortraits.clear();
    binocularArtwork.reset();
    scopeArtwork.clear();
    villageTitleArtwork.reset();
    subscreenAssetRoot=directory;
    subscreenItemDirty=subscreenItemId>=0;
  }
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_setSubscreenOpen(JNIEnv *,jclass,jboolean open) {
  // GLSurfaceView.queueEvent owns this command; backgrounding the Activity
  // can stop drawing without changing the original gameplay pause reason.
  if(!open && inventoryCase.puzzleUnsettled())return;
  subscreenOpen=open!=0;cockpitUiRemainder=0;suspendSimulationInput();walkFrame={};subscreenFrame={};
  if(subscreenOpen) {prepareSubscreenPortrait();publishCase();publishSubscreen();}
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_subscreenSnapshot(JNIEnv *env,jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);return env->NewStringUTF(subscreenInfo.c_str());
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_subscreenLayout(JNIEnv *env,jclass,jstring pack) {
  std::lock_guard<std::mutex> lock(subscreenAssetMutex);
  try {
    const auto name=jstringValue(env,pack);auto &art=menuArt(name);
    return env->NewStringUTF((name=="village_title"?villageTitleArtwork->layoutJson(8000):name=="binocular"?binocularArtwork->layoutJson(8000):
        name.rfind("scope_",0)==0?scopeArtwork.at(name)->layoutJson(8000):art.layoutJson()).c_str());
  }
  catch(const std::exception &e) {
    __android_log_print(ANDROID_LOG_WARN,"RE4Subscreen","Menu artwork: %s",e.what());
    return env->NewStringUTF("{}");
  }
}
extern "C" JNIEXPORT jintArray JNICALL
Java_com_ast_re4decomp_NativeBridge_subscreenTexture(JNIEnv *env,jclass,jstring pack,jint id,jint frameIndex) {
  std::lock_guard<std::mutex> lock(subscreenAssetMutex);
  if(id<0 || frameIndex<0)return nullptr;
  try {
    const auto name=jstringValue(env,pack);
    const auto key=name+":"+std::to_string(id)+":"+std::to_string(frameIndex);
    const re4::Texture *image{};
    if(name=="portrait") {
      const auto found=subscreenPortraits.find(unsigned(id));
      if (found == subscreenPortraits.end()) return nullptr;
      image=&found->second;
    } else if(name=="itemicon" || name=="filepage" || name=="itemname" || name=="caseboard" || name=="pickuptext" || name=="casemessage") {
      auto found=subscreenImages.find(key);
      if(found==subscreenImages.end()) {
        // Bounded cache. Menus may visit hundreds of source document pages.
        if(subscreenImages.size()>=24)subscreenImages.clear();
        auto loaded=name=="casemessage"?re4::LoadOriginalPickupMessage(subscreenAssetRoot,0,0,0,unsigned(frameIndex),true):name=="pickuptext"?re4::LoadOriginalPickupMessage(subscreenAssetRoot,unsigned(id)&255,unsigned(id)>>8,unsigned(frameIndex)>>2,unsigned(frameIndex)&3):
            name=="caseboard"?re4::LoadOriginalCaseBoardImage(subscreenAssetRoot,unsigned(id),unsigned(frameIndex)):
            name=="itemicon"?re4::LoadOriginalItemIcon(subscreenAssetRoot,unsigned(id)):
            name=="itemname"?re4::LoadOriginalItemName(subscreenAssetRoot,unsigned(id)):
            re4::LoadOriginalFilePage(subscreenAssetRoot,unsigned(id),unsigned(frameIndex));
        found=subscreenImages.emplace(key,std::move(loaded)).first;
      }
      image=&found->second;
    } else image=menuArt(name).texture(unsigned(id),unsigned(frameIndex));
    if(!image || !image->width || !image->height || image->rgba.size()!=std::size_t(image->width)*image->height*4 || image->rgba.size()>16*1024*1024)
      return nullptr;
    const auto pixels=image->rgba.size()/4;std::vector<jint> argb(pixels+2);
    argb[0]=jint(image->width);argb[1]=jint(image->height);
    for(std::size_t i=0;i<pixels;++i) {
      const auto *p=image->rgba.data()+i*4;
      argb[i+2]=static_cast<jint>((std::uint32_t(p[3])<<24)|(std::uint32_t(p[0])<<16)|(std::uint32_t(p[1])<<8)|p[2]);
    }
    auto result=env->NewIntArray(jsize(argb.size()));
    if(result)env->SetIntArrayRegion(result,0,jsize(argb.size()),argb.data());
    return result;
  }catch(const std::exception &e) {
    __android_log_print(ANDROID_LOG_WARN,"RE4Subscreen","Menu texture: %s",e.what());return nullptr;
  }
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_setSubscreenModelViewport(JNIEnv *,jclass,
    jfloat left,jfloat top,jfloat viewportWidth,jfloat viewportHeight) {
  re4::SetSubscreenItemViewport({left,top,viewportWidth,viewportHeight});
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_subscreenItem(JNIEnv *,jclass,jint id,jboolean examine) {
  subscreenItemId=id>=0 && id<=0xe0?id:-1;subscreenExamining=examine!=0;subscreenItemDirty=true;
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_pendingPickup(JNIEnv *env, jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  return env->NewStringUTF(pickupInfo.c_str());
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_answerPickup(JNIEnv *, jclass, jlong token, jboolean accepted) {
  std::lock_guard<std::mutex> lock(stateMutex);
  pickupAnswerToken = token > 0 ? std::uint64_t(token) : 0;
  pickupAnswerAccepted = accepted != 0;
  pickupAnswerUse = false;
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_answerPickupChoice(JNIEnv *,jclass,jlong token,jint choice) {
  if(choice<1 || choice>3)return;
  std::lock_guard<std::mutex> lock(stateMutex);
  pickupAnswerToken=token>0?std::uint64_t(token):0;
  pickupAnswerAccepted=choice!=2;pickupAnswerUse=choice==3;
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_caseAction(JNIEnv *, jclass, jint op, jint slot, jint a, jint b, jint c) {
  // Only GLSurfaceView.queueEvent calls this entry. A paused surface still
  // services CPU events; the next draw uploads changed weapon GPU resources.
  if (!leon || transferPending()) return;
  try {
    synchronizeCase();
    const auto *item = slot >= 0 ? inventoryCase.item(unsigned(slot)) : nullptr;
    unsigned combineCue=0x27;
    if(op==5 && item && item->inCase() && a>=0) {
      if(const auto *second=inventoryCase.item(unsigned(a))) {
        const auto type=re4::OriginalItemInfo(second->id).type;
        combineCue=type==6?0x27:type==2?0x29:0x28;
      }
    }
    bool success = op == 0;unsigned puzzleResult=0;
    if(op==7 && item)success=inventoryCase.puzzleTake(unsigned(slot));
    else if(op==8){auto candidate=inventoryCase;puzzleResult=candidate.puzzlePut(a,b,unsigned(c)&3,(c&4)!=0);
      success=puzzleResult!=0;
      const auto id=candidate.equippedId();
      if(success && id!=inventoryCase.equippedId())success=leon->canEquip(id) && leon->equipWeapon(id);
      if(success || !puzzleResult){inventoryCase=std::move(candidate);if(success){weaponGpuDirty=true;installWeaponAudio();}}
    }
    else if(op==9)success=inventoryCase.puzzleReturn();
    else if(op==10){auto candidate=inventoryCase;success=candidate.puzzleDiscardSpace();
      if(success && candidate.equippedId()!=inventoryCase.equippedId())success=leon->equipWeapon(candidate.equippedId());
      if(success){inventoryCase=std::move(candidate);weaponGpuDirty=true;installWeaponAudio();}
    }
    else if (item && op == 1 && !pendingPickup.token) {
      const auto id = inventoryCase.weaponId(unsigned(slot));
      synchronizeCase();
      if (leon->canEquip(id) && leon->equipWeapon(id)) {
        success = inventoryCase.equip(unsigned(slot));
        weaponGpuDirty = success;
        if (success) installWeaponAudio();
      }
    } else if (item && op == 2 && !pendingPickup.token) {
      const auto amount = re4::OriginalHealingAmount(item->id);
      success = inventoryCase.consumeHealing(unsigned(slot), leon->life(), re4::LeonPreview::MaxLife);
      if (success) leon->heal(amount);
    } else if(item && (op==3 || op==5 || op==6)) {
      auto candidate=inventoryCase;
      success=op==3?candidate.discard(unsigned(slot)):
          op==6?candidate.detach(unsigned(slot)):a>=0 && candidate.combine(unsigned(slot),unsigned(a));
      const auto id=candidate.equippedId();
      if(success && id!=inventoryCase.equippedId()) {
        success=leon->canEquip(id) && leon->equipWeapon(id);
        if(success){weaponGpuDirty=true;installWeaponAudio();}
      }
      if(success)inventoryCase=std::move(candidate);
    }
    else if (item && op == 4 && a >= 0 && b >= 0 && c >= 0)
      success = inventoryCase.move(unsigned(slot),a,b,unsigned(c));
    if (op) inventoryMessage = success ? "Case updated" :
        op == 1 ? "Cannot equip weapon now · Check original data" :
        op == 2 ? "Health is already full · Item retained" :
                  "Action unavailable · Check space or items";
    if (success) {
      applyCaseAmmo();
      if ((op==1 || op==3 || op==5 || op==6 || op==8 || op==10) && subscreenOpen) prepareSubscreenPortrait();
    }
    if (op) soundMixer.play(0,success ? op==7 || puzzleResult==3 ? 0xd : op==5 || puzzleResult==2 ? combineCue : op==4 || op==8 || op==9 ? 0xc : 8 : 7);
    publishCase();publishSubscreen();publishPickup();
    { std::lock_guard<std::mutex> lock(stateMutex); inventoryInfo = suppliesText(); }
  } catch (const std::exception &e) {
    __android_log_print(ANDROID_LOG_ERROR,"RE4Items","%s",e.what());
  }
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_requestUseGreenHerb(JNIEnv *, jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  useGreenRequested = true;
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_startupConfigure(JNIEnv *env,jclass,jstring path,jint) {
  try {
    const auto root=jstringValue(env,path);
    constexpr unsigned selected=subscreenLanguage;
    auto art=std::make_shared<const re4::TitleData>(root,selected);
    std::vector<std::shared_ptr<const re4::SoundBank>> banks;
    // Title_task::titleInit reads the common bank. The separate language
    // file has only cues0..3 and would incorrectly replace its music cue4.
    for (const auto &source : {root+"/etc/core.das",root+"/ss/Cmn/title.snd"}) {
      auto data=re4::SoundBank::LoadDvd(source);
      banks.insert(banks.end(),data.begin(),data.end());
    }
    std::lock_guard<std::mutex> assetLock(startupAssetMutex);
    if(startupConfiguredRoot!=root) {
      startupMixer.clear();
      startupMixer.installBanks(banks);
      startupConfiguredRoot=root;
      startupVoice=0;
      startupVoiceComplete.store(true);startupLoadVoice=0;startupLoadComplete=true;startupLoadFence=false;
      std::lock_guard<std::mutex> lock(stateMutex);
      pendingStartupAudio.clear();
    }
    startupArt=std::move(art);
    __android_log_print(ANDROID_LOG_INFO,"RE4Startup","Original title loaded · Language %u",selected);
  } catch (const std::exception &e) {
    auto type=env->FindClass("java/io/IOException");
    if (type) env->ThrowNew(type,e.what());
  }
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_startupLayout(JNIEnv *env,jclass,jint group) {
  std::lock_guard<std::mutex> lock(startupAssetMutex);
  try {
    if(!startupArt || group<0)return nullptr;
    return env->NewStringUTF(startupArt->layoutJson(unsigned(group)).c_str());
  } catch(const std::exception &e) {
    __android_log_print(ANDROID_LOG_WARN,"RE4Startup","Title group: %s",e.what());return nullptr;
  }
}
extern "C" JNIEXPORT jintArray JNICALL
Java_com_ast_re4decomp_NativeBridge_startupTexture(JNIEnv *env,jclass,jint id,jint frameIndex) {
  std::lock_guard<std::mutex> lock(startupAssetMutex);
  if(!startupArt || id<0 || frameIndex<0)return nullptr;
  const auto *image=startupArt->texture(unsigned(id),unsigned(frameIndex));
  if(!image || !image->width || !image->height ||
      image->rgba.size()!=std::size_t(image->width)*image->height*4 || image->rgba.size()>16*1024*1024)return nullptr;
  const auto pixels=image->rgba.size()/4;
  std::vector<jint> argb(pixels+2);argb[0]=jint(image->width);argb[1]=jint(image->height);
  for(std::size_t i=0;i<pixels;++i) {
    const auto *p=image->rgba.data()+i*4;
    argb[i+2]=static_cast<jint>((std::uint32_t(p[3])<<24)|(std::uint32_t(p[0])<<16)|(std::uint32_t(p[1])<<8)|p[2]);
  }
  auto result=env->NewIntArray(jsize(argb.size()));
  if(result)env->SetIntArrayRegion(result,0,jsize(argb.size()),argb.data());
  return result;
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_startupCompose(JNIEnv *env,jclass,jintArray frame,jintArray texture,jintArray mask,jfloatArray parameters) {
  // Immutable input arrays and bounded arithmetic only: never acquire a scene/audio mutex.
  if(!frame||!texture||!parameters||(env->GetArrayLength(parameters)!=25 && env->GetArrayLength(parameters)!=29)) {
    auto type=env->FindClass("java/lang/IllegalArgumentException");if(type)env->ThrowNew(type,"Title compositor arrays");return;
  }
  const auto frameCount=env->GetArrayLength(frame),textureCount=env->GetArrayLength(texture),maskCount=mask?env->GetArrayLength(mask):0;
  if(frameCount!=640*480||textureCount<1||textureCount>4096*4096||maskCount>4096*4096) {
    auto type=env->FindClass("java/lang/IllegalArgumentException");if(type)env->ThrowNew(type,"Title compositor array budget");return;
  }
  const auto parameterCount=env->GetArrayLength(parameters);
  float p[29];env->GetFloatArrayRegion(parameters,0,parameterCount,p);if(env->ExceptionCheck())return;
  jint *fb=env->GetIntArrayElements(frame,nullptr);if(!fb)return;
  jint *tex=env->GetIntArrayElements(texture,nullptr);if(!tex){env->ReleaseIntArrayElements(frame,fb,JNI_ABORT);return;}
  jint *m=mask?env->GetIntArrayElements(mask,nullptr):nullptr;
  if(mask&&!m){env->ReleaseIntArrayElements(texture,tex,JNI_ABORT);env->ReleaseIntArrayElements(frame,fb,JNI_ABORT);return;}
  std::string failure;try {
    re4::ComposeStartupFrame(reinterpret_cast<std::uint32_t *>(fb),std::size_t(frameCount),
      reinterpret_cast<const std::uint32_t *>(tex),std::size_t(textureCount),
      reinterpret_cast<const std::uint32_t *>(m),std::size_t(maskCount),p,std::size_t(parameterCount));
  } catch(const std::exception &e) {failure=e.what();}
  if(m)env->ReleaseIntArrayElements(mask,m,JNI_ABORT);
  env->ReleaseIntArrayElements(texture,tex,JNI_ABORT);env->ReleaseIntArrayElements(frame,fb,failure.empty()?0:JNI_ABORT);
  if(!failure.empty()){auto type=env->FindClass("java/lang/IllegalArgumentException");if(type)env->ThrowNew(type,failure.c_str());}
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_startupAudio(JNIEnv *,jclass,jboolean active) {
  audioStartupActive.store(active==JNI_TRUE);
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_ast_re4decomp_NativeBridge_startupVoiceEnded(JNIEnv *,jclass) {
  return (startupLoadFence.load()?startupLoadComplete.load():startupVoiceComplete.load())?JNI_TRUE:JNI_FALSE;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_ast_re4decomp_NativeBridge_eventActive(JNIEnv *,jclass) {
  return eventUiActive.load()?JNI_TRUE:JNI_FALSE;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_ast_re4decomp_NativeBridge_messageActive(JNIEnv *,jclass) {
  return messageUiActive.load()?JNI_TRUE:JNI_FALSE;
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_startupCue(JNIEnv *,jclass,jint block,jint cue) {
  if(block<0 || block>8 || cue<0 || cue>0xff || !audioStartupActive.load())return;
  std::lock_guard<std::mutex> lock(stateMutex);
  if(pendingStartupAudio.size()==64)pendingStartupAudio.erase(pendingStartupAudio.begin());
  if(block==6){startupVoiceComplete.store(false);startupLoadFence=false;}
  if(block==0 && cue==0x38){startupLoadComplete=false;startupLoadFence=true;}
  pendingStartupAudio.emplace_back(unsigned(block),unsigned(cue));
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_audioActive(JNIEnv *, jclass, jboolean active) {
  // Activity lifecycle callbacks must never wait for the mixer while a room
  // initializes its banks. The audio worker applies this gate before its next
  // PCM block, even when the GL thread is still loading or suspended.
  audioUiActive.store(active == JNI_TRUE);
  if (active != JNI_TRUE) {
    setSaveClockRunning(false);
    std::lock_guard<std::mutex> lock(stateMutex);
    pendingMenuAudio.clear();
  }
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_subscreenAudio(JNIEnv *, jclass, jboolean active) {
  audioSubscreenActive.store(active != 0);
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_menuSound(JNIEnv *, jclass, jint cue) {
  if (cue < 0 || cue > 0xff || !audioUiActive.load()) return;
  // Never decode/mix or wait on the audio owner in a Canvas/lifecycle callback.
  // Applying the audio gate before consuming cues also preserves the first
  // confirmation sound when a subscreen opens from a paused diagnostics view.
  std::lock_guard<std::mutex> lock(stateMutex);
  if (pendingMenuAudio.size() == 64) pendingMenuAudio.erase(pendingMenuAudio.begin());
  pendingMenuAudio.push_back(unsigned(cue));
}
extern "C" JNIEXPORT jint JNICALL
Java_com_ast_re4decomp_NativeBridge_renderAudio(JNIEnv *env, jclass, jshortArray samples) {
  if (!samples) return 0;
  const auto length = env->GetArrayLength(samples);
  if (length <= 0 || length > 32768 || (length & 1)) return 0;
  auto *pcm = env->GetShortArrayElements(samples, nullptr);
  if (!pcm) return 0;
  if (audioStartupActive.load()) {
    startupMixer.setPaused(!audioUiActive.load());
    std::vector<std::pair<unsigned,unsigned>> cues;
    {
      std::lock_guard<std::mutex> lock(stateMutex);if(audioUiActive.load())cues.swap(pendingStartupAudio);
    }
    for (const auto &cue:cues) {
      if(cue.first==6) {startupMixer.stop(startupVoice);startupVoice=0;}
      const auto voice=startupMixer.play(cue.first,cue.second);
      if(cue.first==6)startupVoice=voice;
      if(cue.first==0 && cue.second==0x38)startupLoadVoice=voice;
    }
    startupMixer.render(reinterpret_cast<std::int16_t *>(pcm),std::size_t(length/2));
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      const bool queuedVoice=std::any_of(pendingStartupAudio.begin(),pendingStartupAudio.end(),
          [](const auto &cue){return cue.first==6;});
      startupVoiceComplete.store(!queuedVoice && !startupMixer.voiceActive(startupVoice));
      const bool queuedLoad=std::any_of(pendingStartupAudio.begin(),pendingStartupAudio.end(),
          [](const auto &cue){return cue.first==0 && cue.second==0x38;});
      startupLoadComplete.store(!queuedLoad && !startupMixer.voiceActive(startupLoadVoice));
    }
    env->ReleaseShortArrayElements(samples,pcm,0);
    return length/2;
  }
  syncAudioPause();
  std::vector<unsigned> cues;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    cues.swap(pendingMenuAudio);
  }
  for (const auto cue : cues) soundMixer.play(0,cue);
  soundMixer.render(reinterpret_cast<std::int16_t *>(pcm), std::size_t(length / 2));
  { // Speech may still be finishing after the last visual event frame.
    thread_local std::vector<std::int16_t> speech;
    speech.resize(std::size_t(length));eventMixer.render(speech.data(),std::size_t(length/2));
    for(jsize i=0;i<length;++i)pcm[i]=static_cast<jshort>(std::clamp(int(pcm[i])+int(speech[std::size_t(i)]),-32768,32767));
  }
  env->ReleaseShortArrayElements(samples, pcm, 0);
  return length / 2;
}

// Worker-thread operation: no GL or shared game state is accessed.
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_decodeRoom(JNIEnv *env, jclass,
                                               jstring source, jstring target) {
  const char *s = env->GetStringUTFChars(source, nullptr);
  if (!s)
    return nullptr;
  std::string sourcePath(s);
  env->ReleaseStringUTFChars(source, s);
  const char *t = env->GetStringUTFChars(target, nullptr);
  if (!t)
    return nullptr;
  std::string targetPath(t);
  env->ReleaseStringUTFChars(target, t);
  try {
    auto result = re4::LoadYz2(sourcePath);
    std::ofstream out(targetPath, std::ios::binary | std::ios::trunc);
    if (!out)
      throw std::runtime_error("Temporary room file is unavailable");
    out.write(reinterpret_cast<const char *>(result.bytes.data()),
              static_cast<std::streamsize>(result.bytes.size()));
    out.close();
    if (!out)
      throw std::runtime_error(
          "Could not write complete room file");
    std::ostringstream info;
    info << result.bytes.size() << " bytes extracted · " << result.consumed
         << " bytes read from stream";
    return env->NewStringUTF(info.str().c_str());
  } catch (const std::exception &e) {
    jclass type = env->FindClass("java/io/IOException");
    if (type)
      env->ThrowNew(type, e.what());
    return nullptr;
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_inspectArchive(JNIEnv *env, jclass,
                                                   jstring source,
                                                   jstring preview,
                                                   jint imageIndex) {
  const char *chars = env->GetStringUTFChars(source, nullptr);
  if (!chars)
    return nullptr;
  std::string sourcePath(chars);
  env->ReleaseStringUTFChars(source, chars);
  chars = env->GetStringUTFChars(preview, nullptr);
  if (!chars)
    return nullptr;
  std::string previewPath(chars);
  env->ReleaseStringUTFChars(preview, chars);
  try {
    auto arc = re4::RoomArchive::Load(sourcePath);
    std::string text = arc.describe();
    // GetDataExt returns null for absent tags. Valid rooms such as r300
    // omit EMI; directory inspection must not impose engine-part requirements.
    text += "Subfiles depend on the room. Contents are not yet interpreted as "
            "Engine structures implemented.\n";
    for (int occurrence = 0; const auto *entry = arc.find("SMD", occurrence);
         ++occurrence) {
      auto index = static_cast<std::size_t>(entry - arc.entries().data());
      auto bytes = arc.copy(index);
      if (bytes.size() < 16)
        throw std::runtime_error("SMD: Truncated header");
      if (bytes[0] != 0x40) {
        text += "SMD[" + std::to_string(occurrence) + "]: Version " +
                std::to_string(bytes[0]) +
                " not yet supported as geometry.\n";
        continue;
      }
      re4::Smd smd(bytes);
      text += "SMD[" + std::to_string(occurrence) + "]: " + smd.describe();
      for (const auto &model : smd.models)
        (void)re4::RestJointCenters(model);
      if (occurrence == 0) {
        std::unique_ptr<re4::Smd> common;
        if (const auto *entryCommon = arc.find("SMD", 1))
          common = std::make_unique<re4::Smd>(
              arc.copy(
                  static_cast<std::size_t>(entryCommon - arc.entries().data())),
              smd.commonRequirements());
        auto scene = re4::BuildStaticScene(smd, -1, common.get());
        re4::AttachSceneTextures(smd, scene, common.get());
        if (const auto *smxEntry = arc.find("SMX")) {
          re4::Smx smx(arc.copy(
              static_cast<std::size_t>(smxEntry - arc.entries().data())));
          re4::ApplySceneSmx(smx, scene);
          text += "SMX: " + std::to_string(smx.records.size()) + " Records · " +
                  std::to_string(scene.smxDraws) + " matching draws · " +
                  std::to_string(scene.materialColorTriangles) +
                  " material color triangles\n";
        }
        text += scene.describe();
        text +=
            "Room base textures: " + std::to_string(scene.texturedTriangles) +
            " triangles · " + std::to_string(scene.fallbackTriangles) +
            " unresolved\n";
        text +=
            "Common textures: " + std::to_string(scene.commonTextureTriangles) +
            " base triangles · " + std::to_string(scene.commonAlphaTriangles) +
            " mask triangles\n";
      } else {
        text += "Resource source; placements are not drawn as a second "
                "Room scene drawn.\n";
      }
    }
    if (!previewPath.empty()) {
      const auto *entry = arc.find("TPL");
      if (!entry)
        throw std::runtime_error("Room archive: No TPL preview available");
      const auto index = static_cast<std::size_t>(entry - arc.entries().data());
      auto raw = arc.copy(index);
      if (imageIndex < 0)
        throw std::runtime_error("Invalid TPL image index");
      auto image = re4::DecodeTpl(raw, static_cast<unsigned>(imageIndex));
      std::ofstream out(previewPath, std::ios::binary | std::ios::trunc);
      out.write(reinterpret_cast<const char *>(raw.data()), raw.size());
      out.close();
      if (!out)
        throw std::runtime_error(
            "Could not cache room texture");
      text += "TPL[0], Image " + std::to_string(imageIndex) + ": " +
              std::to_string(image.width) + "×" + std::to_string(image.height) +
              " · GX-Format " + std::to_string(image.format) + "\n";
    }
    return env->NewStringUTF(text.c_str());
  } catch (const std::exception &e) {
    jclass type = env->FindClass("java/io/IOException");
    if (type)
      env->ThrowNew(type, e.what());
    return nullptr;
  }
}

extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_environmentData(JNIEnv *env, jclass,
                                                    jstring path) {
  try {
    const auto source = jstringValue(env, path);
    coreAssetPath = source;
    if (source.empty()) {
      coreEnvironmentEff.clear();
      laserSightData.reset();
      footShadowTexture.reset();
      coreLighting.reset();coreSpecular.reset();
      return;
    }
    auto archive = re4::RoomArchive::LoadDrs(source);
    coreLighting = re4::ReadRoomLighting(archive);
    coreSpecular=std::make_shared<const std::vector<re4::Texture>>(re4::LoadCoreSpecular(source));
    const auto *entry = archive.find("EFF");
    if (!entry)
      throw std::runtime_error("CORE-EFF is missing");
    coreEnvironmentEff = archive.copy(entry - archive.entries().data());
    laserSightData = std::make_shared<re4::LaserSightData>(
        re4::BuildLaserSightData(coreEnvironmentEff));
    footShadowTexture = std::make_shared<const re4::Texture>(re4::BuildFootShadowTexture(coreEnvironmentEff));
    __android_log_print(ANDROID_LOG_INFO, "RE4Laser", "CORE EST3/50 · Weapon marker + hit point loaded");
    __android_log_print(ANDROID_LOG_INFO, "RE4Environment",
                        "Original CORE effects loaded: %zu bytes",
                        coreEnvironmentEff.size());
  } catch (const std::exception &e) {
    coreEnvironmentEff.clear();
    laserSightData.reset();
    footShadowTexture.reset();
    coreLighting.reset();
    __android_log_print(ANDROID_LOG_WARN, "RE4Environment",
                        "CORE effects unavailable: %s", e.what());
  }
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_configureSavedGame(JNIEnv *env,jclass,jbyteArray image) {
  if(sceneMode && sceneSession==campaignSession && walking)return;
  try {
    if(!image){pendingSavedGame.reset();return;}
    const auto size=env->GetArrayLength(image);
    if(size!=re4::OriginalSaveCardBytes && size!=re4::OriginalSaveMeaningfulBytes)
      throw std::runtime_error("Original save: File size");
    std::vector<std::uint8_t> bytes(std::size_t(size),0);
    env->GetByteArrayRegion(image,0,size,reinterpret_cast<jbyte*>(bytes.data()));
    if(env->ExceptionCheck())return;
    auto card=re4::DecodeOriginalSaveCard(bytes);
    const auto saved=re4::DecodeNativeSave(card.sections);
    if(!re4::IsVillageRoom(saved.room) || saved.enemyListNo>1 || !saved.life ||
       saved.life>re4::LeonPreview::MaxLife || saved.maxLife!=re4::LeonPreview::MaxLife)
      throw std::runtime_error("Original save: Player profile outside the first level");
    pendingSavedGame=std::move(card);savedGameSession=campaignSession;
  } catch(const std::exception &e) {
    pendingSavedGame.reset();env->ThrowNew(env->FindClass("java/io/IOException"),e.what());
  }
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_savedGameProblem(JNIEnv *env,jclass,jbyteArray image) {
  try {
    if(!image)throw std::runtime_error("Save file is missing");
    const auto size=env->GetArrayLength(image);
    if(size!=re4::OriginalSaveCardBytes && size!=re4::OriginalSaveMeaningfulBytes)
      throw std::runtime_error("Invalid save file size");
    std::vector<std::uint8_t> bytes(std::size_t(size),0);
    env->GetByteArrayRegion(image,0,size,reinterpret_cast<jbyte*>(bytes.data()));
    if(env->ExceptionCheck())return nullptr;
    const auto saved=re4::DecodeNativeSave(re4::DecodeOriginalSaveCard(bytes).sections);
    if(!re4::IsVillageRoom(saved.room) || saved.enemyListNo>1 || !saved.life ||
       saved.life>re4::LeonPreview::MaxLife || saved.maxLife!=re4::LeonPreview::MaxLife)
      throw std::runtime_error("Player profile is outside the playable first level");
    return env->NewStringUTF("");
  } catch(const std::exception &error) {return env->NewStringUTF(error.what());}
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_ast_re4decomp_NativeBridge_saveRequested(JNIEnv *,jclass) {
  return saveMenuRequested.load()?JNI_TRUE:JNI_FALSE;
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_finishSave(JNIEnv *,jclass) {
  pendingSaveWrite.reset();pendingSaveWriteId=0;
  saveMenuRequested.store(false);subscreenOpen=false;walkFrame={};suspendSimulationInput();
}
extern "C" JNIEXPORT jlong JNICALL
Java_com_ast_re4decomp_NativeBridge_saveWriteId(JNIEnv *,jclass) {
  return jlong(pendingSaveWriteId);
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_ast_re4decomp_NativeBridge_completeSaveWrite(JNIEnv *,jclass,jlong transaction,jboolean written) {
  if(!transaction || std::uint64_t(transaction)!=pendingSaveWriteId || !pendingSaveWrite)return JNI_FALSE;
  if(written==JNI_TRUE) {
    originalSaveBasis=pendingSaveWrite->sections;
    originalSaveMetadata.saveCount=pendingSaveWrite->metadata.saveCount;
    entryCheckpoint=re4::DecodeNativeSave(pendingSaveWrite->sections);
    checkpointBasis=pendingSaveWrite->sections;
  }
  pendingSaveWrite.reset();pendingSaveWriteId=0;
  return written==JNI_TRUE?JNI_TRUE:JNI_FALSE;
}
namespace {
re4::NativeSaveSnapshot captureNativeGame() {
    if(inventoryCase.puzzleUnsettled())throw std::runtime_error("Place case/space items before saving");
    storeInteractionState();synchronizeCase();
    auto snapshot=originalSaveMetadata;snapshot.scenario=scenarioState;
    snapshot.inventory=inventoryCase.items();snapshot.caseSize=inventoryCase.size();snapshot.equippedSlot=inventoryCase.equippedSlot();
    snapshot.room=activeRoomId;snapshot.part=std::uint8_t(activePart);snapshot.life=std::uint16_t(leon->life());
    snapshot.maxLife=re4::LeonPreview::MaxLife;snapshot.heading=leon->heading();
    snapshot.radioHeard=r100RadioHeard;snapshot.radioMessageNo=r100RadioMessageNo;
    for(unsigned k=0;k<3;++k)snapshot.position[k]=float(leon->feet()[k]);
    {
      std::lock_guard<std::mutex> lock(saveClockMutex);
      const auto now=saveWallSeconds();originalSaveClock.fold(now);snapshot.playSeconds=originalSaveClock.seconds(now);
    }
    if(!snapshot.enemyDeaths)snapshot.enemyDeaths.emplace();
    for(unsigned index:r100DeadIndices)if(index<255)re4::SetOriginalSaveEnemyDead(*snapshot.enemyDeaths,unsigned(snapshot.enemyListNo),index);
    for(unsigned index:r100UsedTraps)re4::SetOriginalSaveEnemyDead(*snapshot.enemyDeaths,unsigned(snapshot.enemyListNo),index);
    if(forestAnimals)for(unsigned index:forestAnimals->deadIndices())re4::SetOriginalSaveEnemyDead(*snapshot.enemyDeaths,unsigned(snapshot.enemyListNo),index);
    if(villageAnimals)for(unsigned index:villageAnimals->deadIndices())re4::SetOriginalSaveEnemyDead(*snapshot.enemyDeaths,unsigned(snapshot.enemyListNo),index);
    if(roomPickups)runtimeRoomDrops[activeRoomId]=roomPickups->runtimeDrops();
    snapshot.looseItems=re4::CaptureOriginalPersistentDrops(runtimeRoomDrops);
    const auto listPath=snapshot.enemyListNo==1?enemySpawnPath.substr(0,enemySpawnPath.find_last_of('/'))+"/emleon01.esl":enemySpawnPath;
    std::ifstream source(listPath,std::ios::binary|std::ios::ate);
    if(!source || source.tellg()!=re4::EnemySpawns::FileBytes)throw std::runtime_error("Save: Original ESL missing");
    snapshot.enemyList.emplace(re4::EnemySpawns::FileBytes);source.seekg(0);
    if(!source.read(reinterpret_cast<char*>(snapshot.enemyList->data()),snapshot.enemyList->size()))throw std::runtime_error("Save: Truncated ESL");
    return snapshot;
}
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_ast_re4decomp_NativeBridge_deathRequested(JNIEnv *,jclass) {
  return deathRequested.load()?JNI_TRUE:JNI_FALSE;
}
extern "C" JNIEXPORT jintArray JNICALL
Java_com_ast_re4decomp_NativeBridge_deathBackdrop(JNIEnv *env,jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  if(deathPicture.empty())return nullptr;
  auto result=env->NewIntArray(jsize(deathPicture.size()));
  if(result)env->SetIntArrayRegion(result,0,jsize(deathPicture.size()),deathPicture.data());
  return result;
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_continueSound(JNIEnv *,jclass,jint number) {
  if(number==5 || number==6 || number==8)soundMixer.play(0,unsigned(number));
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_continueConfirmed(JNIEnv *,jclass) {
  if(!deathRequested.load())return;
  std::lock_guard<std::mutex> lock(stateMutex);
  continueConfirmed.store(true);deathPicture.clear();
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_ast_re4decomp_NativeBridge_continueGame(JNIEnv *,jclass) {
  if(!campaignMode || !deathRequested.load() || !entryCheckpoint || !entryCheckpoint->life || transferPending())return JNI_FALSE;
  try {
    pendingContinue=re4::RestoreNativeCheckpoint(*entryCheckpoint,captureNativeGame());
    requestRoom(pendingContinue->room,pendingContinue->part,
        {pendingContinue->position[0],pendingContinue->position[1],pendingContinue->position[2]},pendingContinue->heading);
    return JNI_TRUE;
  } catch(const std::exception &error) {
    pendingContinue.reset();__android_log_print(ANDROID_LOG_ERROR,"RE4Continue","%s",error.what());return JNI_FALSE;
  }
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_ast_re4decomp_NativeBridge_saveGame(JNIEnv *env,jclass,jint slot,jintArray calendar) {
  try {
    if(!campaignMode || !leon || !leon->alive() || !saveMenuRequested.load() || !subscreenOpen || activeEvent)
      throw std::runtime_error("Save: Typewriter is not active");
    if(pendingSaveWrite)throw std::runtime_error("Save: Write operation already in progress");
    if(!calendar || env->GetArrayLength(calendar)!=10)throw std::runtime_error("Save: Calendar missing");
    jint clock[10]{};env->GetIntArrayRegion(calendar,0,10,clock);if(env->ExceptionCheck())return nullptr;
    const re4::OriginalSaveCalendar date{clock[0],clock[1],clock[2],clock[3],clock[4],clock[5],clock[6],clock[7],clock[8],clock[9]};
    auto snapshot=captureNativeGame();
    auto sections=re4::EncodeNativeSave(snapshot,originalSaveBasis?&*originalSaveBasis:nullptr);
    const auto files=playerAssetPath.substr(0,playerAssetPath.find_last_of('/'))+"/..";
    auto bytes=re4::EncodeOriginalSaveCard(sections,re4::LoadOriginalSaveCardArt(files),date,unsigned(slot));
    auto card=re4::DecodeOriginalSaveCard(bytes);
    auto result=env->NewByteArray(jsize(bytes.size()));if(result)env->SetByteArrayRegion(result,0,jsize(bytes.size()),reinterpret_cast<const jbyte*>(bytes.data()));
    // The source CARD write must complete before its new checkpoint and
    // save counter become active. Java acknowledges the durable rename on
    // this same GL owner; failed writes keep the old checkpoint intact.
    if(result && !env->ExceptionCheck()) {
      pendingSaveWrite=std::move(card);pendingSaveWriteId=++saveWriteSequence;
    }
    return result;
  } catch(const std::exception &e) {
    __android_log_print(ANDROID_LOG_ERROR,"RE4Save","%s",e.what());return nullptr;
  }
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_configurePlayMode(JNIEnv *, jclass,
                                                    jboolean explore, jlong session) {
  // GL-thread-owned; the same Activity/EGL recreation must preserve a
  // deliberate switch to a combat test made in its diagnostic menu.
  if (playModeSession == session) return;
  playModeSession = session; exploreMode = explore != 0;
  initialCombatTest = !exploreMode;
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_configureWeapons(JNIEnv *env, jclass,
                                                  jstring handgun, jstring shotgun) {
  try {
    handgunAssetPath = jstringValue(env, handgun);
    shotgunAssetPath = jstringValue(env, shotgun);
  } catch (const std::exception &e) {
    handgunAssetPath.clear(); shotgunAssetPath.clear();
    __android_log_print(ANDROID_LOG_ERROR,"RE4Weapon","%s",e.what());
  }
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_configureMusic(JNIEnv *env, jclass, jstring directory) {
  try {
    auto path = jstringValue(env,directory);
    if (path != musicAssetRoot) { musicCache.clear(); streamCache.clear(); musicAssetRoot = std::move(path); }
  } catch (const std::exception &e) {
    musicAssetRoot.clear(); musicCache.clear(); streamCache.clear();
    __android_log_print(ANDROID_LOG_ERROR,"RE4Audio","%s",e.what());
  }
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_configureDisplayBrightness(JNIEnv *,jclass,jint value) {
  displayBrightness.store(std::clamp(int(value),34,114));
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_configureAimInversion(JNIEnv *,jclass,jboolean inverted) {
  aimInverted.store(inverted==JNI_TRUE);
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_configureCampaignMode(JNIEnv *,jclass,
    jboolean enabled,jint difficulty,jlong session) {
  campaignMode=enabled==JNI_TRUE;
  campaignDifficulty=unsigned(std::clamp(int(difficulty),0,2));
  campaignSession=session;
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_configureOpeningCanceled(JNIEnv *,jclass,jboolean canceled) {
  configuredOpeningCanceled=canceled==JNI_TRUE;
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_configureTestProfile(JNIEnv *, jclass,
                                                       jint profile, jlong session) {
  if (testProfileSession == session) return;
  testProfileSession = session;
  testProfile = profile == 1 ? 1u : 0u;
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_createScene(
    JNIEnv *env, jclass, jstring path, jstring playerPath, jstring weaponPath,
    jstring infoPath, jstring roomId, jlong session) {
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    sceneStatus = true;
  }
  // The same Activity may receive a fresh GL context after suspend. Preserve
  // the current room, actor and pending transfer instead of reopening its
  // initial intent's room. A new Activity has a distinct session identifier.
  const bool resume = sceneMode && sceneSession == session && previewSmd;
  const auto previousView = re4::SaveSceneView();
  program = vao = vbo = texture = 0;
  hasTexture = false;
  sceneMode = true;
  re4::ForgetSceneGl();
  re4::ForgetEventActorsGl();
  re4::ForgetRadioTermGl();
  re4::ForgetSubscreenItemGl();subscreenItemDirty=subscreenItemId>=0;
  re4::ForgetDisplayBrightnessGl();
  re4::ForgetGraphicsOptionsGl();
  actorGpuReady = false;
  try {
    if (resume) {
      uploadPreview(false, false);
      re4::RestoreSceneView(previousView);
      walkFrame = {};
      suspendSimulationInput();
      __android_log_print(ANDROID_LOG_INFO, "RE4Flow",
                          "%s: GL context restored",
                          roomName(activeRoomId).c_str());
      return;
    }
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      roomRequest = {};
      roomCompletion = {};
      normalView = false;
      inspectorRequested = false;
      useGreenRequested = false;
      inventoryInfo.clear();
    }
    objectRoomStates.clear();
    scenarioState = {};
    r100DeadIndices.clear();r100UsedTraps.clear();r100DroppedIndices.clear();runtimeRoomDrops.clear();enemyDropState={};currentEnemyDropContext={};forestGunFired=false;
    audioGameActive.store(false);
    audioSubscreenActive.store(false); soundMixer.setSubscreen(false);
    syncAudioPause();
    soundMixer.clear();
    staticAudioProfile.clear();
    inventoryMessage.clear();
    inventoryCase.clear();
    subscreenOpen=false;subscreenItemId=-1;subscreenItemDirty=false;subscreenFrame={};
    muzzleEffects.reset(); muzzleWeaponId = 0; windowEffectSerial = 0;
    pendingPickup = {};audioPickupActive.store(false);
    weaponGpuDirty = false;
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      caseInfo.clear(); pickupInfo.clear(); pickupAnswerToken = 0;
    }
    resetWalk();
    sceneSession = session;
    std::optional<re4::NativeSaveSnapshot> loadedSave;
    originalSaveBasis.reset();originalSaveMetadata={};activePartnerId=activePartnerCondition=0;saveMenuRequested.store(false);
    pendingSaveWrite.reset();pendingSaveWriteId=0;
    entryCheckpoint.reset();pendingContinue.reset();checkpointBasis.reset();
    {std::lock_guard<std::mutex> lock(stateMutex);deathPicture.clear();}
    saveAccuracyOwner=nullptr;saveAccuracyCursor=0;saveKillCursors.clear();
    r100RadioHeard={};r100RadioMessageNo=24;
    if(pendingSavedGame && savedGameSession==session) {
      loadedSave=re4::DecodeNativeSave(pendingSavedGame->sections);
      originalSaveBasis=pendingSavedGame->sections;originalSaveMetadata=*loadedSave;
      scenarioState=loadedSave->scenario;
      r100RadioHeard=loadedSave->radioHeard;r100RadioMessageNo=loadedSave->radioMessageNo;
      for(const auto &item:loadedSave->inventory)if(item.active)scenarioState.carriedItems[item.id]+=item.count;
      if(loadedSave->enemyDeaths)for(unsigned index=0;index<255;++index)
        if(re4::OriginalSaveEnemyDead(*loadedSave->enemyDeaths,unsigned(loadedSave->enemyListNo),index)) {
          r100DeadIndices.insert(index);r100DroppedIndices.insert(index);
          r100UsedTraps.insert(index);
        }
      if(loadedSave->looseItems)runtimeRoomDrops=re4::RestoreOriginalPersistentDrops(*loadedSave->looseItems);
      pendingSavedGame.reset();
    }
    {
      std::lock_guard<std::mutex> lock(saveClockMutex);
      originalSaveClock.reset(originalSaveMetadata.playSeconds,saveWallSeconds(),audioUiActive.load());
    }
    campaignMode = campaignMode && campaignSession == session;
    if(campaignMode && !loadedSave) {
      originalSaveMetadata.difficulty=campaignDifficulty==2?6:campaignDifficulty==1?1:5;
      originalSaveMetadata.gameLevel=campaignDifficulty==1?2:campaignDifficulty==2?10:5;
      originalSaveMetadata.difficultyPoint=campaignDifficulty==2?10999:5500;
    }
    if(campaignMode && !loadedSave)re4::BeginOriginalOpening(scenarioState,configuredOpeningCanceled);
    activeRoomId = parseRoom(jstringValue(env, roomId));
    if(loadedSave && loadedSave->room!=activeRoomId)throw std::runtime_error("Save file and prepared room do not match");
    activePart = 0;
    paused = false;
    flowInfo.clear();
    roomInfo.reset();
    auto information = jstringValue(env, infoPath);
    if (!information.empty()) {
      try {
        roomInfo = std::make_unique<re4::RoomInfo>(readRoomInfo(information));
      } catch (const std::exception &e) {
        __android_log_print(ANDROID_LOG_WARN, "RE4Flow",
                            "Room starts unavailable: %s", e.what());
      }
    }
    playerAssetPath = jstringValue(env, playerPath);
    weaponAssetPath = jstringValue(env, weaponPath);
    installStaticAudio();
    const auto entryScenario=scenarioState; // gameStageInit saves before R100Init/StageSet.
    auto prepared = prepareRoom(jstringValue(env, path), activeRoomId);
    resetWalk();
    collisionView = false;
    prepared.swapActive();
    leon.reset();
    playerInfo = "Leon unavailable; check Disc 1 and RE4Player log";
    auto player = jstringValue(env, playerPath),
         weapon = jstringValue(env, weaponPath);
    playerAssetPath = player;
    weaponAssetPath = weapon;
    if (!player.empty() && !weapon.empty()) {
      try {
        // All normal room starts use the same player capabilities as test
        // transfers. Loading EM12 motion data must not spawn an encounter.
        const bool fullPlayerActions = !enemyAssetPath.empty();
        NativeLoadClock loadClock("Leon and weapons",activeRoomId);
        leon = std::make_unique<re4::LeonPreview>(
            player, weapon, enemyAssetPath, fullPlayerActions);
        if (!handgunAssetPath.empty()) leon->installHandgun(handgunAssetPath);
        if (!shotgunAssetPath.empty()) leon->installShotgun(shotgunAssetPath);
        if (!handgunAssetPath.empty() && leon->equipWeapon(0x23)) {
          inventoryCase.resetGameStart(0x23);
          applyCaseAmmo();
        } else inventoryCase.resetTestLoadout(leon->magazine(),leon->reserve(),0x21);
        if(dogTraps)dogTraps->installPlayerMotions(*leon);
        if(r100Secondary)r100Secondary->installPlayerMotion(*leon);
        leon->setUnlimitedHealth(!campaignMode || (testCheatFlags.load()&1));
        leon->setUnlimitedAmmo((testCheatFlags.load()&2)!=0);
        leon->setDamageDifficulty(campaignMode && campaignDifficulty==1?2:
            campaignMode && campaignDifficulty==2?10:5);
        leon->setRenderInterpolation(true);
        __android_log_print(ANDROID_LOG_INFO, "RE4Player", "%s · Rendering interpolated",
            campaignMode ? "New game: Original health values" : "Test mode: Infinite health ON");
        playerInfo = leon->describe();
        __android_log_print(ANDROID_LOG_INFO, "RE4Player", "%s",
                            playerInfo.c_str());
        if (!fullPlayerActions)
          __android_log_print(ANDROID_LOG_WARN, "RE4Player",
                              "Knife/Grab/Kick disabled: EM12 data missing; "
                              "Check Disc 1 import and RE4Enemy log");
        __android_log_print(ANDROID_LOG_INFO, "RE4Weapon", "%s",
                            leon->weapon()->describe().c_str());
        installWeaponAudio();
      } catch (const std::exception &e) {
        playerInfo = "Leon not loaded: " + std::string(e.what());
        __android_log_print(ANDROID_LOG_ERROR, "RE4Player", "%s",
                            playerInfo.c_str());
      }
    }
    startWalking();
    if(loadedSave && leon && walkProbe) {
      inventoryCase.restoreSnapshot(loadedSave->caseSize,loadedSave->inventory,loadedSave->equippedSlot);
      if(!leon->restoreLife(loadedSave->life))throw std::runtime_error("Save: Invalid health values");
      if(!leon->equipWeapon(inventoryCase.equippedId()))throw std::runtime_error("Save: Unsupported weapon");
      applyCaseAmmo();
      const re4::CollisionVec savedFeet{loadedSave->position[0],loadedSave->position[1],loadedSave->position[2]};
      if(!walkProbe->place(savedFeet,1600))throw std::runtime_error("Save: Player position outside room collision");
      leon->setPlacement(walkProbe->position(),loadedSave->heading);activePart=loadedSave->part;
      re4::SceneFollowHeading(leon->heading());re4::SceneSeedFollowView(0,0);
    }
    uploadPreview(false, false);
    commitRoomAudio(prepared);
    if(campaignMode && leon) {
      if(loadedSave)entryCheckpoint=*loadedSave; // SYS_LOAD_GAME preserves the loaded checkpoint.
      else {entryCheckpoint=captureNativeGame();entryCheckpoint->scenario=entryScenario;entryCheckpoint->saveKind=-1;}
      checkpointBasis=re4::EncodeNativeSave(*entryCheckpoint,originalSaveBasis?&*originalSaveBasis:nullptr);
    }
    if(campaignMode && activePart<8 && std::find(re4::OriginalSaveRoomIds().begin(),re4::OriginalSaveRoomIds().end(),activeRoomId)!=re4::OriginalSaveRoomIds().end())
      originalSaveMetadata.passedRooms[activeRoomId]|=std::uint8_t(0x80u>>activePart);
    beginCampaignEntry();
    if (walking && leon)
      re4::SceneFollowHeading(leon->heading());
    publishCase();
    logWalk();
    __android_log_print(ANDROID_LOG_INFO,"RE4Mode","%s · %s",
                        roomName(activeRoomId).c_str(),exploreMode?"Explore without enemies":"Ganado combat test");
    if (initialCombatTest && activeRoomId==0x400 && leon &&
        !enemyAssetPath.empty() && !enemySpawnPath.empty()) {
      initialCombatTest=false;
      requestRoom(0x400,0,re4::EnemyEncounter::TerrainPlayerPosition(),
                  3.14159265358979323846f,true);
    }
  } catch (const std::exception &e) {
    re4::DestroySceneGl();
    resetWalk();
    collisionView = false;
    previewSat.reset();
    leon.reset();
    previewSmd.reset();
  previewLighting.reset();previewShadows.reset();
    previewCommon.reset();
    previewSmx.reset();
    previewEnvironment.reset();
  previewWorldEffects.reset();
  previewObjects.reset();
    roomFlow.reset();
    previewPlacements.clear();
    std::string info = std::string("3D error: ") + e.what();
    __android_log_print(ANDROID_LOG_ERROR, "RE4Scene", "%s", info.c_str());
    std::lock_guard<std::mutex> lock(stateMutex);
    roomRequest = {};
    roomCompletion = {};
    graphics = info;
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_pendingRoomRequest(JNIEnv *env, jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  if (!roomRequest.token)
    return env->NewStringUTF("");
  const auto request =
      std::to_string(roomRequest.token) + "\t" + roomName(roomRequest.room);
  return env->NewStringUTF(request.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_requestDoorTest(JNIEnv *, jclass) {
  if (!sceneMode || !roomInfo || transferPending())
    return;
  if (const auto *start = roomInfo->find(0x301))
    requestRoom(0x301, 0, start->position, static_cast<float>(start->heading));
  else
    flowInfo = "Original r301 start is missing";
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_configureAmbient(JNIEnv *env, jclass,
                                                  jstring asset, jstring list) {
  chickenAssetPath = jstringValue(env, asset);
  chickenSpawnPath = jstringValue(env, list);
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_configureEnemy(JNIEnv *env, jclass,
                                                   jstring model,
                                                   jstring list) {
  // Called on the owning GL thread, including context restoration. Existing
  // encounters retain CPU HP/pose/RNG; these paths also provide Leon's grab,
  // knife and kick profile when a new normal scene session is created.
  try {
    enemyAssetPath = jstringValue(env, model);
    enemySpawnPath = jstringValue(env, list);
  } catch (const std::exception &e) {
    enemyAssetPath.clear();
    enemySpawnPath.clear();
    __android_log_print(ANDROID_LOG_ERROR, "RE4Enemy", "%s", e.what());
  }
}
extern "C" JNIEXPORT jint JNICALL
Java_com_ast_re4decomp_NativeBridge_cheatFlags(JNIEnv *,jclass) {return jint(testCheatFlags.load());}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_setCheatFlags(JNIEnv *,jclass,jint flags) {
  testCheatFlags.store(unsigned(flags)&3);
  if(leon) {leon->setUnlimitedHealth((flags&1)!=0);leon->setUnlimitedAmmo((flags&2)!=0);}
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_giveTestWeapon(JNIEnv *env,jclass,jint itemId) {
  try {
  constexpr unsigned ids[]={0x38,0x23,0x21,0x40,0x25,0x27,0x3,0x29,0x2a,0x37,0x2c,0x94,0x2d,0x2e,0x2f,0x30,0x3e,0x34,0x36,0x35,0x17,0x6d,0x52,0x1,0x2,0xe,0x3f,0x42,0x43,0x44,0x45,0xaa,0xc5};
  const auto id=unsigned(itemId);std::string message;
  if(!leon || !leon->alive() || activeEvent || transferPending())message="Weapons are unavailable during this action.";
  else if(std::find(std::begin(ids),std::end(ids),id)==std::end(ids))message="Unknown GameCube weapon.";
  else {
    auto candidate=inventoryCase;
    const bool already=candidate.count(id) && ((re4::OriginalWeaponCarry(id) && id!=1 && id!=2 && id!=0xe) || id==0x38);
    if(!already && !candidate.add(id,1))message="There is not enough space in the case.";
    else if(id==0x52 && !candidate.count(0x72) && !candidate.add(0x72,10))
      message="There is not enough space for the bow and its arrows.";
    else {
      const bool equip=leon->canEquip(id);
      if(equip && !leon->equipWeapon(id))return env->NewStringUTF("Cannot equip this weapon during the current action.");
      if(equip)for(const auto &item:candidate.items())if(item.active && item.id==id){candidate.equip(item.slot);break;}
      inventoryCase=std::move(candidate);applyCaseAmmo();
      if(equip) {weaponGpuDirty=true;installWeaponAudio();message=already?"Weapon equipped.":"Weapon added and equipped.";}
      else if(id==0x38)message="Knife added. Hold L1 to use it.";
      else if(re4::OriginalItemInfo(id).type!=1 && id!=1 && id!=2 && id!=0xe)
        message="Accessory added to the case.";
      else message=already?"This weapon is already in the case. Combat support is still pending.":
          "Item added to the case. Combat support for this weapon is still pending.";
    }
  }
  inventoryMessage=message;return env->NewStringUTF(message.c_str());
  } catch(const std::exception &error) {
    inventoryMessage=std::string("Could not equip original weapon: ")+error.what();
    __android_log_print(ANDROID_LOG_ERROR,"RE4Weapon","%s",inventoryMessage.c_str());
    return env->NewStringUTF(inventoryMessage.c_str());
  }
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_requestGanadoTest(JNIEnv *, jclass) {
  if (!sceneMode || !leon || transferPending())
    return;
  if (enemyAssetPath.empty() || enemySpawnPath.empty()) {
    flowInfo = "Ganado data is missing; check complete Disc 1 import";
    __android_log_print(ANDROID_LOG_ERROR, "RE4Enemy", "%s", flowInfo.c_str());
    return;
  }
  requestRoom(0x400, 0, re4::EnemyEncounter::TerrainPlayerPosition(),
              3.14159265358979323846f, true);
  exploreMode=false;
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_requestVillageTest(JNIEnv *, jclass) {
  if (!sceneMode || !leon || !roomInfo || transferPending()) return;
  testProfile = 1;
  exploreMode = true;
  if (const auto *start = roomInfo->find(0x101, 1))
    requestRoom(0x101, 0, start->position, static_cast<float>(start->heading), false, true);
  else flowInfo = "Original r101 test start is missing";
}
extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_requestInspector(JNIEnv *, jclass) {
  std::lock_guard<std::mutex> lock(stateMutex);
  inspectorRequested = true;
}

namespace {
void applyRoomCompletion() {
  RoomRequest pending;
  RoomCompletion completion;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!roomCompletion.token || roomCompletion.token != roomRequest.token)
      return;
    pending = roomRequest;
    completion = std::move(roomCompletion);
    roomCompletion = {};
  }
  const bool continuing=pendingContinue && pendingContinue->room==pending.room;
  const auto oldScenario=scenarioState;
  const auto oldMetadata=originalSaveMetadata;
  const auto oldInventory=inventoryCase;
  const auto oldDead=r100DeadIndices,oldDropped=r100DroppedIndices,oldTraps=r100UsedTraps;
  const auto oldDrops=runtimeRoomDrops;
  const auto oldObjects=objectRoomStates;
  const auto oldRadio=r100RadioHeard;
  const auto oldRadioNo=r100RadioMessageNo;
  std::optional<re4::NativeSaveSnapshot> nextCheckpoint;
  std::optional<re4::OriginalSaveSections> nextCheckpointBasis;
  try {
    if (!completion.error.empty())
      throw std::runtime_error(completion.error);
    if(continuing) {
      originalSaveMetadata=*pendingContinue;scenarioState=pendingContinue->scenario;
      inventoryCase.restoreSnapshot(pendingContinue->caseSize,pendingContinue->inventory,pendingContinue->equippedSlot);
      r100RadioHeard=pendingContinue->radioHeard;r100RadioMessageNo=pendingContinue->radioMessageNo;
      r100DeadIndices.clear();r100DroppedIndices.clear();r100UsedTraps.clear();objectRoomStates.clear();
      if(pendingContinue->enemyDeaths)for(unsigned index=0;index<255;++index)
        if(re4::OriginalSaveEnemyDead(*pendingContinue->enemyDeaths,unsigned(pendingContinue->enemyListNo),index)) {
          r100DeadIndices.insert(index);r100DroppedIndices.insert(index);
          r100UsedTraps.insert(index);
        }
      runtimeRoomDrops=pendingContinue->looseItems?re4::RestoreOriginalPersistentDrops(*pendingContinue->looseItems):decltype(runtimeRoomDrops){};
      // gameStageInit saves again for SYS_CONTINUE, but skips SYS_LOAD_GAME.
      if(pendingContinue->saveKind.value_or(-1)==-1)nextCheckpoint=*pendingContinue;
    } else if(campaignMode && leon && leon->alive()) {
      nextCheckpoint=captureNativeGame();nextCheckpoint->saveKind=-1;
      nextCheckpoint->room=pending.room;nextCheckpoint->part=std::uint8_t(pending.part);
      nextCheckpoint->enemyListNo=int(re4::VillageEnemyList(pending.room,nextCheckpoint->scenario.scenario.test(39),unsigned(nextCheckpoint->enemyListNo)));
      if(nextCheckpoint->enemyListNo!=originalSaveMetadata.enemyListNo) {
        const auto targetList=enemySpawnPath.substr(0,enemySpawnPath.find_last_of('/'))+(nextCheckpoint->enemyListNo?"/emleon01.esl":"/emleon00.esl");
        std::ifstream list(targetList,std::ios::binary|std::ios::ate);
        if(!list || list.tellg()!=re4::EnemySpawns::FileBytes)throw std::runtime_error("Checkpoint: Original village enemy list missing");
        nextCheckpoint->enemyList.emplace(re4::EnemySpawns::FileBytes);list.seekg(0);
        if(!list.read(reinterpret_cast<char*>(nextCheckpoint->enemyList->data()),nextCheckpoint->enemyList->size()))throw std::runtime_error("Checkpoint: Truncated village enemy list");
      }
      nextCheckpoint->roomContinues=0;nextCheckpoint->previousRoom=activeRoomId;nextCheckpoint->previousPart=std::uint8_t(activePart);
      for(unsigned k=0;k<3;++k)nextCheckpoint->position[k]=float(pending.position[k]);
      nextCheckpoint->heading=pending.heading;
    }
    if(nextCheckpoint)nextCheckpointBasis=re4::EncodeNativeSave(*nextCheckpoint,
        continuing && checkpointBasis?&*checkpointBasis:originalSaveBasis?&*originalSaveBasis:nullptr);
    if(nextCheckpoint)originalSaveMetadata.enemyDeaths=nextCheckpoint->enemyDeaths;
    auto next = prepareRoom(completion.path, pending.room,!continuing);
    if (!next.probe || !(re4::IsVillageRoom(pending.room)?next.probe->placeAuthored(pending.position):next.probe->place(pending.position)))
      throw std::runtime_error(
          "Original destination has no supporting SAT floor");
    float destinationHeading = pending.heading;
    if(continuing) {
      next.player=std::make_unique<re4::LeonPreview>(playerAssetPath,weaponAssetPath,enemyAssetPath,true);
      if(!handgunAssetPath.empty())next.player->installHandgun(handgunAssetPath);
      if(!shotgunAssetPath.empty())next.player->installShotgun(shotgunAssetPath);
      if(!next.player->restoreLife(pendingContinue->life) || !next.player->equipWeapon(inventoryCase.equippedId()))
        throw std::runtime_error("Checkpoint: Invalid player state");
      next.player->setUnlimitedHealth((testCheatFlags.load()&1)!=0);next.player->setUnlimitedAmmo((testCheatFlags.load()&2)!=0);next.player->setDamageDifficulty(pendingContinue->gameLevel);
      next.player->setRenderInterpolation(true);next.replacePlayer=true;
    }
    if (pending.village) {
      next.player = std::make_unique<re4::LeonPreview>(
          playerAssetPath, weaponAssetPath, enemyAssetPath, true);
      if (!handgunAssetPath.empty()) next.player->installHandgun(handgunAssetPath);
      if (!shotgunAssetPath.empty()) next.player->installShotgun(shotgunAssetPath);
      if (leon) next.player->inheritInventory(*leon);
      next.player->setUnlimitedHealth(true);
      next.player->setRenderInterpolation(true);
      next.replacePlayer = true;
    }
    if (pending.combat) {
      next.player = std::make_unique<re4::LeonPreview>(
          playerAssetPath, weaponAssetPath, enemyAssetPath, true);
      if (!handgunAssetPath.empty()) next.player->installHandgun(handgunAssetPath);
      if (!shotgunAssetPath.empty()) next.player->installShotgun(shotgunAssetPath);
      if (leon)
        next.player->inheritInventory(*leon);
      next.player->setUnlimitedHealth(true);
      next.player->setRenderInterpolation(true);
      next.replacePlayer = true;
      if (!next.world || !next.effect)
        throw std::runtime_error("Ganado test requires SAT and EAT");
      std::ifstream file(enemySpawnPath, std::ios::binary | std::ios::ate);
      if (!file || file.tellg() != re4::EnemySpawns::FileBytes)
        throw std::runtime_error("emleon00.esl is missing or invalid");
      std::vector<std::uint8_t> bytes(re4::EnemySpawns::FileBytes);
      file.seekg(0);
      if (!file.read(reinterpret_cast<char *>(bytes.data()), bytes.size()))
        throw std::runtime_error("emleon00.esl is truncated");
      const re4::EnemySpawns spawns(bytes);
      const auto *spawn = spawns.find(re4::EnemyEncounter::TestSpawn);
      if (!spawn)
        throw std::runtime_error("Original Ganado record is missing");
      next.enemy = std::make_unique<re4::EnemyEncounter>(
          *next.world, *next.effect, *spawn, enemyAssetPath, true,
          re4::EnemyEncounter::TerrainEnemyPlacement());
      // Explicit r400 test placement; preserve the original ESL identity,
      // HP, flags and RNG index rather than claiming an original r400 spawn.
      destinationHeading =
          float(std::atan2(next.enemy->feet()[0] - pending.position[0],
                           next.enemy->feet()[2] - pending.position[2]));
      __android_log_print(ANDROID_LOG_INFO, "RE4Enemy", "ESL #37 · %s · %s",
                          next.enemy->actor().describe().c_str(),
                          next.enemy->status().c_str());
    }
    const auto oldView = re4::SaveSceneView();
    const auto oldId = activeRoomId;
    const auto oldPart = activePart;
    const bool oldWalking = walking, oldCollision = collisionView,
               oldPaused = paused;
    next.swapActive();
    activeRoomId = pending.room;
    activePart = pending.part;
    walking = true;
    collisionView = false;
    paused = false;
    if(continuing)applyCaseAmmo();
    try {
      // Scene upload can fail independently of decode/CPU validation.
      uploadPreview(true, false);
    } catch (...) {
      next.swapActive();
      activeRoomId = oldId;
      activePart = oldPart;
      walking = oldWalking;
      collisionView = oldCollision;
      paused = oldPaused;
      try {
        uploadPreview(false, false);
        re4::RestoreSceneView(oldView);
      } catch (...) {
        re4::DestroySceneGl();
      }
      throw;
    }
    commitRoomAudio(next,continuing);
    if(nextCheckpoint) {
      entryCheckpoint=std::move(nextCheckpoint);checkpointBasis=std::move(nextCheckpointBasis);
      if(!continuing)originalSaveMetadata.roomContinues=0;
    }
    if(continuing) {
      originalSaveBasis=checkpointBasis;saveAccuracyOwner=nullptr;saveAccuracyCursor=0;saveKillCursors.clear();
      pendingContinue.reset();deathTicks=0;deathRequested.store(false);continueConfirmed.store(false);subscreenOpen=false;
      {std::lock_guard<std::mutex> lock(stateMutex);deathPicture.clear();}
      pendingPickup={};audioPickupActive.store(false);saveMenuRequested.store(false);currentEnemyDropContext={};
      cockpitTicks=0;cockpitUiRemainder=0;++cockpitEpoch;
      {std::lock_guard<std::mutex> lock(saveClockMutex);originalSaveClock.reset(originalSaveMetadata.playSeconds,saveWallSeconds(),audioUiActive.load());}
    }
    if(campaignMode && activePart<8 && std::find(re4::OriginalSaveRoomIds().begin(),re4::OriginalSaveRoomIds().end(),activeRoomId)!=re4::OriginalSaveRoomIds().end())
      originalSaveMetadata.passedRooms[activeRoomId]|=std::uint8_t(0x80u>>activePart);
    if(r100Secondary && leon)r100Secondary->installPlayerMotion(*leon);
    if(dogTraps && leon)dogTraps->installPlayerMotions(*leon);
    for(const auto handle:next.feedbackVoices)soundMixer.stop(handle);
    // Original EspRoomInit discards room-owned particles. An EGL rebuild stays
    // on uploadPreview and therefore retains the live effect owner instead.
    if (muzzleEffects) muzzleEffects->clear();
    if (leon) {
      leon->setPlacement(walkProbe->position(), destinationHeading);
    }
    suspendSimulationInput();
    re4::SceneFollowHeading(destinationHeading);
    re4::SceneFollowDistance(4000);
    walkFrame = {};
    flowInfo = roomName(oldId) + " → " + roomName(activeRoomId) +
               (pending.combat ? " · Ganado melee · Leon HP infinite"
                : pending.village ? " · Village start profile · Leon HP infinite"
                                  : " · Original destination start");
    __android_log_print(ANDROID_LOG_INFO, "RE4Flow", "PASS %s Part %u",
                        flowInfo.c_str(), activePart);
    logWalk();
    beginCampaignEntry();
  } catch (const std::exception &e) {
    scenarioState=oldScenario;originalSaveMetadata=oldMetadata;inventoryCase=oldInventory;
    r100DeadIndices=oldDead;r100DroppedIndices=oldDropped;r100UsedTraps=oldTraps;
    runtimeRoomDrops=oldDrops;objectRoomStates=oldObjects;r100RadioHeard=oldRadio;r100RadioMessageNo=oldRadioNo;
    if(continuing) {
      pendingContinue.reset();subscreenOpen=true;
      continueConfirmed.store(false);{std::lock_guard<std::mutex> lock(stateMutex);deathPicture.clear();}
    }
    flowInfo = "Room transition canceled: " + std::string(e.what());
    __android_log_print(ANDROID_LOG_ERROR, "RE4Flow",
                        "%s · Current room %s retained", flowInfo.c_str(),
                        roomName(activeRoomId).c_str());
  }
  std::lock_guard<std::mutex> lock(stateMutex);
  if (roomRequest.token == pending.token) {
    roomRequest = {};
    roomCompletion = {};
  }
}
} // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_ast_re4decomp_NativeBridge_finishRoomRequest(JNIEnv *env, jclass,
                                                      jlong token, jstring path,
                                                      jstring error) {
  // CPU-only mailbox: a background worker can finish while the Surface is
  // paused and has no EGL context. GPU commits happen exclusively in draw().
  try {
    RoomCompletion completion{static_cast<std::uint64_t>(token),
                              jstringValue(env, path),
                              jstringValue(env, error)};
    std::lock_guard<std::mutex> lock(stateMutex);
    if (token > 0 && completion.token == roomRequest.token)
      roomCompletion = std::move(completion);
  } catch (const std::exception &e) {
    __android_log_print(ANDROID_LOG_ERROR, "RE4Flow",
                        "Could not apply loading result: %s",
                        e.what());
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_inspectScene(JNIEnv *env, jclass,
                                                 jstring path) {
  const char *chars = env->GetStringUTFChars(path, nullptr);
  if (!chars)
    return nullptr;
  std::string filename(chars);
  env->ReleaseStringUTFChars(path, chars);
  try {
    auto arc = re4::RoomArchive::Load(filename);
    const auto *e = arc.find("SMD");
    if (!e)
      throw std::runtime_error("No SMD geometry in this room");
    re4::Smd s(arc.copy(e - arc.entries().data()));
    std::unique_ptr<re4::Smd> common;
    if (const auto *c = arc.find("SMD", 1))
      common = std::make_unique<re4::Smd>(arc.copy(c - arc.entries().data()),
                                          s.commonRequirements());
    auto scene = re4::BuildStaticScene(s, -1, common.get());
    if (scene.vertices.empty())
      throw std::runtime_error("No supported rigid geometry");
    re4::AttachSceneTextures(s, scene, common.get());
    if (const auto *x = arc.find("SMX")) {
      re4::Smx smx(arc.copy(x - arc.entries().data()));
      re4::ApplySceneSmx(smx, scene);
    }
    auto report =
        scene.describe() +
        "SMX material colors: " + std::to_string(scene.materialColorTriangles) +
        " triangles\nBase images: " + std::to_string(scene.texturedTriangles) +
        " · unresolved: " + std::to_string(scene.fallbackTriangles) + "\n";
    if (const auto *satEntry = arc.find("SAT", 0)) {
      re4::Sat sat(arc.copy(satEntry - arc.entries().data()));
      report += sat.describe();
    }
    return env->NewStringUTF(report.c_str());
  } catch (const std::exception &e) {
    jclass type = env->FindClass("java/io/IOException");
    if (type)
      env->ThrowNew(type, e.what());
    return nullptr;
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_ast_re4decomp_NativeBridge_configureGraphics(JNIEnv *env,jclass,jint msaa,jint af,jboolean mipmapping,jboolean roomFps,jboolean gyro,jboolean dof) {
 re4::userDepthOfField=dof==JNI_TRUE;
 gyroEnabled.store(gyro==JNI_TRUE && gyroPresent.load());
 re4::userMipmapping=mipmapping==JNI_TRUE;re4::userRoomFps.store(roomFps==JNI_TRUE);
 re4::requestedMsaa=(msaa==2 || msaa==4)?msaa:0;
 re4::userAf=(af==2 || af==4 || af==8)?af:1;
 const int actual=re4::PrepareMsaaGl(width,height);
 // Reject unsupported modes explicitly; don't silently retry allocations every frame.
 if(actual!=re4::requestedMsaa)re4::requestedMsaa=actual;
 const bool msaaUnavailable=actual!=int(msaa);
 std::string message=actual?std::to_string(actual)+"x MSAA":"No MSAA";
 GLfloat maximum=1;const auto *ext=reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
 if(ext && (std::strstr(ext,"GL_EXT_texture_filter_anisotropic") || std::strstr(ext,"GL_ARB_texture_filter_anisotropic")))glGetFloatv(0x84ff,&maximum);
 re4::userAf=std::min(re4::userAf,int(maximum));
 message+=" / "+std::to_string(re4::userAf)+"x AF / Mipmapping "+(re4::userMipmapping?"ON":"OFF")+" / Room/FPS "+(re4::userRoomFps.load()?"ON":"OFF");
 message+=" / Depth of Field "+std::string(re4::userDepthOfField?"ON":"OFF");
 message+=" / Gyroscope "+std::string(gyroEnabled.load()?"ON":gyroPresent.load()?"OFF":"unavailable");
 __android_log_print(ANDROID_LOG_INFO,"RE4Graphics","Mipmapping %s; Room/FPS %s; Gyroscope %s; Depth of Field %s",re4::userMipmapping?"ON":"OFF",re4::userRoomFps.load()?"ON":"OFF",gyroEnabled.load()?"ON":"OFF",re4::userDepthOfField?"ON":"OFF");
 __android_log_print(ANDROID_LOG_INFO,"RE4Graphics","Requested MSAA %d / AF %d; applied MSAA %d / AF %d; AF maximum %.0f; framebuffer %dx%d; renderer %s",int(msaa),int(af),actual,re4::userAf,maximum,width,height,reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
 if(msaaUnavailable || re4::userAf!=int(af))message="Requested mode unavailable; "+message;
 return env->NewStringUTF(message.c_str());
}
extern "C" JNIEXPORT jintArray JNICALL
Java_com_ast_re4decomp_NativeBridge_graphicsOptions(JNIEnv *env,jclass) {
 __android_log_print(ANDROID_LOG_INFO,"RE4Graphics","Settings: MSAA %d; AF %d; Mipmapping %s; Room/FPS %s; Gyroscope %s; Depth of Field %s",re4::requestedMsaa,re4::userAf,re4::userMipmapping?"ON":"OFF",re4::userRoomFps.load()?"ON":"OFF",gyroEnabled.load()?"ON":"OFF",re4::userDepthOfField?"ON":"OFF");
 const jint values[]{re4::requestedMsaa,re4::userAf,re4::userMipmapping?1:0,re4::userRoomFps.load()?1:0,gyroEnabled.load()?1:0,re4::userDepthOfField?1:0};auto out=env->NewIntArray(6);if(out)env->SetIntArrayRegion(out,0,6,values);return out;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_ast_re4decomp_NativeBridge_roomFpsVisible(JNIEnv *,jclass) {
 return re4::userRoomFps.load()?JNI_TRUE:JNI_FALSE;
}
