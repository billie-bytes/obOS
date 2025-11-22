#ifndef HEADER_COMMAND_H
#define HEADER_COMMAND_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_COMMAND_LENGTH 256
#define MAX_ARGS           16
#define MAX_PATH_LENGTH    256
#define HISTORY_COUNT      16

extern uint32_t current_directory_inode;
extern char current_path[MAX_PATH_LENGTH];

void print(const char* str, uint8_t color);
void print_char(char ch, uint8_t color);
char read_char(void);
void get_cursor_pos(uint8_t* x, uint8_t* y);
void set_cursor_pos(uint8_t x, uint8_t y);
void clear_screen(void);

void add_to_history(const char* command);
int  read_line(char* out, int out_max);
void parse_command(char* line, char* argv[], int* argc_out);
void execute_command(int argc, char* argv[]);
void print_prompt(void);

void cmd_ls   (char* args[], int arg_count);
void cmd_cd   (char* args[], int arg_count);
void cmd_pwd  (char* args[], int arg_count);
void cmd_mkdir(char* args[], int arg_count);
void cmd_cat  (char* args[], int arg_count);
void cmd_cp   (char* args[], int arg_count);
void cmd_rm   (char* args[], int arg_count);
void cmd_mv   (char* args[], int arg_count);
void cmd_find (char* args[], int arg_count);
void cmd_help (char* args[], int arg_count);
void cmd_clear(char* args[], int arg_count);
void cmd_kill (char* args[], int arg_count);
void cmd_exec (char* args[], int arg_count);

void ps(void);

#endif
