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
        "void cDataUnit::setLoadToAram()" "void cDataUnit::checkLoadToAram()"
        "void cDataUnit::checkAramToMram()" "void cDataUnit::checkMramToAram()"
        "void cDataUnit::checkAramToAram()" "void cDataUnit::checkMramToMram()"
        "void cDataUnit::checkCondition()" "int cDataUnit::waitUseOk()" "int cDataUnit::waitLoadOk()"
        "cDataUnit* cDataCtrl::setData(char* name)" "cDataUnit* cDataCtrl::getNewUnit()"
        "u32 cDataCtrl::getAramFree(u32 size)" "void cDataCtrl::initDataUnit()"
        "void cDataCtrl::deleteAll()" "void cDataCtrl::setAramSort(int flag)"
        "int cDataCtrl::checkAramSort()" "void cDataCtrl::initDummyId()"
        "void cDataCtrl::setDummyId(int dummy_id)" "void cDataCtrl::checkDummyId()"
        "void cDataCtrl::check()")
        re4_extract_function("${root}/src/game/datactrl.cpp" "${signature}" definition)
        # Logical ARAM offsets and full-width MRAM pointers are distinct domains.
        # Only the old ABI casts change; commands and completion states are original.
        string(REPLACE "(u32) m_addr" "nativeAddress(m_addr)" definition "${definition}")
        string(REPLACE "(u32) m_dest_addr" "nativeAddress(m_dest_addr)" definition "${definition}")
        string(REPLACE "(u32) m_arg_addr" "nativeAddress(m_arg_addr)" definition "${definition}")
        string(REPLACE "(int) m_dest_addr" "static_cast<int>(aramOffset(m_dest_addr))" definition "${definition}")
        string(REPLACE "(void*) DC.getAramFree(m_size)" "aramPointer(DC.getAramFree(m_size))" definition "${definition}")
        foreach(address "u->getArgAddr()" "u->getAddr()" "u->getDestAddr()" "tbl[i]->getAddr()" "tbl[j]->getAddr()")
            string(REPLACE "(u32) ${address}" "aramOffset(${address})" definition "${definition}")
        endforeach()
        string(REPLACE "(void*) base" "aramPointer(base)" definition "${definition}")
        string(REPLACE "AramArea tbl[32];" "AramArea tbl[64];\n    size = alignedSize(size);\n    if (size > ARAM_END - ARAM_FREE_BASE) return 0;" definition "${definition}")
        string(REPLACE "= u->getSize();" "= alignedSize(u->getSize());" definition "${definition}")
        string(REPLACE "base += tbl[i]->getSize();" "base += alignedSize(tbl[i]->getSize());" definition "${definition}")
        # getDestAddr is MRAM in condition 5/7: never treat it as an ARAM offset.
        string(REPLACE "base == aramOffset(u->getDestAddr())" "base == nativeAddress(u->getDestAddr())" definition "${definition}")
        string(REPLACE "while (Aram.TransCheck(m_id) != 1) {\n        }" "waitAram(m_id);" definition "${definition}")
        string(REPLACE "checkMallocRelease();\n        pLog->err" "checkMallocRelease();\n        m_condition = COND_NO_DATA; m_addr = nullptr; m_dest_addr = nullptr;\n        pLog->err" definition "${definition}")
        if(signature STREQUAL "void cDataUnit::setLoadToAram()")
            string(REPLACE "setErr(3);\n            checkMallocRelease();" "setErr(3);\n            if (m_condition != COND_MRAM_OK) checkMallocRelease();" definition "${definition}")
            string(REPLACE "case COND_ARAM_OK:\n        if" "case COND_ARAM_OK:\n        if (m_arg_addr == nullptr || m_arg_addr == m_addr) { m_command = CMND_NONE; break; }\n        if" definition "${definition}")
        endif()
        if(signature STREQUAL "void cDataUnit::checkLoadToMram()" OR signature STREQUAL "void cDataUnit::checkLoadToAram()")
            string(REPLACE "m_addr = m_dest_addr;" "m_addr = m_dest_addr;\n        unitFileReady(this);" definition "${definition}")
        endif()
        # The decompilation frees before copying in the MRAM move branch. Retain the
        # intended bytes and move the release after the copy (tested on owned data).
        string(REPLACE "checkMallocRelease();\n                memcpy(m_arg_addr, m_addr, m_size);"
            "checkedMove(m_arg_addr, m_addr, m_size);\n                checkMallocRelease();" definition "${definition}")
        string(REPLACE "checkMallocRelease();\n            memcpy(m_fix_addr, m_addr, m_size);"
            "checkedMove(m_fix_addr, m_addr, m_size);\n            checkMallocRelease();" definition "${definition}")
        # Stale fixed addresses cannot survive a reused unit; old stack/heap resets
        # zeroed them globally, whereas the native domain is longer lived.
        string(REPLACE "u->setBeAlive(1);" "*u = cDataUnit{};\n            u->setBeAlive(1);\n            resourceRegistered(u);" definition "${definition}")
        # A native per-unit failure must not strand other units/frames in a HALT.
        string(REPLACE "u->checkCommand();\n                u->checkCondition();"
            "try { u->checkCommand(); u->checkCondition(); } catch (...) { nativeUnitFault(u); }" definition "${definition}")
        string(REPLACE "memclr_asm(u, sizeof(cDataUnit));" "*u = cDataUnit{};" definition "${definition}")
        if(signature STREQUAL "int cDataUnit::setClear()")
            string(REPLACE "m_condition = COND_NO_DATA;" "m_condition = COND_NO_DATA;\n    unitFileCleared(this);" definition "${definition}")
        endif()
        string(REPLACE "\n{" "\n{\n    loaderTrace(\"${signature}\");" definition "${definition}")
        string(APPEND definitions "${definition}\n")
    endforeach()
    foreach(signature "int cAram::DmaTransReq(int type, u32 src, u32 dst, u32 len, int mode)"
        "cAramQueue* cAram::pullAramQueue(int* id)" "int cAram::TransCheck(int id)")
        re4_extract_function("${root}/src/game/dvd.cpp" "${signature}" definition)
        string(REPLACE "int type, u32 src, u32 dst" "int type, std::uintptr_t src, std::uintptr_t dst" definition "${definition}")
        string(REPLACE "r = pullAramQueue(&no);" "checkAramRequest(type, src, dst, len);\n    r = pullAramQueue(&no);" definition "${definition}")
        string(REPLACE "memclr_asm(r, sizeof(cAramQueue));" "*r = cAramQueue{};" definition "${definition}")
        string(REPLACE "cAramQueue* r = &AramQueue[id];" "if (id < 0 || id >= 16) return -1;\n    cAramQueue* r = &AramQueue[id];" definition "${definition}")
        string(REPLACE "pCur_queue = r;" "while (pCur_queue) advance(context().limits.transferBytesPerFrame);\n        pCur_queue = r;" definition "${definition}")
        string(REPLACE "\n{" "\n{\n    loaderTrace(\"${signature}\");" definition "${definition}")
        string(APPEND definitions "${definition}\n")
    endforeach()
    foreach(signature "void cBlockUnit::setBlockCommand(int cmd, int a)" "void cBlockUnit::setBlockLoadToMram()"
        "int cBlockUnit::checkBlockLoadToMramSet()" "int cBlockUnit::checkBlockLoadToAramSet()"
        "int cBlockUnit::checkBlockLoadToAram()" "int cBlockUnit::checkBlockDelete()")
        re4_extract_function("${root}/src/game/block.cpp" "${signature}" definition)
        string(REPLACE "arg = a;" "if (a < 0 || a > 255) throw std::invalid_argument(\"block command argument\");\n    arg = static_cast<u8>(a);" definition "${definition}")
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
    string(REPLACE "void setNBlkStop(int flag)" "cDataUnit* nativeUnit(std::size_t slot) { return slot < 32 ? &m_DataUnit[slot] : nullptr; }\n    void setNBlkStop(int flag)" header "${header}")
    string(REPLACE "#endif" "}\n#endif" header "${header}")
    file(CONFIGURE OUTPUT "${destination}/include/datactrl_native.h" CONTENT "${header}" @ONLY)
    file(CONFIGURE OUTPUT "${destination}/loader_control.cpp" CONTENT
        "// Generated from fingerprinted original definitions; adaptations in LoaderSources.cmake.\n#include \"loader_original.h\"\nnamespace re4::port::loading {\nstruct AramArea { u32 addr; u32 size; };\nint roomReadCheck(int,void**);\n${definitions}\n}\n" @ONLY)
endfunction()
