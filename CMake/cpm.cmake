# Bootstraps CPM.cmake into ${LIB_DIR}/cpm/ (gitignored). Must run after LIB_DIR is set.

set(CPM_DOWNLOAD_VERSION 0.42.3)
set(CPM_DOWNLOAD_LOCATION "${LIB_DIR}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

# Expand relative path (and resolve ~ if ever used).
get_filename_component(CPM_DOWNLOAD_LOCATION "${CPM_DOWNLOAD_LOCATION}" ABSOLUTE)

function(download_cpm)
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
        "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
        "${CPM_DOWNLOAD_LOCATION}"
        STATUS _cpm_download_status
        TLS_VERIFY ON
    )
    list(GET _cpm_download_status 0 _cpm_download_code)
    if (NOT _cpm_download_code EQUAL 0)
        list(GET _cpm_download_status 1 _cpm_download_msg)
        file(REMOVE "${CPM_DOWNLOAD_LOCATION}")
        message(FATAL_ERROR
            "Failed to download CPM.cmake (HTTP/network error ${_cpm_download_code}): ${_cpm_download_msg}\n"
            "URL: https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake\n"
            "If you are offline, place CPM.cmake at:\n  ${CPM_DOWNLOAD_LOCATION}")
    endif ()
endfunction()

if (NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
    download_cpm()
else ()
    # Resume / replace a previous empty or truncated download.
    file(SIZE "${CPM_DOWNLOAD_LOCATION}" _cpm_file_size)
    if (_cpm_file_size LESS 1000)
        message(STATUS "CPM.cmake at ${CPM_DOWNLOAD_LOCATION} looks incomplete (${_cpm_file_size} bytes); re-downloading")
        download_cpm()
    endif ()
    unset(_cpm_file_size)
endif ()

include("${CPM_DOWNLOAD_LOCATION}")

if (NOT COMMAND CPMAddPackage)
    message(FATAL_ERROR
        "CPM.cmake was included from:\n  ${CPM_DOWNLOAD_LOCATION}\n"
        "but CPMAddPackage is not defined. Delete that file and re-run configure, "
        "or re-download CPM ${CPM_DOWNLOAD_VERSION} manually.")
endif ()
