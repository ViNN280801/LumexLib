// Stands for the real header of that path: it refuses to compile until a
// configuration macro is defined, so the checker has to define it.
#ifndef LUMEX_LOGGER_CONFIG_FORMAT_PLAIN_TEXT
#error "needs LUMEX_LOGGER_CONFIG_FORMAT_PLAIN_TEXT"
#endif
