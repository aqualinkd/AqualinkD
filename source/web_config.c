
/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the 
 * Free Software Foundation. For the terms of this license, 
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/aqualinkd
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "config.h"
#include "color_lights.h"
#include "utils.h"
#include "aq_systemutils.h"

#define WEBCONFIGFILE "/config.json"
void fprintf_json(FILE *fp,const char *json_string);

// This should  be called dynamic web config, not webconfig to avoid confusion.
int build_dynamic_webconfig_js(struct aqualinkdata *aqdata, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int length = 0;

  length = build_color_lights_js(aqdata, buffer, size);

  length += sprintf(buffer+length, "var _enable_schedules = %s;\n",(_aqconfig_.enable_scheduler)?"true":"false");

  return length;
}



char* find_nth_char(const char* str, int character, int n) {
    char* p = (char*)str;
    int count = 0;

    while (count < n && (p = strchr(p, character)) != NULL) {
        count++;
        if (count == n) {
            return p;
        }
        p++; // Move pointer past the found character for the next search
    }
    return NULL; // Return NULL if the Nth instance is not found
}




int save_web_config_json(const char* inBuf, int inSize, char* outBuf, int outSize, struct aqualinkdata *aqdata)
{
  FILE *fp;
  char configfile[256];
  bool ro_root;
  bool created_file;
  char *contents;

  snprintf(configfile, 256, "%s%s", _aqconfig_.web_directory,WEBCONFIGFILE);


  fp = aq_open_file(configfile, &ro_root, &created_file);

  if (fp == NULL) {
    LOG(AQUA_LOG,LOG_ERR, "Open config file failed '%s'\n", configfile);
    //remount_root_ro(true);
    //fprintf(stdout, "Open file failed 'sprinkler.cron'\n");
    return false;
  }

  char* secondColonPos = find_nth_char(inBuf, ':', 2);
  // Check if the pointer is not NULL, and calculate the index inside the printf
  if (secondColonPos) {
    contents = malloc(sizeof(char) * inSize);
    int pos = (secondColonPos - inBuf) + 1;
  // Need to strip off {"uri":"webconfig/set","values": from beginning and the trailing }
    snprintf(contents, inSize - pos, "%s", inBuf+pos);

    // Below 2 will print string to file, but on one line
    // use fprintf_json to make look nicer
    //fprintf(fp, "%s", contents);
    //fprintf(fp,"\n");
    fprintf_json(fp,contents);

    free(contents);
  } else {
    // Error bad string of something.
    LOG(AQUA_LOG,LOG_ERR, "Bad web config '%s'\n", inBuf);
  }
  //fclose(fp);
  aq_close_file(fp, ro_root);


  return sprintf(outBuf, "{\"message\":\"Saved Web Config\"}");
}




void fprint_indentation(FILE *fp, int indent_level) {
    for (int i = 0; i < indent_level; i++) {
        fprintf(fp,"  "); // Use 2 spaces for indentation
    }
}

void fprintf_json(FILE *fp, const char *json_string) {
    int indent_level = 0;
    int in_string = 0; // Flag to track if inside a string literal

    for (int i = 0; i < strlen(json_string); i++) {
        char c = json_string[i];

        if (in_string) {
            fprintf(fp,"%c", c);
            if (c == '"' && json_string[i - 1] != '\\') { // End of string, handle escaped quotes
                in_string = 0;
            }
        } else {
            switch (c) {
                case '{':
                case '[':
                    fprintf(fp,"%c\n", c);
                    indent_level++;
                    fprint_indentation(fp,indent_level);
                    break;
                case '}':
                case ']':
                    fprintf(fp,"\n");
                    indent_level--;
                    fprint_indentation(fp,indent_level);
                    fprintf(fp,"%c", c);
                    break;
                case ',':
                    fprintf(fp,"%c\n", c);
                    fprint_indentation(fp,indent_level);
                    break;
                case ':':
                    fprintf(fp,"%c ", c);
                    break;
                case '"':
                    fprintf(fp,"%c", c);
                    in_string = 1;
                    break;
                default:
                    fprintf(fp,"%c", c);
                    break;
            }
        }
    }
    fprintf(fp,"\n");
}

