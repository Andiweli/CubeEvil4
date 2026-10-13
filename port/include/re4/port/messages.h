#pragma once
#include "re4/port/model_types.h"
#include <memory>

namespace re4::port {
class GameplayHeaps;
// Backend-neutral GX initialization state. Storage is pinned; no GC address is
// packed into a pointer-sized native object and no GPU upload is implied.
struct MessageTexture {
    ResourceLease pixels;
    u16 width{}, height{};
    u32 format{}, wrapS{}, wrapT{}, tlutName{};
    u8 minFilter = 1, magFilter = 1; // GXInitTexObj without mipmaps: linear.
    bool indexed{}, initialized{};
};
struct MessagePalette {
    ResourceLease entries;
    u32 format{};
    u16 count{};
    bool initialized{};
};
struct MessageFontSnapshot {
    bool loaded{};
    u16 textureWidth{}, textureHeight{};
    u8 charWidth{}, charHeight{};
    ResourceLease widths;
    ResourceLease tplSource;
    std::vector<MessageTexture> textures;
    std::vector<MessagePalette> palettes;
};
struct MessageLayoutSnapshot { s8 width{}, height{}; u16 charGap{}, lineGap{}; bool queueBound{}; };
class MessageRuntime final {
  public:
    MessageRuntime(EngineLoader &, GameplayHeaps &, NativeGameState &, u8 &systemLanguage,
                   std::shared_ptr<const ImportedFiles>, u8 disc);
    ~MessageRuntime();
    MessageRuntime(const MessageRuntime &) = delete;
    MessageRuntime &operator=(const MessageRuntime &) = delete;
    // Task-only original CoreDataRead + cMes.init. English policy selects the
    // original pSys language 1; all disc reads use the existing Dvd queues.
    void initialize();
    void gameInit();
    void stageInit();
    void retireStageFont(); // Owner-thread heap handoff; live snapshots retain their leases.
    void clear(); // Frame-boundary release; snapshots independently retain leases.
    bool ready() const;
    void setLayout(int slot, int layout);
    void releaseFont(int slot);
    void reloadCommonFont();
    MessageFontSnapshot font(int slot) const;
    MessageLayoutSnapshot layout(int slot) const;
    s16 glyphWidth(int slot, int glyph, s8 *left, s8 *right);
    int messageCount(int type);
    ResourceLease message(int type, int number); // Immutable original BE bytes.
    u16 firstNativeWord(int type, int number); // Actual original MessageData lookup.
    std::vector<u16> nativeMessageWords(int type,int number);
    void collectCancelledReads(); // Owner frame boundary; drains abandoned DVD requests.
    ResourceLease coreBody() const;
    std::vector<MessageTexture> coreTextures() const;
    MessagePalette thermalPalette() const;
    const std::vector<std::string> &trace() const;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace re4::port
