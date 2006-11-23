/*
 * This file has been modified for the cdrkit suite.
 *
 * The behaviour and appearence of the program code below can differ to a major
 * extent from the version distributed by the original author(s).
 *
 * For details, see Changelog file distributed with the cdrkit package. If you
 * received this file from another source then ask the distributing person for
 * a log of modifications.
 *
 */

extern char *jtemplate_out;
extern char *jjigdo_out;
extern char *jmd5_list;
extern FILE	*jthelper;
extern FILE *jtjigdo;
extern FILE *jttemplate;
extern int  jte_min_size;

extern void write_jt_header(FILE *template_file, FILE *jigdo_file);
extern void write_jt_footer(void);
extern void jtwrite(void *buffer, int size, int count, int submode, BOOL islast);
extern void write_jt_match_record(char *filename, char *mirror_name, int sector_size, off_t size, unsigned char md5[16]);
extern int  list_file_in_jigdo(char *filename, off_t size, char **realname, unsigned char md5[16]);
extern int  jte_add_exclude(char *pattern);
extern int  jte_add_include(char *pattern);
extern int  jte_add_mapping(char *arg);

#define MIN_JIGDO_FILE_SIZE 1024
