/* vim: set ts=8 sw=8 nu rnu sts=8 et: */
#ifndef SH_MAIN_H
#define SH_MAIN_H

#include "include/evilgeneral.h"

struct user_str {
        uid_t uid, euid, saveuid;
        gid_t gid, egid, savegid;
        char * nullable user;
        char * nullable passwd_shell;
        char * nullable home;
};

extern char * nullable evil_cwd;
extern int evil_gnu_error_format;
extern char * nullable * nullable environ;
extern struct user_str evil_user;

#endif /* SH_MAIN_H */
