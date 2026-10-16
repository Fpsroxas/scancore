# Desktop integration files for Linux installs and AppImages.
if(UNIX AND NOT APPLE AND NOT ANDROID)
    install(FILES ${PROJECT_SOURCE_DIR}/packaging/linux/com.mediumdev.scancore.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${PROJECT_SOURCE_DIR}/resources/branding/scancore.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME com.mediumdev.scancore.svg)
    install(FILES ${PROJECT_SOURCE_DIR}/packaging/linux/com.mediumdev.scancore.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
endif()

# CPack: a tarball everywhere; installers are built by the release workflow
# with the platform tools (linuxdeploy, Inno Setup, macdeployqt).
set(CPACK_PACKAGE_NAME "scancore")
set(CPACK_PACKAGE_VENDOR "Medium Dev")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_GENERATOR "TGZ")
include(CPack)
