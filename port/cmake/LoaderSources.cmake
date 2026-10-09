function(re4_prepare_loader root destination)
    # Full original definitions, with explicit ABI/safety changes only. No original
    # file is edited; #line retains traceability into the decompilation.
    set(definitions "")
    file(READ "${root}/src/game/dvd.cpp" dvd_source)
    string(REPLACE "\r\n" "\n" dvd_source "${dvd_source}")
    string(FIND "${dvd_source}" "FileTblEntry FileTbl[] = {" table_start)
    string(SUBSTRING "${dvd_source}" ${table_start} -1 table_tail)
    string(FIND "${table_tail}" "\n};" table_end)
    math(EXPR table_size "${table_end} + 3")
    string(SUBSTRING "${table_tail}" 0 ${table_size} table)
    string(APPEND definitions "const ${table}\nconst std::size_t FileTblCount = sizeof(FileTbl)/sizeof(FileTbl[0]);\n")
    foreach(signature "int DvdRead(int fileNo, void* dst, u32 aram, u32 ofs, u32 length, int mode, const char* file, int line)"
        "int DvdReadN(const char* name, void* dst, int a, int b, int c, int mode, const char* file, int line)")
        re4_extract_function("${root}/src/game/dvd.cpp" "${signature}" definition)
        string(REPLACE "memclr_asm(w, sizeof(DVD_REQ_WORK));" "*w = DVD_REQ_WORK{};" definition "${definition}")
        string(REPLACE "w->file_no = fileNo;" "if (fileNo < 0 || static_cast<std::size_t>(fileNo) >= FileTblCount) return -1;\n    w->file_no = static_cast<u16>(fileNo);" definition "${definition}")
        string(REPLACE "sprintf(w->fname, \"%s\", name);" "if (!name || std::strlen(name) >= sizeof(w->fname) || a < 0 || b < 0 || c < 0) return -1;\n    std::snprintf(w->fname, sizeof(w->fname), \"%s\", name);" definition "${definition}")
        string(REPLACE "sprintf(w->reqfile, \"%s\", file);" "std::snprintf(w->reqfile, sizeof(w->reqfile), \"%s\", file ? file : \"\");" definition "${definition}")
        string(REPLACE "w->mode = mode;" "w->mode = static_cast<u16>(mode);" definition "${definition}")
        string(REPLACE "\n{" "\n{\n    loaderTrace(\"${signature}\");" definition "${definition}")
        string(APPEND definitions "${definition}\n")
    endforeach()
    foreach(signature "int cDvdQueue::LinkQueue()" "void cDvdQueue::PushQueue()"
        "int cDvd::ReadReq()" "void cDvd::ReadProc()" "void cDvd::readProcMain(cDvdQueue* pQueue)"
        "cDvdQueue* cDvd::pullReadQueue()" "int cDvd::readCheckMain(int id, DVD_READINFO* pInfo)"
        "void DvdReadProc()")
        re4_extract_function("${root}/src/game/dvd.cpp" "${signature}" definition)
        string(REPLACE "memclr_asm(q, sizeof(cDvdQueue));" "resetQueue(q);" definition "${definition}")
        string(REPLACE "q->m_Id = ReadID++;" "q->m_Id = nextReadID();" definition "${definition}")
        string(REPLACE "q->Initialize();" "try { q->Initialize(); } catch (...) { q->PushQueue(); throw; }" definition "${definition}")
        string(REPLACE "while (pQueue->Read() == 1) {" "while (pQueue->Read() == 1) {\n        boundedDvdStep(pQueue);" definition "${definition}")
        # Native FileRead errors are already published by Read(); no physical-drive
        # error screen is emulated. Explicit blocking reads wait for the worker.
        string(REPLACE "ErrCheck(-1, 0);" "std::this_thread::yield();" definition "${definition}")
        string(REPLACE "ResetFlag(1);" "ResetFlag(1);\n    releaseQueueStorage(this);" definition "${definition}")
        string(REPLACE "\n{" "\n{\n    loaderTrace(\"${signature}\");" definition "${definition}")
        string(APPEND definitions "${definition}\n")
    endforeach()
    foreach(signature "void cDataUnit::setCommand(int cmd, void* arg, u8 wait)"
        "int cDataUnit::getCommand()" "void cDataUnit::setCondition(int c)" "int cDataUnit::getCondition()"
        "void cDataUnit::checkMallocRelease()" "void cDataUnit::setMallocInfo(int set, void* malloc_addr)"
        "void cDataUnit::fixMramAddr(void* a)" "int cDataUnit::isUseOk()" "int cDataUnit::isLoadOk()"
        "void cDataUnit::setLoadToMram()" "int cDataUnit::setClear()" "int cDataUnit::setDelete()"
        "void cDataUnit::checkLoadToMram()" "void cDataUnit::checkCommand()"
        "cDataUnit* cDataCtrl::setData(char* name)" "cDataUnit* cDataCtrl::getNewUnit()")
        re4_extract_function("${root}/src/game/datactrl.cpp" "${signature}" definition)
        # Native buffers are not GameCube ARAM addresses. Reject the still-unported
        # DMA branch explicitly; never truncate a pointer or report a fake transfer.
        string(REPLACE "Aram.DmaTransReq(1, (u32) m_addr, (u32) m_dest_addr, m_size, m_mode)" "unsupportedAram()" definition "${definition}")
        string(REPLACE "Aram.DmaCancel(m_id)" "unsupportedAram()" definition "${definition}")
        # The decompilation frees before copying in the MRAM move branch. Retain the
        # intended bytes and move the release after the copy (tested on owned data).
        string(REPLACE "checkMallocRelease();\n                memcpy(m_arg_addr, m_addr, m_size);"
            "memmove(m_arg_addr, m_addr, m_size);\n                checkMallocRelease();" definition "${definition}")
        string(REPLACE "checkMallocRelease();\n            memcpy(m_fix_addr, m_addr, m_size);"
            "memmove(m_fix_addr, m_addr, m_size);\n            checkMallocRelease();" definition "${definition}")
        string(REPLACE "memclr_asm(u, sizeof(cDataUnit));" "*u = cDataUnit{};" definition "${definition}")
        string(REPLACE "\n{" "\n{\n    loaderTrace(\"${signature}\");" definition "${definition}")
        string(APPEND definitions "${definition}\n")
    endforeach()
    re4_extract_function("${root}/src/game/block.cpp" "int cBlock::getBlockWork(u8 no)" definition)
    string(REPLACE "\n{" "\n{\n    loaderTrace(\"int cBlock::getBlockWork(u8 no)\");" definition "${definition}")
    string(APPEND definitions "${definition}\n")
    re4_extract_function("${root}/src/game/read.cpp" "void ReadAreaData()" definition)
    string(REPLACE "(void*) ((u32) MemGetHeapEndAddr(MemGetCurrentHeap()) - READ_BUFF_OFS)"
        "roomReadDestination()" definition "${definition}")
    # Original only checks req<0 after its wait, which otherwise loops forever.
    string(REPLACE "Dvd.ReadCheck(req, 0, 0, &in_data_addr)" "roomReadCheck(req, &in_data_addr)" definition "${definition}")
    string(REPLACE "iTaskExec(decodeData);" "while (!iTaskExec(decodeData)) { TaskSleep(1); }" definition "${definition}")
    string(REPLACE "\n{" "\n{\n    loaderTrace(\"void ReadAreaData()\");" definition "${definition}")
    string(APPEND definitions "${definition}\n")
    file(READ "${root}/include/datactrl.h" header)
    string(REPLACE "extern cDataCtrl DC;" "" header "${header}")
    string(REPLACE "class cDataUnit {" "namespace re4::port::loading {\nclass cDataUnit {" header "${header}")
    string(REPLACE "cDataUnit() {}" "cDataUnit() = default;" header "${header}")
    string(REPLACE "#endif" "}\n#endif" header "${header}")
    file(CONFIGURE OUTPUT "${destination}/include/datactrl_native.h" CONTENT "${header}" @ONLY)
    file(CONFIGURE OUTPUT "${destination}/loader_control.cpp" CONTENT
        "// Generated from fingerprinted original definitions; adaptations in LoaderSources.cmake.\n#include \"loader_original.h\"\nnamespace re4::port::loading {\nint roomReadCheck(int,void**);\n${definitions}\n}\n" @ONLY)
endfunction()
