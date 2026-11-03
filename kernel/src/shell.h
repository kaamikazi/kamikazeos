/*
 * shell.h — the KamikazeOS command shell.
 *
 * In Phase 1 the shell runs INSIDE the kernel (there's no userspace yet):
 * read a line from the keyboard, match it against a command table, run the
 * handler. Real OSes run shells as unprivileged processes — that separation
 * v2.6 can execute one fixed linked CPL3 test routine; it is not a process model.
 */
#ifndef KMZ_SHELL_H
#define KMZ_SHELL_H

void shell_run(void);   /* never returns */

#endif /* KMZ_SHELL_H */
