
#[==[
Generate two library targets with the given basename for both STATIC and SHARED
link types.

    mongo_add_multitype_library(
        <basename>
        [OUTPUT_NAME <name>]
        [COLLECT_SOURCES [<glob>|!<glob>] ...]
        [REQUIREMENTS ...]
        [STATIC_REQUIREMENTS ...]
        [SHARED_REQUIREMENTS ...]
        [INSTALL_HEADERS ...]
        [HEADER_INSTALL_DESTINATION <relpath>]
        [TARGET_INSTALL_DESTINATION <relpath>]
    )

The generated libraries will have names `${basename}_static` and
`${basename}_shared`, but will also be available as `mongo::${basename}_static`
and `mongo::${basename}_shared`, which should be preferred when linking them
into downstream targets.

The following points should be understood about the generated library targets:

- The compilation of the library sources will include the `src/` subdirectories
  of the CMake source directory and binary directory at the time of the call.
- The libraries will PUBLIC-use mongo::detail::c_platform.
- The libraries will have a build-tree-only PUBLIC usage of
  mcd::build_interface.
- The VERSION on the libraries will inhert the PROJECT_VERSION value.
- The SOVERSION on the SHARED library will use the value of
  PROJECT_VERSION_MAJOR.

The following parameters are defined:

OUTPUT_NAME <name>
    - Set the OUTPUT_NAME property on the generated targets. If unspecified,
      defaults to <basename>.

COLLECT_SOURCES (<pattern> | !<pattern>) ...
    - Specfiy globbing patterns to collect the source files for the targets.
      Globs are evaluated using GLOB_RECURSE. Globs are evaluated in the order
      they are listed, which is relevant if any pattern begins with "!". When a
      pattern begins with "!", then the matching source files will be removed
      from the list.

      Source files can be added later by using `target_sources(INTERFACE)` on
      the `${basename}-sources` INTERFACE target.

REQUIREMENTS [...]
STATIC_REQUIREMENTS [...]
SHARED_REQUIREMENTS [...]
    - Specify target properties and usage requirements for the generated
      libraries. See `mongo_target_requirements()` for more information.

      IMPORTANT: Only the REQUIREMENTS argument can be used to affect object
      file compilation! The STATIC and SHARED libraries are generated from
      object files that are compiled separately, so they will not have any
      effect on the compilation of their input objects.

INSTALL_HEADERS [...]
    - Install the contents of the `src/` subdirectories from both the source and
      binary subdirectories. The values of this argument are additional
      arguments to be passed to the `install(DIRECTORY)` command, as if by:

        install(DIRECTORY ... DESTINATION ... ${INSTALL_HEADERS})

      The DESTINATION comes from the HEADER_INSTALL_DESTINATION argument. The
      most important value for this argument would be using FILES_MATCHING to
      filter to include *only* header files that the caller wishes to install.

HEADER_INSTALL_DESTINATION <relpath>
    - Specify a relative path (from the install prefix) where header files
      should be installed using INSTALL_HEADERS. If unspecified, defaults to
      CMAKE_INSTALL_INCLUDEDIR. This will also be used as the INCLUDES value
      when targets are installed.

TARGET_INSTALL_DESTINATION <relpath>
    - Specify a relative path from the install prefix where CMake target EXPORT
      files should be placed. If unspecified, uses
      ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}.


Target Installation
===================

Two additional configure-time build settings will be defined:
`ENABLE_${BASENAME}_STATIC_INSTALL` and `ENABLE_${BASENAME}_SHARED_INSTALL`,
which will enable/disable the installation rules for the static and the shared
library targets, respectively.

