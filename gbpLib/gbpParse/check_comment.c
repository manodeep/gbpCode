#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gbpCommon.h>
#include <gbpParse.h>

int check_comment(char *line){
  char temp_char[2];
  int  j,flag=TRUE,rval=FALSE;
  for(j=0;j<strlen(line) && flag;j++) {
    sprintf(temp_char,"%c",line[j]);
    if(strcmp(temp_char," ")){
      if(!strcmp(temp_char,"#"))
        rval=TRUE;
      flag=FALSE;
    }
  }
  if(flag)
    rval=TRUE;
  return(rval);
}
