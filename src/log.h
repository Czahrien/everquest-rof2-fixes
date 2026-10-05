#pragma once

// Opens <directory>\rof2fixes.log, or rof2fixes.2.log, .3.log, ... when another game
// instance already has it open, so concurrent clients don't overwrite each other.
void LogOpen(const char* directory);
void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
