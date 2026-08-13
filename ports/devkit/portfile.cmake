set(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../devkit")
set(VCPKG_POLICY_SKIP_COPYRIGHT_CHECK enabled)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DDEVKIT_BUILD_TESTS=OFF
        -DDEVKIT_BUILD_EXAMPLES=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH share/devkit)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

if(EXISTS "${SOURCE_PATH}/LICENSE")
    vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
endif()
