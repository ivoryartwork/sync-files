struct send_file{
  char *source_file;
  char *target_file;
  int opt_type;// 0:modify,1:delete,2:add
  struct send_file *next_file;
};
struct target_filepath_list{
        char *filepath;
        struct target_filepath_list * next;
};

int syc_files(struct send_file *sendfiles);
int syc_remote_check();
