include(../config.pri)

 # Use the full API for all plugins.
CONFIG += dengplugin_libcore_full

deng_noclient {
    CONFIG += libgui_headers_only
}

macx {
    CONFIG += lib_bundle
    QMAKE_BUNDLE_EXTENSION = .bundle
    macx-xcode {
        xcBundle.name = WRAPPER_EXTENSION
        xcBundle.value = bundle
        QMAKE_MAC_XCODE_SETTINGS += xcBundle
    }
    xcodeFinalizeAppBuild()
    
    gamedata.path = Contents/Resources
    macx-xcode: gamedata.path = Versions/Current/Resources
}
unix:!macx {
    INSTALLS += target
    target.path = $$DENG_PLUGIN_LIB_DIR
}
win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll

    # Deploy plugin DLLs to the plugins folder.
    DLLDESTDIR = $$DENG_PLUGIN_LIB_DIR
}

INCLUDEPATH += $$DENG_API_DIR

!dengplugin_libcore_full {
    # The libcore C wrapper can be used from all plugins.
    DEFINES += DENG_NO_QT DENG2_C_API_ONLY
    include(../dep_core_cwrapper.pri)
}
else: include(../dep_core.pri)

include(../dep_doomsday.pri)
include(../dep_legacy.pri)

*-gcc*|*-g++*|*-clang* {
    QMAKE_CXXFLAGS_WARN_ON += -Wno-missing-field-initializers
}

deng_mingw: QMAKE_CFLAGS_WARN_ON += \
    -Wno-unused-parameter \
    -Wno-missing-field-initializers \
    -Wno-missing-braces \
    -Wno-unused-function
