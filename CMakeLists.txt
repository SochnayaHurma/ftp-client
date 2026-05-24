cmake_minimum_required(VERSION 3.20)

project(SecureTransfer VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/forms)

option(SECURETRANSFER_WITH_CURL "Enable optional FTP/SFTP backend through libcurl" ON)
option(SECURETRANSFER_BUILD_TESTS "Build Qt unit tests" ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

set(SECURETRANSFER_APP_SOURCES
    src/main.cpp
    forms/MainWindow.ui
    forms/RemoteConnectionDialog.ui
    forms/HistoryDialog.ui
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/FormatUtils.h
    src/ui/FormatUtils.cpp
    src/ui/FileFilterMode.h
    src/ui/FileSystemFilterProxyModel.h
    src/ui/FileSystemFilterProxyModel.cpp
    src/ui/RemoteFileModel.h
    src/ui/RemoteFileModel.cpp
    src/ui/RemoteConnectionDialog.h
    src/ui/RemoteConnectionDialog.cpp
    src/ui/HistoryDialog.h
    src/ui/HistoryDialog.cpp
    src/core/TransferTypes.h
    src/core/TransferQueue.h
    src/core/PreflightAnalyzer.h
    src/core/PreflightAnalyzer.cpp
    src/core/TransferManager.h
    src/core/TransferManager.cpp
    src/core/Journal.h
    src/core/Journal.cpp
    src/core/storage/StorageTypes.h
    src/core/storage/IStorageBackend.h
    src/core/storage/LocalStorageBackend.h
    src/core/storage/LocalStorageBackend.cpp
    src/core/storage/RemoteConnectionProfile.h
    src/core/storage/RemoteConnectionProfile.cpp
    src/core/storage/ConnectionProfileStore.h
    src/core/storage/ConnectionProfileStore.cpp
    src/core/storage/CurlConnectionTester.h
    src/core/storage/CurlConnectionTester.cpp
    src/core/storage/CurlRemoteStorage.h
    src/core/storage/CurlRemoteStorage.cpp
)

add_executable(SecureTransferLite ${SECURETRANSFER_APP_SOURCES})
target_include_directories(SecureTransferLite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(SecureTransferLite PRIVATE Qt6::Widgets Qt6::Sql)


if(SECURETRANSFER_WITH_CURL)
    find_package(CURL QUIET)


    if(NOT CURL_FOUND)
        include(FetchContent)

        set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
        set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
        set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
        if(WIN32)
                set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
            elseif(APPLE)
                set(CURL_USE_SECTRANSP ON CACHE BOOL "" FORCE)
            else()
                set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
        endif()

        FetchContent_Declare(
            curl
            GIT_REPOSITORY https://github.com/curl/curl.git
            GIT_TAG curl-8_7_1
        )

        FetchContent_MakeAvailable(curl)
        if(NOT TARGET CURL::libcurl)
            add_library(CURL::libcurl ALIAS libcurl)
        endif()

    endif()
    target_compile_definitions(SecureTransferLite PRIVATE SECURETRANSFER_WITH_CURL)
    target_link_libraries(SecureTransferLite PRIVATE CURL::libcurl)
endif()

if(SECURETRANSFER_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    set(SECURETRANSFER_TEST_CORE_SOURCES
        src/core/TransferTypes.h
        src/core/TransferQueue.h
        src/core/PreflightAnalyzer.h
        src/core/PreflightAnalyzer.cpp
        src/core/TransferManager.h
        src/core/TransferManager.cpp
        src/core/Journal.h
        src/core/Journal.cpp
        src/core/storage/StorageTypes.h
        src/core/storage/IStorageBackend.h
        src/core/storage/LocalStorageBackend.h
        src/core/storage/LocalStorageBackend.cpp
    )

    function(add_securetransfer_test target_name source_file)
        add_executable(${target_name}
            ${source_file}
            ${SECURETRANSFER_TEST_CORE_SOURCES}
        )
        target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(${target_name} PRIVATE Qt6::Test Qt6::Sql)
        add_test(NAME ${target_name} COMMAND ${target_name})
    endfunction()

    add_securetransfer_test(TestPreflightAnalyzer tests/TestPreflightAnalyzer.cpp)
    add_securetransfer_test(TestTransferManager tests/TestTransferManager.cpp)
    add_securetransfer_test(TestJournal tests/TestJournal.cpp)
endif()
