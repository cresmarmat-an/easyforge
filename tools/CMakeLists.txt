# easyforge-icon is defined whenever assets is built, even when another project
# fetches easyforge, because easyforge_app_icon runs it. It is only compiled when
# something needs it.
if(TARGET easyforge::assets)
    add_subdirectory(easyforge-icon)
endif()
