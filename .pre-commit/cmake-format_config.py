# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# pyright: reportUndefinedVariable=false
# cmake-format evaluates this module with `section` injected.

# Custom commands in this repo, ESP-IDF public CMake API (see IDF tools/cmake/{build,component}.cmake), and bundled
# third-party test helpers — cmake-format `additional_commands`.
with section("parse"):
    additional_commands = {
        "idf_build_component": {"pargs": 1},
        "idf_build_executable": {"pargs": 1},
        "idf_build_get_config": {
            "flags": ["GENERATOR_EXPRESSION"],
            "pargs": 2,
        },
        "idf_build_get_property": {
            "flags": ["GENERATOR_EXPRESSION"],
            "pargs": 2,
        },
        "idf_build_process": {
            "kwargs": {
                "BUILD_DIR": 1,
                "COMPONENTS": "+",
                "PROJECT_DIR": 1,
                "PROJECT_NAME": 1,
                "PROJECT_VER": 1,
                "SDKCONFIG": 1,
                "SDKCONFIG_DEFAULTS": "+",
            },
            "pargs": 1,
        },
        "idf_build_set_property": {
            "flags": ["APPEND"],
            "pargs": 2,
        },
        "idf_build_unset_property": {"pargs": 1},
        "idf_component_add_link_dependency": {
            "kwargs": {
                "FROM": 1,
                "TO": 1,
            },
        },
        "idf_component_get_property": {
            "flags": ["GENERATOR_EXPRESSION"],
            "pargs": 3,
        },
        "idf_component_mock": {
            "kwargs": {
                "INCLUDE_DIRS": "+",
                "MOCK_HEADER_FILES": "+",
                "REQUIRES": "+",
            },
        },
        "idf_component_optional_requires": {"pargs": "1+"},
        "idf_component_register": {
            "flags": ["WHOLE_ARCHIVE"],
            "kwargs": {
                "EMBED_FILES": "+",
                "EMBED_TXTFILES": "+",
                "EXCLUDE_SRCS": "+",
                "INCLUDE_DIRS": "+",
                "KCONFIG": 1,
                "KCONFIG_PROJBUILD": 1,
                "LDFRAGMENTS": "+",
                "PRIV_INCLUDE_DIRS": "+",
                "PRIV_REQUIRES": "+",
                "REQUIRED_IDF_TARGETS": "+",
                "REQUIRES": "+",
                "SRCS": "+",
                "SRC_DIRS": "+",
            },
        },
        "idf_component_set_property": {
            "flags": ["APPEND"],
            "pargs": 3,
        },
        "target_add_binary_data": {
            "kwargs": {
                "DEPENDS": "+",
                "RENAME_TO": 1,
            },
            "pargs": 3,
        },
    }

with section("format"):
    line_width = 120
    tab_size = 4
    use_tabchars = False
    separate_ctrl_name_with_space = True
    dangle_parens = True
    dangle_align = "prefix"
    line_ending = "unix"