The static and the shared libraries are installed into different export sets and
write different export-target files, named `${basename}_static-targets.cmake`
and `${basename}_shared-targets.cmake`, respectively. The config-file package
for these targets is not generated here, but should be written to `include()`
these export-target files.

]==]
function(mongo_add_multitype_library basename)
    list(APPEND CMAKE_MESSAGE_CONTEXT "${CMAKE_CURRENT_FUNCTION}(${basename})")
    set(options)
    set(args
        OUTPUT_NAME
        HEADER_INSTALL_DESTINATION
        TARGET_INSTALL_DESTINATION
        )
    set(list_args
        COLLECT_SOURCES
        REQUIREMENTS
        STATIC_REQUIREMENTS
        SHARED_REQUIREMENTS
        INSTALL_HEADERS
        )
    cmake_parse_arguments(PARSE_ARGV 1 lib "${options}" "${args}" "${list_args}")
    foreach(arg IN LISTS lib_UNPARSED_ARGUMENTS)
        message(SEND_ERROR "Unrecognized argument “${arg}”")
    endforeach()
    if(lib_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unrecognized arguments given to mongo_add_multitype_library")
    endif()
    # Default values
    if(NOT lib_OUTPUT_NAME)
        set(lib_OUTPUT_NAME "${basename}")
    endif()

    string(MAKE_C_IDENTIFIER "${basename}" ident)
    string(TOUPPER "${ident}" IDENT)
    mongo_bool_setting(
        ENABLE_${IDENT}_SHARED_INSTALL "Install the ${basename} shared library"
        VISIBLE_IF ENABLE_SHARED
    )
    mongo_bool_setting(
        ENABLE_${IDENT}_STATIC_INSTALL "Install the ${basename} static library"
        VISIBLE_IF ENABLE_STATIC
    )

    # Define a set of usage requirements for compiling and using this library
    # within the build tree. This target must not be installed or used in any
    # installed usage requirements.
    set(bld_iface_lib _priv_${basename}_build_interface)
    add_library(${bld_iface_lib} INTERFACE)
    add_library(${basename}::build_interface ALIAS ${bld_iface_lib})
    mongo_target_requirements(${bld_iface_lib}
        INCLUDE_DIRECTORIES INTERFACE
            # Include the "src/" subdirectory
            src/
            # Include any generated "src/" directory in the binary dir
            ${CMAKE_CURRENT_BINARY_DIR}/src/
        LINK_LIBRARIES INTERFACE
            # Platform usage requirements:
            mongo::detail::c_platform
            # Common build interface usage requirements:
            mcd::build_interface
        )

    # Collect source files using glob patterns.
    set(sources)
    # We want to add the common sources to the library:
    set(common_pattern
        "${mongo-c-driver_SOURCE_DIR}/src/common/*.c"
        "${mongo-c-driver_SOURCE_DIR}/src/common/*.h"
        )
    foreach(pat IN LISTS lib_COLLECT_SOURCES common_pattern)
        # Switch on if it is a negated pattern:
        set(negate FALSE)
        set(glob "${pat}")
        if(pat MATCHES "^!(.*)")
            set(glob "${CMAKE_MATCH_1}")
            set(negate TRUE)
        endif()
        # Collect them:
        file(GLOB_RECURSE found CONFIGURE_DEPENDS "${glob}")
        # Update the source list:
        if(negate)
            list(REMOVE_ITEM sources ${found} ~~~) # "~~~" prevents calling with an empty argument list
        else()
            list(APPEND sources ${found})
        endif()
        # Debug info:
        list(LENGTH found n_found)
        message(DEBUG "Source pattern “${pat}” matched ${n_found} files")
    endforeach()
    list(REMOVE_DUPLICATES sources)
    list(LENGTH sources n_src)
    message(DEBUG "Found ${n_src} source files")

    # Add a library that exposes the SOURCES that we collect. Object libraries link to
    # this so that they both the same source files, and the caller can update the source list
    # by updating this single target's SOURCES.
    add_library(${basename}-sources INTERFACE)
    add_library(${basename}::sources ALIAS ${basename}-sources)
    # It is okay if 'sources' is empty, since the caller may add sources later.
    target_sources(${basename}-sources INTERFACE ${sources})

    # Define the base object files library
    add_library(_${basename}-objects OBJECT EXCLUDE_FROM_ALL)
    add_library(${basename}::objects ALIAS _${basename}-objects)
    target_link_libraries(_${basename}-objects PRIVATE ${basename}-sources)
    set(object_libs _${basename}-objects)

    # Decide how to handle PIC-enabled sources
    if(ENABLE_PIC OR WIN32)
        # User wants (or platform requires) static libs to use PIC code. Since we
        # already need PIC for the dynamic library, we can consolidate things and
        # use a single object library for both the static and the shared library.
        # No duplicate compilations necessary!
        set_property(TARGET _${basename}-objects PROPERTY POSITION_INDEPENDENT_CODE TRUE)
        # The ${basename}::objects-pic is just an alias of the base library:
        add_library(${basename}::objects-pic ALIAS _${basename}-objects)
     else()
        # User does not want PIC in the static library. In that case, we just need a second object
        # library that has PIC enabled so it can be used in creating the dynamic library.
        add_library(_${basename}-objects-pic OBJECT EXCLUDE_FROM_ALL)
        set_property(TARGET _${basename}-objects-pic PROPERTY POSITION_INDEPENDENT_CODE TRUE)
        add_library(${basename}::objects-pic ALIAS _${basename}-objects-pic)
        list(APPEND object_libs _${basename}-objects-pic)
     endif()

    # Define usage requirements for the object libraries.
    mongo_target_requirements(
        ${object_libs}
        LINK_LIBRARIES
            # This usage requirement will cause the SOURCES to populate:
            PRIVATE ${basename}::sources
            # Link with the common build_interface properties
            PUBLIC ${basename}::build_interface
        COMPILE_DEFINITIONS
            # Set the prefix for the common library components, to prevent symbol
            # collision with other compilations of the common libs:
            PRIVATE
                MCOMMON_NAME_PREFIX=$<MAKE_C_IDENTIFIER:_${basename}_mcommon>
    )
    # Apply any usage requirements that were given to us from the caller:
    mongo_target_requirements(${object_libs} ${lib_REQUIREMENTS})

    # "libs" is just the list of libraries we are building
    set(libs)
    if(ENABLE_STATIC)
        # Static library was enabled
        add_library(${basename}_static STATIC)
        add_library(mongo::${basename}_static ALIAS ${basename}_static)
        # Link the object library to inject the objects into the generated archive:
        target_link_libraries(${basename}_static PRIVATE $<BUILD_INTERFACE:${basename}::objects>)
        # Apply usage requirements from the caller:
        mongo_target_requirements(${basename}_static ${lib_STATIC_REQUIREMENTS})
        list(APPEND libs "${basename}_static")
    endif()

    if(ENABLE_SHARED)
        # Shared library was enabled
        add_library(${basename}_shared SHARED)
        add_library(mongo::${basename}_shared ALIAS ${basename}_shared)
        # Link to the object library that has PIC enabled. This *may* be the same
        # object library used for the static library, depending on whether PIC is
        # enabled for static components. This sharing reduces redundant compilation
        # when both libraries have the same codegen
        target_link_libraries(${basename}_shared PRIVATE $<BUILD_INTERFACE:${basename}::objects-pic>)
        # Apply usage requirements from the caller:
        mongo_target_requirements(${basename}_shared ${lib_SHARED_REQUIREMENTS})
        list(APPEND libs "${basename}_shared")
    endif()

    # A utility target that just depends on both types:
    add_custom_target(${basename}-all DEPENDS ${libs})

    if(NOT ENABLE_SHARED AND NOT ENABLE_STATIC)
        # ??? We have nothing to build, so there's nothing left to do, I guess.
        return()
    endif()

    # Set usage requirements/properties that are common to both libraries:
    mongo_target_requirements(
        ${libs} LINK_LIBRARIES PUBLIC
        # Build-tree requirements for these libraries:
        $<BUILD_INTERFACE:${basename}::build_interface>
        # Include in the install interface explicitly:
        mongo::detail::c_platform
    )
    set_target_properties(${libs} PROPERTIES
        VERSION "0.0.0" # Currently, we hardcode these to 0.0.0
        SOVERSION "0"   # And SONAME is always 0
        OUTPUT_NAME "${lib_OUTPUT_NAME}"
    )

    # Apply framework properties for the dynamic library:
    if(ENABLE_APPLE_FRAMEWORK AND TARGET ${basename}_shared)
        set_target_properties(${basename}_shared PROPERTIES
            FRAMEWORK TRUE
            MACOSX_FRAMEWORK_BUNDLE_VERSION "${mongo-c-driver_VERSION_FULL}"
            MACOSX_FRAMEWORK_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_FRAMEWORK_IDENTIFIER org.mongodb.bson
        )
    endif()

    # The subdirectory of the prefix where we will install the library's header files:
    if(NOT lib_HEADER_INSTALL_DESTINATION)
        set(lib_HEADER_INSTALL_DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    endif()

    if(NOT lib_TARGET_INSTALL_DESTINATION)
        set(lib_TARGET_INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}")
    endif()

    # Pick what the user has asked us to install:
    set(targets_to_install)
    if(ENABLE_${IDENT}_SHARED_INSTALL AND TARGET ${basename}_shared)
        list(APPEND targets_to_install ${basename}_shared)
    endif()
    if(ENABLE_${IDENT}_STATIC_INSTALL AND TARGET ${basename}_static)
        list(APPEND targets_to_install ${basename}_static)
    endif()

    foreach(target IN LISTS targets_to_install)
        install(
            TARGETS "${target}"
            # Important: We generate a unique export set for each target type.
            EXPORT "${target}-targets"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            INCLUDES DESTINATION "${lib_HEADER_INSTALL_DESTINATION}"
            FRAMEWORK DESTINATION "${CMAKE_INSTALL_BINDIR}"
        )
        # Install the unique export set into a file that is qualified by the name of
        # the target itself. The main config-file package will search for the
        # possibly-installed exported targets for the known targets. See: bson-config.cmake
        install(
            EXPORT "${target}-targets"
            NAMESPACE mongo::
            FILE "${target}-targets.cmake"
            DESTINATION "${lib_TARGET_INSTALL_DESTINATION}"
        )
        # Notify pkg-config generation of the include directory:
        set_property(
            TARGET "${target}"
            APPEND PROPERTY pkg_config_INCLUDE_DIRECTORIES "${lib_HEADER_INSTALL_DESTINATION}"
        )
    endforeach()

    # Install the headers if the caller has asked us to
    if(lib_INSTALL_HEADERS)
        # We do a directory-based install, to preserve the path to each header
        # file relative to the src/ directory, which is used as the only in-tree
        # include search path. This ensures that all files both inside and outside of
        # the source tree use the proper #include directives.
        install(
            DIRECTORY
                # Trailing "/" requests directory contents, not the dir itself:
                src/
                # Also get the generated dir:
                ${CMAKE_CURRENT_BINARY_DIR}/src/
            DESTINATION "${lib_HEADER_INSTALL_DESTINATION}"
            # Paste the caller's arguments here:
            ${lib_INSTALL_HEADERS}
        )
    endif()
endfunction()
