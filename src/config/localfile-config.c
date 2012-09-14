/* @(#) $Id: ./src/config/localfile-config.c, 2012/03/28 dcid Exp $
 */

/* Copyright (C) 2009 Trend Micro Inc.
 * All right reserved.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */

 

#include "shared.h" 
#include "localfile-config.h"


int Read_Localfile(XML_NODE node, void *d1, void *d2)
{
    int pl = 0;
    int i = 0;
    
    int glob_set = 0; 
    
    #ifndef WIN32
    int glob_offset = 0;
    #endif


    /* XML Definitions */
    char *xml_localfile_location = "location";
    char *xml_localfile_command = "command";
    char *xml_localfile_logformat = "log_format";
    char *xml_localfile_frequency = "frequency";
    char *xml_localfile_alias = "alias";

    logreader *logf;
    logreader_config *log_config;

    log_config = (logreader_config *)d1;

    /* If config is not set, we need to create it */ 
    if(!log_config->config)
    {
        os_calloc(2, sizeof(logreader), log_config->config);
        logf = log_config->config;
        logf[0].file = logf[0].ffile = logf[0].alias = NULL;
        logf[1].file = logf[1].ffile = logf[1].alias = NULL;
    }
    else
    {
        logf = log_config->config;
        while(logf[pl].file != NULL)
            pl++;

        /* Allocating more memory */
        os_realloc(logf, (pl +2)*sizeof(logreader), log_config->config);
        logf = log_config->config;
        logf[pl+1].file = logf[pl+1].ffile = logf[pl+1].alias = NULL;
    }
    
    logf[pl].file = logf[pl].ffile = logf[pl].alias = NULL;
    logf[pl].ign = 360;
    
    /* Searching for entries related to files */
    i = 0;
    while(node[i])
    {
        if(!node[i]->element)
        {
            merror(XML_ELEMNULL, ARGV0);
            return(OS_INVALID);
        }
        else if(!node[i]->content)
        {
            merror(XML_VALUENULL, ARGV0, node[i]->element);
            return(OS_INVALID);
        }
        else if(strcmp(node[i]->element,xml_localfile_command) == 0)
        {
            /* We don't accept remote commands from the manager - just in case. */
            if(log_config->agent_cfg == 1 && log_config->accept_remote == 0)
            {
                merror("%s: Remote commands are not accepted from the manager. "
                       "Ignoring it on the agent.conf", ARGV0);

                logf[pl].file = logf[pl].ffile = logf[pl].alias = NULL;
                return(OS_INVALID);
            }

            os_strdup(node[i]->content, logf[pl].file);
            logf[pl].command = logf[pl].file;
        }
        else if(strcmp(node[i]->element,xml_localfile_frequency) == 0)
        {
            if(!OS_StrIsNum(node[i]->content))
            {
                merror(XML_VALUEERR,ARGV0,node[i]->element,node[i]->content);
                return(OS_INVALID);
            }

            logf[pl].ign = atoi(node[i]->content);
        }
        else if(strcmp(node[i]->element,xml_localfile_location) == 0)
        {
            #ifdef WIN32
            /* Expand variables on Windows. */
            if(strchr(node[i]->content, '%'))
            {
                int expandreturn = 0;   
                char newfile[OS_MAXSTR +1];

                newfile[OS_MAXSTR] = '\0';
                expandreturn = ExpandEnvironmentStrings(node[i]->content, 
                                                        newfile, OS_MAXSTR);

                if((expandreturn > 0) && (expandreturn < OS_MAXSTR))
                {
                    free(node[i]->content);

                    os_strdup(newfile, node[i]->content);
                }
            }   
            #endif


            /* This is a glob*.
             * We will call this file multiple times until
             * there is no one else available.
             */
            #ifndef WIN32 /* No windows support for glob */ 
            if(strchr(node[i]->content, '*') ||
               strchr(node[i]->content, '?') ||
               strchr(node[i]->content, '['))
            {
                glob_t g;
                
                /* Setting ot the first entry of the glob */
                if(glob_set == 0)
                    glob_set = pl +1;
                
                if(glob(node[i]->content, 0, NULL, &g) != 0)
                {
                    merror(GLOB_ERROR, ARGV0, node[i]->content);
                    os_strdup(node[i]->content, logf[pl].file);
                    i++;
                    continue;
                }
             
                /* Checking for the last entry */
                if((g.gl_pathv[glob_offset]) == NULL)
                {
                    /* Checking when nothing is found. */
                    if(glob_offset == 0)
                    {
                        merror(GLOB_NFOUND, ARGV0, node[i]->content);
                        return(OS_INVALID);
                    }
                    i++;
                    continue;
                }


                /* Checking for strftime on globs too. */
                if(strchr(g.gl_pathv[glob_offset], '%'))
                {
                    struct tm *p;
                    time_t l_time = time(0);
                    char lfile[OS_FLSIZE + 1];
                    size_t ret;

                    p = localtime(&l_time);

                    lfile[OS_FLSIZE] = '\0';
                    ret = strftime(lfile, OS_FLSIZE, g.gl_pathv[glob_offset], p);
                    if(ret == 0)
                    {
                        merror(PARSE_ERROR, ARGV0, g.gl_pathv[glob_offset]);
                        return(OS_INVALID);
                    }

                    os_strdup(g.gl_pathv[glob_offset], logf[pl].ffile);
                    os_strdup(g.gl_pathv[glob_offset], logf[pl].file);
                }
                else
                {
                    os_strdup(g.gl_pathv[glob_offset], logf[pl].file);
                }

                
                glob_offset++;
                globfree(&g);

                /* Now we need to create another file entry */
                pl++;
                os_realloc(logf, (pl +2)*sizeof(logreader), log_config->config);
                logf = log_config->config;
                
                logf[pl].file = logf[pl].ffile = logf[pl].alias = NULL;
                logf[pl+1].file = logf[pl+1].ffile = logf[pl+1].alias = NULL;

                /* We can not increment the file count in here */
                continue;
            }
            else if(strchr(node[i]->content, '%'))
            #else
            if(strchr(node[i]->content, '%'))    
            #endif /* WIN32 */

            /* We need the format file (based on date) */
            {
                struct tm *p;
                time_t l_time = time(0);
                char lfile[OS_FLSIZE + 1];
                size_t ret;

                p = localtime(&l_time);

                lfile[OS_FLSIZE] = '\0';
                ret = strftime(lfile, OS_FLSIZE, node[i]->content, p);
                if(ret == 0)
                {
                    merror(PARSE_ERROR, ARGV0, node[i]->content);
                    return(OS_INVALID);
                }

                os_strdup(node[i]->content, logf[pl].ffile);
                os_strdup(node[i]->content, logf[pl].file);
            }
            
            
            /* Normal file */
            else
            {
                os_strdup(node[i]->content, logf[pl].file);
            }
        }

        /* Getting log format */
        else if(strcasecmp(node[i]->element,xml_localfile_logformat) == 0)
        {
            os_strdup(node[i]->content, logf[pl].logformat);

            if(strcmp(logf[pl].logformat, "syslog") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "generic") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "snort-full") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "snort-fast") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "apache") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "iis") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "squid") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "nmapg") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "mysql_log") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "ossecalert") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "mssql_log") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "postgresql_log") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "djb-multilog") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "syslog-pipe") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "command") == 0)
            {
            }
            else if(strcmp(logf[pl].logformat, "full_command") == 0)
            {
            }
            else if(strncmp(logf[pl].logformat, "multi-line", 10) == 0)
            {
                int x = 0;
                char *p = logf[pl].logformat+10;

                while(*p == ' ')
                    p++;
                
                if(*p != ':')
                {
                    merror(XML_VALUEERR,ARGV0,node[i]->element,node[i]->content);
                    return(OS_INVALID);
                }
                p++;

                while(*p == ' ')
                    p++;
                
                while(p[x] >= '0' && p[x] <= '9')    
                    x++;

                while(p[x] == ' ')
                    x++;

                if(p[x] != '\0')
                {
                    merror(XML_VALUEERR,ARGV0,node[i]->element,node[i]->content);
                    return(OS_INVALID);
                }

                logf[pl].lines = atoi(p);

                if (logf[pl].lines == 0)
                {
                    merror(XML_VALUEERR,ARGV0,node[i]->element,node[i]->content);
                    return(OS_INVALID);
                }
            }
            else if(strncmp(logf[pl].logformat, "modsec_audit", 10) == 0)
            {
            }
            else if(strncmp(logf[pl].logformat, "regex", 10) == 0)
            {
                if (node[i]->attributes == NULL || node[i]->values == NULL) {
                    merror(XML_INVATTR,ARGV0,node[i]->element,node[i]->content);
                    return(OS_INVALID);
                }

                int a = 0;

                while (node[i]->attributes[a] && node[i]->values[a]) {
                    if (strncmp(node[i]->attributes[a], "start_regex", 11) == 0) {
                        if (logf[pl].start_regex != NULL)
                            os_free(logf[pl].start_regex);

                        os_strdup(node[i]->values[a],logf[pl].start_regex);  //no strndup in Win32 
                        //logf[pl].start_regex = strndup(node[i]->values[a], OS_PATTERN_MAXSIZE);
                        // @todo Maybe try to immediatelly compile regex so that user knows if it is OK?
                    }

                    if (strncmp(node[i]->attributes[a], "end_regex", 9) == 0) {
                        if (logf[pl].end_regex != NULL)
                            os_free(logf[pl].end_regex);

                        os_strdup(node[i]->values[a],logf[pl].end_regex);  //no strndup in win32
                        //logf[pl].start_regex = strndup(node[i]->values[a], OS_PATTERN_MAXSIZE);
                        // @todo Maybe try to immediatelly compile regex so that user knows if it is OK?
                    }

                    a++;
                }

            }
            else if(strncmp(logf[pl].logformat, "linux_auditd", 12) == 0)
            {
                if (node[i]->attributes == NULL || node[i]->values == NULL) {
                    merror(XML_INVATTR,ARGV0,node[i]->element,node[i]->content);
                    return(OS_INVALID);
                }

                int a = 0;

                while (node[i]->attributes[a] && node[i]->values[a]) {

                    if (strncmp(node[i]->attributes[a], "window", 6) == 0)
                        logf[pl].window = atoi(node[i]->values[a]);
                    else if (strncmp(node[i]->attributes[a], "timeout", 7) == 0)
                        logf[pl].timeout = atoi(node[i]->values[a]);

                    a++;
                }

                if ((logf[pl].window == 0 && logf[pl].timeout == 0) ||
                        (logf[pl].window != 0 && logf[pl].timeout != 0)) {
                    merror(XML_INVATTR,ARGV0,node[i]->element,node[i]->content);
                    return(OS_INVALID);
                }
            }
            else if(strcmp(logf[pl].logformat, EVENTLOG) == 0)
            {
            }
            else
            {
                merror(XML_VALUEERR,ARGV0,node[i]->element,node[i]->content);
                return(OS_INVALID);
            }
        }
        else if(strcasecmp(node[i]->element,xml_localfile_alias) == 0)
        {
            os_strdup(node[i]->content, logf[pl].alias);
        }
        else
        {
            merror(XML_INVELEM, ARGV0, node[i]->element);
            return(OS_INVALID);
        }

        i++;
    }


    /* Validating glob entries */
    if(glob_set)
    {
        char *format;
        
        /* Getting log format */
        if(logf[pl].logformat)
        {
            format = logf[pl].logformat;
        }
        else if(logf[glob_set -1].logformat)
        {
            format = logf[glob_set -1].logformat;
        }
        else
        {
            merror(MISS_LOG_FORMAT, ARGV0);
            return(OS_INVALID);
        }

        /* The last entry is always null on glob */
        pl--;


        /* Setting format for all entries */
        for(i = (glob_set -1); i<= pl; i++)
        {
            /* Every entry must be valid */
            if(!logf[i].file)
            {
                merror(MISS_FILE, ARGV0);
                return(OS_INVALID);
            }
            
            if(logf[i].logformat == NULL)
            {
                logf[i].logformat = format;
            }

        }
    }

    /* Missing log format */
    if(!logf[pl].logformat)
    {
        merror(MISS_LOG_FORMAT, ARGV0);
        return(OS_INVALID);
    }

    /* Missing file */
    if(!logf[pl].file)
    {
        merror(MISS_FILE, ARGV0);
        return(OS_INVALID);
    }
    
    /* Verifying a valid event log config */
    if(strcmp(logf[pl].logformat, EVENTLOG) == 0)
    {
        if((strcmp(logf[pl].file, "Application") != 0)&&
           (strcmp(logf[pl].file, "System") != 0)&&
           (strcmp(logf[pl].file, "Security") != 0))
         {
             /* Invalid event log */
             merror(NSTD_EVTLOG, ARGV0, logf[pl].file);
             return(0);
         }
    }

    if((strcmp(logf[pl].logformat, "command") == 0)||
       (strcmp(logf[pl].logformat, "full_command") == 0)) 
    {
        if(!logf[pl].command)
        {
            merror("%s: ERROR: Missing 'command' argument. "
                   "This option will be ignored.", ARGV0);
        }
    }

    return(0);
}

/* EOF */
