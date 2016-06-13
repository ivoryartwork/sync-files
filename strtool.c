#include<stdio.h>
#include<stdlib.h>
#include"strtool.h"

char* replace_a(char str[],char *rep_src,char *rep_dest){
    if(str==NULL||rep_src==NULL||rep_dest==NULL){
       return NULL;
    }
    if(strlen(str)==0||strlen(rep_src)==0){
      return NULL;
    }
    char *p;
    while(NULL!=(p=strstr(str,rep_src))){
       p[0]='\0';
       p+=strlen(rep_src);
       char *temp = malloc(strlen(str)+strlen(rep_dest)+strlen(p)+1);
       strcpy(temp,str);
       strcat(temp,rep_dest);
       strcat(temp,p);
       str = temp;
    }
    return str;
}

char* replace_p(char *str,char *rep_src,char *rep_dest){
    if(str==NULL||rep_src==NULL||rep_dest==NULL){
       return NULL;
    }
    int len = strlen(str);
    if(len<=0||strlen(rep_src)==0){
         return NULL;
    }
    char str_tmp[len];
    strcpy(str_tmp,str);
    return replace_a(str_tmp,rep_src,rep_dest);
}
