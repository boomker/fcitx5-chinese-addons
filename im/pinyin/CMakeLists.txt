set(PINYIN_SOURCES
    pinyin.cpp
    customphrase.cpp
    symboldictionary.cpp
    workerthread.cpp
    pinyincandidate.cpp
    pinyinenginefactory.cpp
)

add_fcitx5_addon(pinyin ${PINYIN_SOURCES})
target_link_libraries(pinyin Fcitx5::Core Fcitx5::Config LibIME::Pinyin Fcitx5::Module::Punctuation Fcitx5::Module::QuickPhrase Fcitx5::Module::Notifications Fcitx5::Module::Spell Fcitx5::Module::PinyinHelper Pthread::Pthread)

if (TARGET Fcitx5::Module::LuaAddonLoader)
    target_compile_definitions(pinyin PRIVATE -DFCITX_HAS_LUA)
    target_link_libraries(pinyin Fcitx5::Module::LuaAddonLoader)
    install(FILES pinyin.lua DESTINATION "${FCITX_INSTALL_PKGDATADIR}/lua/imeapi/extensions"
            COMPONENT config)
endif()

install(TARGETS pinyin DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")
fcitx5_translate_desktop_file(pinyin.conf.in pinyin.conf)
fcitx5_translate_desktop_file(shuangpin.conf.in shuangpin.conf)
fcitx5_translate_desktop_file(shuangpin-custom.conf.in shuangpin-custom.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pinyin.conf" "${CMAKE_CURRENT_BINARY_DIR}/shuangpin.conf" "${CMAKE_CURRENT_BINARY_DIR}/shuangpin-custom.conf" DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod"
        COMPONENT config)

# Each built-in Shuangpin scheme is a first-class input method entry, so users
# can add several schemes side by side from the input method add dialog.
set(SHUANGPIN_SCHEMES
    "xiaohe|Xiaohe Shuangpin"
    "ms|MS Shuangpin"
    "ziguang|Ziguang Shuangpin"
    "gb|GB Shuangpin"
    "ziranma|Ziranma Shuangpin"
    "abc|ABC Shuangpin"
    "zhongwenzhixing|Zhongwenzhixing Shuangpin"
    "pinyinjiajia|PinyinJiajia Shuangpin")
set(SHUANGPIN_SCHEME_CONFS)
foreach(SHUANGPIN_SCHEME IN LISTS SHUANGPIN_SCHEMES)
    string(REPLACE "|" ";" SHUANGPIN_PARTS "${SHUANGPIN_SCHEME}")
    list(GET SHUANGPIN_PARTS 0 SHUANGPIN_ID)
    list(GET SHUANGPIN_PARTS 1 SHUANGPIN_NAME)
    set(SHUANGPIN_TEMPLATE "${CMAKE_CURRENT_BINARY_DIR}/shuangpin-${SHUANGPIN_ID}.conf.in")
    set(SHUANGPIN_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shuangpin-${SHUANGPIN_ID}.conf")
    configure_file(shuangpin-scheme.conf.in "${SHUANGPIN_TEMPLATE}" @ONLY)
    fcitx5_translate_desktop_file("${SHUANGPIN_TEMPLATE}" "${SHUANGPIN_OUTPUT}")
    list(APPEND SHUANGPIN_SCHEME_CONFS "${SHUANGPIN_OUTPUT}")
endforeach()
install(FILES ${SHUANGPIN_SCHEME_CONFS} DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod"
        COMPONENT config)

configure_file(pinyin-addon.conf.in.in pinyin-addon.conf.in)
fcitx5_translate_desktop_file("${CMAKE_CURRENT_BINARY_DIR}/pinyin-addon.conf.in" pinyin-addon.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pinyin-addon.conf" RENAME pinyin.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon"
        COMPONENT config)

install(FILES "symbols" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/pinyin" COMPONENT config)
install(FILES "sp.dat.example" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/pinyin" COMPONENT config)

if (ENABLE_DATA)

  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/chaizi.dict"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/chaizi.txt" LibIME::pinyindict
    COMMAND LibIME::pinyindict "${CMAKE_CURRENT_SOURCE_DIR}/chaizi.txt" "${CMAKE_CURRENT_BINARY_DIR}/chaizi.dict")
  add_custom_target(chaizi-dict ALL DEPENDS "chaizi.dict")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/chaizi.dict" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/pinyin")
endif()
