/*
 * report.h — the machine-readable boot report.
 *
 * v0.4's job is to give the host-side AI Recovery Core a COMPLETE picture
 * of the machine in one parseable block: kernel version, GhostAttach scan
 * summary, every device's status, every driver's status, and the full
 * structured error log — bracketed by START/END markers so the analyzer
 * can find it even in a serial file containing several boots.
 *
 * The report is sent automatically at the end of boot, and again whenever
 * the user runs `export-logs`. It goes to the SERIAL port only: this data
 * is for machines; humans have the shell commands.
 */
#ifndef KMZ_REPORT_H
#define KMZ_REPORT_H

/* Write the full boot report to COM1. Safe to call any time after
   serial_init() + errors_init() + ghostattach_scan(). */
void report_send(void);

#endif /* KMZ_REPORT_H */
