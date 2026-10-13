#pragma once
#include "consumer_original.h"
#include "re4/port/messages.h"
#include "message_texture_original.h"
#include <map>
#include <thread>

namespace re4::port {
struct MessageFontTexture { MessageTexture tex; MessagePalette tlut; Mtx mtx{}; MessageTexHeader *pTex{}; MessageTpl *pTpl{}; };
struct MessageFontCpu {
    u32 be_flag{};
    MessageTpl *m_tpl{};
    MessageFontTexture m_mTex[2];
    u8 *pWidth{};
    u16 m_tex_w{}, m_tex_h{};
    u8 m_char_w{}, m_char_h{};
    std::shared_ptr<MessageTpl> storage;
    ResourceLease widths;
    void create(int, int, MessageTpl *, u8 *);
    void destroy();
    s16 getSize(s16, s8 *, s8 *);
    void requireResolvedPalette(MessageTpl *p);
    void releasePalette();
};
struct MessageGlyph { s16 px{},py{}; u16 mes{}; u8 width{},height{}; u32 col{}; MessageFontCpu *m_p_font{}; };
struct MessageSlotCpu {
    u32 be_flag{}, m_state{};
    s8 m_font_w{},m_font_h{};
    u16 m_line_gap{},m_char_gap{};
    MessageGlyph *m_queue{};
    void setDie();
    void setFontSize(s16,s16);
    void setLineGap(u16);
    void setFontGap(u16);
    void registQueue(MessageGlyph *);
};
// Native named fields used by original initialization. This is not a Message
// wire-layout overlay; unported Move/Trans/commands cannot be called here.
struct MessageControlCpu {
    EngineLoader &loader;
    GameplayHeaps &heaps;
    NativeGameState *pG;
    std::shared_ptr<const ImportedFiles> files;
    u8 disc;
    struct System { u8 &language; } system;
    System *pSys=&system;
    MessageIndexCpu MesData;
    MessageFontCpu MesFont[4];
    MessageSlotCpu m_Msg[16];
    MessageGlyph MsgQueue[3][0x100];
    void *m_font_addr[4]{};
    u32 m_state{},m_stop{};
    std::vector<GameplayHeaps::Allocation> allocations;
    std::map<void *,u32> loadedSizes;
    std::shared_ptr<const LoadedFile> core;
    AssetIndex coreIndex;
    std::map<u32,std::shared_ptr<MessageTpl>> corePalettes;
    struct MessageTable { ResourceLease raw; std::vector<u32> native; };
    std::map<u32,MessageTable> tables;
    MessageTexture Specular[9],IndTex[2],GlobalIlmTex[5];
    MessagePalette ThermoTlut;
    bool initialized{};
    std::vector<std::string> trace;
    const std::thread::id owner=std::this_thread::get_id();
    struct Read { LoadRequest request; void *destination{}; std::size_t capacity{}; bool core{}; TaskHandle caller; };
    std::map<int,Read> reads;
    std::vector<LoadRequest> abandoned;
    int nextRead=1;
    struct ReadInfo { u32 read_size[1][1]{}; };
    struct DvdFacade {
        MessageControlCpu *context;
        int FileExistCheck(const char *, u32 *);
        int ReadCheck(int, int, int, int);
        int ReadCheck(int, ReadInfo *);
    } Dvd{this};
    MessageControlCpu(EngineLoader &l,GameplayHeaps &h,NativeGameState &g,u8 &language,
                      std::shared_ptr<const ImportedFiles> f,u8 d):loader(l),heaps(h),pG(&g),files(std::move(f)),disc(d),system{language} {}
    void check(bool task=false) const;
    void reset();
    void collectCancelledReads();
    void *mem_alloc(u32,const char *,int,int,int);
    int DvdReadN(const char *,void *,int,int,int,int,const char *,int);
    int readCore(int,u16);
    void coreReadBegin();
    int finishRead(int,ReadInfo *);
    void *coreArc(u32);
    void setupFontView(int,int,u8 *,int);
    void CoreDataRead();
    void SpecularInit(MessageTpl *,MessageTpl *,MessageTpl *,MessageTpl *);
    void GlobalIlmTexInit(MessageTpl *);
    void init();
    void gameInit();
    void stageInit();
    void loadStageFont();
    void *pStFnt{};
    void loadCommonFont();
    void loadSystemFont();
    void setupFont(int,int,u8 *,int);
    void releaseFont(int);
    int loadFont(int,int,const char *,int);
    void setLanguage(int);
    void setLayout(int,int);
    void setFontSize(int,s16,s16);
    void setState(u32);
    void unsetState(u32);
    int checkState(u32);
    void Delete(int);
    void Clear();
    void MesRegistQueue(int,MessageGlyph *);
    void MesReleaseQueue(int);
    void setFontGap(int,u16);
    void setLineGap(int,u16);
    void TaskSleep(int n) { loader.scheduler().sleep(n); }
};
} // namespace re4::port
