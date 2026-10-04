#pragma once

// Every easyforge library, and the bridges between them, for programs that
// link easyforge::easyforge and would rather not list headers one by one.
//
//     #include <easyforge/easyforge.h>
//
//     using namespace easyforge;
//
// A program that links only some of the libraries includes their headers
// instead, such as <easyforge/window.h> and <easyforge/ui.h>.

#include <easyforge/version.h>

#include <easyforge/assets.h>
#include <easyforge/core.h>
#include <easyforge/data.h>
#include <easyforge/graphics.h>
#include <easyforge/input.h>
#include <easyforge/network.h>
#include <easyforge/physics.h>
#include <easyforge/script.h>
#include <easyforge/sound.h>
#include <easyforge/ui.h>
#include <easyforge/window.h>

#include <easyforge/bridges/data_network.h>
#include <easyforge/bridges/data_script.h>
#include <easyforge/bridges/ui_script.h>
