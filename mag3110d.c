/**************************************************************************
 * 
 * Read magnetic field from MAG3110 chip with I2C and write it to a log file. 
 *       
 * Copyright (C) 2014 Jaakko Koivuniemi.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************************
 *
 * Mon Nov  3 21:20:26 CET 2014
 * Edit: Sun Nov  9 19:14:57 CET 2014
 *
 * Jaakko Koivuniemi
 **/

#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

const int version=20141109; // program version
int tempint=0; // temperature reading interval
int magnint=300; // field reading interval [s]

const char mdatafilex[200]="/var/lib/mag3110d/Bx";
const char mdatafiley[200]="/var/lib/mag3110d/By";
const char mdatafilez[200]="/var/lib/mag3110d/Bz";

const char tdatafile[200]="/var/lib/mag3110d/temperature";

const char i2cdev[100]="/dev/i2c-1";
int  address=0x0e;
const int  i2lockmax=10; // maximum number of times to try lock i2c port  

const char confile[200]="/etc/mag3110d_config";

const char pidfile[200]="/var/run/mag3110d.pid";

int loglev=3;
const char logfile[200]="/var/log/mag3110d.log";
char message[200]="";

short Bx=0,By=0,Bz=0; // magnetic field [0.1 uT]
short xoffset=0,yoffset=0,zoffset=0; // magnetic field offset [0.1 uT]
signed char toffset=0; // temperature offset [C]
unsigned char ctrlreg1=0; // CTRL_REG1
unsigned char ctrlreg2=0; // CTRL_REG2
int tdelay=15; // delay to wait after trigger before reading [s]

void logmessage(const char logfile[200], const char message[200], int loglev, int msglev)
{
  time_t now;
  char tstr[25];
  struct tm* tm_info;
  FILE *log;

  time(&now);
  tm_info=localtime(&now);
  strftime(tstr,25,"%Y-%m-%d %H:%M:%S",tm_info);
  if(msglev>=loglev)
  {
    log=fopen(logfile, "a");
    if(NULL==log)
    {
      perror("could not open log file");
    }
    else
    { 
      fprintf(log,"%s ",tstr);
      fprintf(log,"%s\n",message);
      fclose(log);
    }
  }
}

