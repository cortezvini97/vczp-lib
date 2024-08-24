#ifndef VCZP_H
#define VCZP_H

void pack_vczp(const char *path_vczp, void* files, void* commands, void* package_info, const void* base_dir, const int is_debug);
void debug(const char *path_vczp, const char *current_env);

#endif // VCZP_H