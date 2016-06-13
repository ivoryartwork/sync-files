#include<stdio.h>
#include "strtool.h"

int main(){

  char *result = replace_p("this is a simple program!","simple","sample");
  printf("result:%s\n",result);
}