void read_config()
{
  FILE *cfile;
  char *line=NULL;
  char par[20];
  float value;
  unsigned int reg;
  size_t len;
  ssize_t read;

  cfile=fopen(confile, "r");
  if(NULL!=cfile)
  {
    sprintf(message,"Read configuration file");
    logmessage(logfile,message,loglev,4);

    while((read=getline(&line,&len,cfile))!=-1)
    {
       if(sscanf(line,"%s %f",par,&value)!=EOF) 
       {
          if(strncmp(par,"LOGLEVEL",8)==0)
          {
             loglev=(int)value;
             sprintf(message,"Log level set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"CTRL_REG1",9)==0)
          {
             if(sscanf(line,"%s 0x%x",par,&reg)!=EOF)
             { 
                sprintf(message,"CTRL_REG1 set to 0x%x",reg);
                logmessage(logfile,message,loglev,4);
                ctrlreg1=(char)reg;
             }
          }
          if(strncmp(par,"CTRL_REG2",9)==0)
          {
             if(sscanf(line,"%s 0x%x",par,&reg)!=EOF)
             { 
                sprintf(message,"CTRL_REG2 set to 0x%x",reg);
                logmessage(logfile,message,loglev,4);
                ctrlreg2=(char)reg;
             }
          }
          if(strncmp(par,"OFF_X",5)==0)
          {
             xoffset=(int)value;
             sprintf(message,"X-offset set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"OFF_Y",5)==0)
          {
             yoffset=(int)value;
             sprintf(message,"Y-offset set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"OFF_Z",5)==0)
          {
             zoffset=(int)value;
             sprintf(message,"Z-offset set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"MAGNINT",7)==0)
          {
             magnint=(int)value;
             sprintf(message,"Field reading interval set to %d s",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"TDELAY",6)==0)
          {
             tdelay=(int)value;
             sprintf(message,"Delay after trigger before reading %d s",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"TEMPINT",7)==0)
          {
             tempint=(int)value;
             sprintf(message,"Temperature reading interval set to %d s",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"TOFFSET",7)==0)
          {
             toffset=(signed char)value;
             sprintf(message,"Temperature offset set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
       }
    }
    fclose(cfile);
  }
  else
  {
    sprintf(message, "Could not open %s", confile);
    logmessage(logfile, message, loglev,4);
  }
}

int cont=1; /* main loop flag */

int write_register(unsigned char reg, unsigned char value)
{
  int fd,rd;
  int cnt=0;
  unsigned char buf[10];

  if((fd=open(i2cdev, O_RDWR)) < 0) 
  {
    sprintf(message,"Failed to open i2c port");
    logmessage(logfile,message,loglev,4);
    return -1;
  }
  rd=flock(fd, LOCK_EX|LOCK_NB);
  cnt=i2lockmax;
  while((rd==1)&&(cnt>0)) // try again if port locking failed
  {
    sleep(1);
    rd=flock(fd, LOCK_EX|LOCK_NB);
    cnt--;
  }
  if(rd)
  {
    sprintf(message,"Failed to lock i2c port");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -2;
  }

  cont=0;
  buf[0]=reg;
  buf[1]=value;
  if(ioctl(fd, I2C_SLAVE, address) < 0) 
  {
    sprintf(message,"Unable to get bus access to talk to slave");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -3;
  }
  if((write(fd, buf, 2)) != 2) 
  {
    sprintf(message,"Error writing to i2c slave");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -4;
  }
  cont=1;
  close(fd);

  return 1;
}

int write_vector(unsigned char reg, short *xval, short *yval, short *zval)
{
  int fd,rd;
  int cnt=0;
  unsigned char buf[10];

  if((fd=open(i2cdev, O_RDWR)) < 0) 
  {
    sprintf(message,"Failed to open i2c port");
    logmessage(logfile,message,loglev,4);
    return -1;
  }
  rd=flock(fd, LOCK_EX|LOCK_NB);
  cnt=i2lockmax;
  while((rd==1)&&(cnt>0)) // try again if port locking failed
  {
    sleep(1);
    rd=flock(fd, LOCK_EX|LOCK_NB);
    cnt--;
  }
  if(rd)
  {
    sprintf(message,"Failed to lock i2c port");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -2;
  }

  cont=0;
  buf[0]=reg;
  buf[1]=(unsigned char)(*xval>>8);
  buf[2]=(unsigned char)(*xval&0x00FF);
  buf[3]=(unsigned char)(*yval>>8);
  buf[4]=(unsigned char)(*yval&0x00FF);
  buf[5]=(unsigned char)(*zval>>8);
  buf[6]=(unsigned char)(*zval&0x00FF);

  if(ioctl(fd, I2C_SLAVE, address) < 0) 
  {
    sprintf(message,"Unable to get bus access to talk to slave");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -3;
  }
  if((write(fd, buf, 7)) != 7) 
  {
    sprintf(message,"Error writing to i2c slave");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -4;
  }
  cont=1;
  close(fd);

  return 1;
}


int read_register(unsigned char reg)
{
  int value=0;
  int fd,rd;
  int cnt=0;
  unsigned char buf[10];
  if((fd=open(i2cdev, O_RDWR)) < 0) 
  {
    sprintf(message,"Failed to open i2c port");
    logmessage(logfile,message,loglev,4);
    return -1;
  }
  rd=flock(fd, LOCK_EX|LOCK_NB);
  cnt=i2lockmax;
  while((rd==1)&&(cnt>0)) // try again if port locking failed
  {
    sleep(1);
    rd=flock(fd, LOCK_EX|LOCK_NB);
    cnt--;
  }
  if(rd)
  {
    sprintf(message,"Failed to lock i2c port");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -2;
  }

  cont=0;
  buf[0]=reg;
  if(ioctl(fd, I2C_SLAVE, address) < 0) 
  {
    sprintf(message,"Unable to get bus access to talk to slave");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -3;
  }
  if((write(fd, buf, 1)) != 1) 
  {
    sprintf(message,"Error writing to i2c slave");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -4;
  }
  else
  {
    if(read(fd, buf,1) != 1) 
    {
      sprintf(message,"Unable to read from slave");
      logmessage(logfile,message,loglev,4);
      close(fd);
      return -5;
    }
    else 
    {
      value=(int)buf[0];
      cont=1; 
    }
  }
  close(fd);
  return value;
}

int read_vector(unsigned char reg, short *xval, short *yval, short *zval)
{
  int fd,rd;
  int cnt=0;
  unsigned char buf[10];

  if((fd=open(i2cdev, O_RDWR)) < 0) 
  {
    sprintf(message,"Failed to open i2c port");
    logmessage(logfile,message,loglev,4);
    return -1;
  }
  rd=flock(fd, LOCK_EX|LOCK_NB);
  cnt=i2lockmax;
  while((rd==1)&&(cnt>0)) // try again if port locking failed
  {
    sleep(1);
    rd=flock(fd, LOCK_EX|LOCK_NB);
    cnt--;
  }
  if(rd)
  {
    sprintf(message,"Failed to lock i2c port");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -2;
  }

  cont=0;
  buf[0]=reg;
  if(ioctl(fd, I2C_SLAVE, address) < 0) 
  {
    sprintf(message,"Unable to get bus access to talk to slave");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -3;
  }
  if((write(fd, buf, 1)) != 1) 
  {
    sprintf(message,"Error writing to i2c slave");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -4;
  }
  else
  {
    if(read(fd, buf,6) != 6) 
    {
      sprintf(message,"Unable to read from slave");
      logmessage(logfile,message,loglev,4);
      close(fd);
      return -5;
    }
    else 
    {
      *xval=((short)(buf[0]))<<8;
      *xval|=(short)buf[1];
      *yval=((short)(buf[2]))<<8;
      *yval|=(short)buf[3];
      *zval=((short)(buf[4]))<<8;
      *zval|=(short)buf[5];
      cont=1; 
    }
  }
  close(fd);

  return 1;
}

void write_temp(int t)
{
  FILE *tfile;
  tfile=fopen(tdatafile, "w");
  if(NULL==tfile)
  {
    sprintf(message,"could not write to file: %s",tdatafile);
    logmessage(logfile,message,loglev,4);
  }
  else
  { 
    fprintf(tfile,"%d",t);
    fclose(tfile);
  }
}

void read_temp()
{
  int temp=0;

  temp=((signed char)(read_register(0x0f))-toffset);
  if(cont==1)
  {
    sprintf(message,"temperature %d",temp);
    logmessage(logfile,message,loglev,4);
    write_temp(temp);
  }
}

void write_field()
{
  FILE *mfile;

  mfile=fopen(mdatafilex, "w");
  if(NULL==mfile)
  {
    sprintf(message,"could not write to file: %s",mdatafilex);
    logmessage(logfile,message,loglev,4);
  }
  else
  { 
    fprintf(mfile,"%d",Bx);
    fclose(mfile);
  }
  mfile=fopen(mdatafiley, "w");
  if(NULL==mfile)
  {
    sprintf(message,"could not write to file: %s",mdatafiley);
    logmessage(logfile,message,loglev,4);
  }
  else
  { 
    fprintf(mfile,"%d",By);
    fclose(mfile);
  }
  mfile=fopen(mdatafilez, "w");
  if(NULL==mfile)
  {
    sprintf(message,"could not write to file: %s",mdatafilez);
    logmessage(logfile,message,loglev,4);
  }
  else
  { 
    fprintf(mfile,"%d",Bz);
    fclose(mfile);
  }
}

void read_field()
{
  int ok=0;
  ok=read_vector(0x01,&Bx,&By,&Bz);
  if((cont==1)&&(ok==1))
  {
    sprintf(message,"Bx=%d By=%d Bz=%d",Bx,By,Bz);
    logmessage(logfile,message,loglev,4);
    write_field();
  }
}

void read_offset()
{
  int ok=0;
  ok=read_vector(0x09,&xoffset,&yoffset,&zoffset);

  if((cont==1)&&(ok==1))
  {
    sprintf(message,"offsetx=%d offsety=%d offsetz=%d",xoffset,yoffset,zoffset);
    logmessage(logfile,message,loglev,4);
  }
}

void stop(int sig)
{
  sprintf(message,"signal %d catched, stop",sig);
  logmessage(logfile,message,loglev,4);
  cont=0;
}

void terminate(int sig)
{
  sprintf(message,"signal %d catched",sig);
  logmessage(logfile,message,loglev,4);

  sleep(1);
  strcpy(message,"stop");
  logmessage(logfile,message,loglev,4);

  cont=0;
}

void hup(int sig)
{
  sprintf(message,"signal %d catched",sig);
  logmessage(logfile,message,loglev,4);
}


int main()
{  
  int ok=0;

  sprintf(message,"mag3110d v. %d started",version); 
  logmessage(logfile,message,loglev,4);

  signal(SIGINT,&stop); 
  signal(SIGKILL,&stop); 
  signal(SIGTERM,&terminate); 
  signal(SIGQUIT,&stop); 
  signal(SIGHUP,&hup); 

  read_config();

  int unxs=(int)time(NULL); // unix seconds
  int nxtemp=unxs+30; // next time to read temperature
  int nxtmagn=unxs; // next time to read field 

  pid_t pid, sid;
        
  pid=fork();
  if(pid<0) 
  {
    exit(EXIT_FAILURE);
  }

  if(pid>0) 
  {
    exit(EXIT_SUCCESS);
  }

  umask(0);

  /* Create a new SID for the child process */
  sid=setsid();
  if(sid<0) 
  {
    strcpy(message,"failed to create child process"); 
    logmessage(logfile,message,loglev,4);
    exit(EXIT_FAILURE);
  }
        
  if((chdir("/")) < 0) 
  {
    strcpy(message,"failed to change to root directory"); 
    logmessage(logfile,message,loglev,4);
    exit(EXIT_FAILURE);
  }
        
  /* Close out the standard file descriptors */
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  FILE *pidf;
  pidf=fopen(pidfile,"w");

  if(pidf==NULL)
  {
    sprintf(message,"Could not open PID lock file %s, exiting", pidfile);
    logmessage(logfile,message,loglev,4);
    exit(EXIT_FAILURE);
  }

  if(flock(fileno(pidf),LOCK_EX||LOCK_NB)==-1)
  {
    sprintf(message,"Could not lock PID lock file %s, exiting", pidfile);
    logmessage(logfile,message,loglev,4);
    exit(EXIT_FAILURE);
  }

  fprintf(pidf,"%d\n",getpid());
  fclose(pidf);

  if(read_register(0x07)!=0xC4) 
  {
    strcpy(message,"reading device id failed, exit"); 
    logmessage(logfile,message,loglev,4);
  }

  if(write_register(0x10,ctrlreg1)!=1)
  {
    strcpy(message,"writing CTRL_REG1 failed");
    logmessage(logfile,message,loglev,4); 
  }
  if(write_register(0x11,ctrlreg2)!=1)
  {
    strcpy(message,"writing CTRL_REG2 failed");
    logmessage(logfile,message,loglev,4); 
  }

  if(write_vector(0x09,&xoffset,&yoffset,&zoffset)!=1)
  {
    strcpy(message,"writing offset failed");
    logmessage(logfile,message,loglev,4); 
  }
  read_offset();

  while(cont==1)
  {
    unxs=(int)time(NULL); 

    if(((unxs>=nxtmagn)||((nxtmagn-unxs)>magnint))&&(magnint>10)) 
    {
      nxtmagn=magnint+unxs;
      write_register(0x10,ctrlreg1|0x02);
      sleep(tdelay);
      read_field();
    }

    if(((unxs>=nxtemp)||((nxtemp-unxs)>tempint))&&(tempint>10)) 
    {
      nxtemp=tempint+unxs;
      read_temp();
    }

    sleep(1);
  }

  strcpy(message,"remove PID file");
  logmessage(logfile,message,loglev,4);
  ok=remove(pidfile);

  return ok;
}
