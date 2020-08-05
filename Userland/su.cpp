/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <AK/Vector.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/GetPassword.h>
#include <alloca.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern "C" int main(int, char**);

int main(int argc, char** argv)
{
    const char* user = nullptr;

    Core::ArgsParser args_parser;
    args_parser.add_positional_argument(user, "User to switch to (defaults to user with UID 0)", "user", Core::ArgsParser::Required::No);
    args_parser.parse(argc, argv);

    if (geteuid() != 0)
        fprintf(stderr, "Not running as root :(\n");

    uid_t uid = 0;
    gid_t gid = 0;
    struct passwd* pwd = nullptr;
    if (user) {
        pwd = getpwnam(user);
        if (!pwd) {
            fprintf(stderr, "No such user: %s\n", user);
            return 1;
        }
        uid = pwd->pw_uid;
        gid = pwd->pw_gid;
    }

    if (!pwd)
        pwd = getpwuid(0);

    if (!pwd) {
        fprintf(stderr, "No passwd entry.\n");
        return 1;
    }

    if (getuid() != 0 && pwd->pw_passwd[0] != '\0') {
        auto password = Core::get_password();
        if (password.is_error()) {
            fprintf(stderr, strerror(password.error()));
            return 1;
        }

        char* hash = crypt(password.value().characters(), pwd->pw_passwd);
        if (hash == NULL || strcmp(hash, pwd->pw_passwd) != 0) {
            fprintf(stderr, "Incorrect or disabled password.\n");
            return 1;
        }
    }

    Vector<gid_t> extra_gids;
    for (auto* group = getgrent(); group; group = getgrent()) {
        for (size_t i = 0; group->gr_mem[i]; ++i) {
            if (!strcmp(pwd->pw_name, group->gr_mem[i]))
                extra_gids.append(group->gr_gid);
        }
    }
    endgrent();

    int rc = setgroups(extra_gids.size(), extra_gids.data());
    if (rc < 0) {
        perror("setgroups");
        return 1;
    }
    rc = setgid(gid);
    if (rc < 0) {
        perror("setgid");
        return 1;
    }
    rc = setuid(uid);
    if (rc < 0) {
        perror("setuid");
        return 1;
    }
    rc = execl(pwd->pw_shell, pwd->pw_shell, nullptr);
    perror("execl");
    return 1;
}
