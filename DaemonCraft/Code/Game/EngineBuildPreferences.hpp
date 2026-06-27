//-----------------------------------------------------------------------------------------------
// EngineBuildPreferences.hpp
//
// Defines build preferences that the Engine should use when building for this particular game.
//
// Note that this file is an exception to the rule "engine code shall not know about game code".
//	Purpose: Each game can now direct the engine via #defines to build differently for that game.
//	Downside: ALL games must now have this Code/Game/EngineBuildPreferences.hpp file.
//

#pragma once
//
// Engine Build Preferences for SimpleMiner
//
// These defines control which Engine subsystems are compiled and linked.
// Disabling unused subsystems reduces binary size and removes runtime DLL dependencies.
//

// #define ENGINE_DISABLE_AUDIO	   // Disables AudioSystem code and removes FMOD linkage
// #define ENGINE_DISABLE_SCRIPT	   // Disables ScriptSubsystem code (NOTE: V8 linking controlled in Engine.vcxproj)

// Enable Debug Rendering System
#define ENGINE_DEBUG_RENDER
#define CONSOLE_HANDLER
// #define ENGINE_SCRIPTING_ENABLED
//
// V8 JavaScript Engine Configuration:
// V8 linking is controlled by <EnableV8ScriptEngine> in Engine/Code/Engine/Engine.vcxproj (line 87)
// - Set to 'false': No V8 linking, SimpleMiner.exe runs without V8 DLLs (current setting)
// - Set to 'true':  V8 links, requires v8.dll/v8_libbase.dll/v8_libplatform.dll at runtime
//
