#ifndef _CMD_LINE_H_
#define _CMD_LINE_H_

void cmdline_routine();
int handle_cmd(char * cmd, int len);
extern pthread_t create_cmd_line_thread();

#endif _CMD_LINE_H_