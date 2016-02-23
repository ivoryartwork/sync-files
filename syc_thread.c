#include<stdio.h>
#include<dirent.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include "syc_scp.h"
char *project_path = "/home/ck/workspace/syc_project_file/projects/";
char *target_project_path="/data/apache-tomcat-7.0.62/webapps/";
struct send_file *send_file_head=NULL,*send_file_p=NULL;
int project_path_len;

int find_newfile(char* target_file){
	if(send_file_p==NULL){
		return -1;
	}
	FILE * projectMd5;
	struct target_filepath_list *target_filepath_list_head=NULL,*target_filepath_list_p=NULL;
	char buff[1024];
	char *p,*addr;
	const char *delim =" ";
	projectMd5 = fopen(target_file,"r");
	if(NULL==projectMd5){
		printf("target md5 file not exists");
		return -1;
	}
	memset(buff,0,sizeof(buff));
	while(NULL!=(fgets(buff,sizeof(buff),projectMd5))){
		p=strtok(buff,delim);
                p=p+strlen(p)+2;
		if(NULL!=(addr=strstr(p,"\n"))){
			*addr='\0'; 
		}
		struct target_filepath_list *tmp= malloc(sizeof(struct target_filepath_list));
		tmp->filepath = malloc(strlen(p)+1);
		strcpy(tmp->filepath,p);
                tmp->next=NULL;
		if(target_filepath_list_head==NULL){
			target_filepath_list_head = tmp;
		}else{
			target_filepath_list_p->next=tmp;
		}
		target_filepath_list_p = tmp;
                //printf("target:%s\n",tmp->filepath);
	}
	fclose(projectMd5);
	read_filelist("/home/ck/workspace/syc_project_file/projects/HMS/",target_filepath_list_head);
	return 0;

}

int read_filelist(char *basePath,struct target_filepath_list *targetfilelist)
{
	DIR *dir;
	struct dirent *ptr;
	char base[1000];

	if ((dir=opendir(basePath)) == NULL)
	{
		perror("Open dir error...");
		exit(1);
	}

	while ((ptr=readdir(dir)) != NULL)
	{
		if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
			continue;
		else if(ptr->d_type == 8||ptr->d_type==10){
			char *re_filepath = malloc(sizeof(ptr->d_name)+sizeof(basePath+project_path_len)+1);
			strcpy(re_filepath,basePath+project_path_len);
			strcat(re_filepath,ptr->d_name);
			check_newfile(re_filepath,targetfilelist);
		}    ///file
		else if(ptr->d_type == 4)    ///dir
		{
			memset(base,'\0',sizeof(base));
			strcpy(base,basePath);
			strcat(base,ptr->d_name);
			strcat(base,"/");
			read_filelist(base,targetfilelist);
		}
	}
	closedir(dir);
	return 1;
}

int check_newfile(char *localfile,struct target_filepath_list *targetfilelist){
	int new =1;
	while(targetfilelist!=NULL){
//                printf("localfile:%s,targetfile:%s\n",localfile,targetfilelist->filepath);
		if(!strcmp(localfile,targetfilelist->filepath)){
			new =0;
			break;
		}
		targetfilelist = targetfilelist->next;
	}
	if(new){
		//new file,add to send_file list
		//printf("new file:%s\n",localfile);
		struct send_file *new_file=(struct send_file *)malloc(sizeof(struct send_file));
		new_file->source_file = malloc(strlen(project_path)+strlen(localfile)+1);
		strcpy(new_file->source_file,project_path);
		strcat(new_file->source_file,localfile);
		new_file->target_file=malloc(strlen(target_project_path)+strlen(localfile)+1);
		strcpy(new_file->target_file,target_project_path);
		strcat(new_file->target_file,localfile);
		new_file->opt_type=2;
		send_file_p->next_file = new_file;
		send_file_p=new_file;
	}
}

int main(int argc,char *argv[]){
        printf("=====================================remote check start=================================\n");
        syc_remote_check();
        printf("=====================================remote check end===================================\n");
	project_path_len = strlen(project_path);
	FILE *fstream,*result; 
	char buff[1024];
	memset(buff,0,sizeof(buff));

	if(NULL==(fstream=popen("cd projects && md5sum -c md5 | grep FAILED > result","r"))){
		printf("failed\n");
		return -1;
	}
	pclose(fstream);
	result = fopen("./projects/result","r");
	memset(buff,0,sizeof(buff));
	char *addr;
	while(NULL!=(fgets(buff,sizeof(buff),result))){
		if(NULL!=(addr=strstr(buff,": FAILED"))){
			struct send_file *temp_file=(struct send_file *)malloc(sizeof(struct send_file));
			if(NULL!=strstr(buff,": FAILED open or read")){
				//delete file
				temp_file->opt_type = 1;
			}
			*addr='\0';
			temp_file->source_file = malloc(strlen(project_path)+strlen(buff)+1);
			strcpy(temp_file->source_file,project_path);
			strcat(temp_file->source_file,buff);
			temp_file->target_file=malloc(strlen(target_project_path)+strlen(buff)+1);
			strcpy(temp_file->target_file,target_project_path);
			strcat(temp_file->target_file,buff);
                        temp_file->next_file=NULL;
			if(send_file_head==NULL){
				send_file_head = temp_file;
			}else{
				send_file_p->next_file = temp_file;
			}      
			send_file_p = temp_file;
		}
	}
	find_newfile("./projects/md5");
	printf("%s\n",send_file_head->source_file);
	printf("%s\n",send_file_head->target_file);
	printf("%d\n",send_file_head->opt_type);
	syc_files(send_file_head);
}
