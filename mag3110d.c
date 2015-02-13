/**************************************************************************
 * 
 * Read magnetic field from MAG3110 chip with I2C and write it to a log file. 
 *       
 * Copyright (C) 2014 - 2015 Jaakko Koivuniemi.
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
 * Edit: Fri Feb 13 18:17:50 CET 2015
 *
 * Jaakko Koivuniemi
 **/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
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

const int version=20150213; // program version
int tempint=0; // temperature reading interval
int magnint=300; // field reading interval [s]

const char mdatafilex[200]="/var/lib/mag3110d/Bx";
const char mdatafiley[200]="/var/lib/mag3110d/By";
const char mdatafilez[200]="/var/lib/mag3110d/Bz";
const char mdatafile[200]="/var/lib/mag3110d/B";

const char tdatafile[200]="/var/lib/mag3110d/temperature";

const char azdatafile[200]="/var/lib/mag3110d/azimuth";
const char eldatafile[200]="/var/lib/mag3110d/altitude";
const char qldatafile[200]="/var/lib/mag3110d/quality";

const char i2cdev[100]="/dev/i2c-1";
int  address=0x0e;
const int  i2lockmax=10; // maximum number of times to try lock i2c port  

const char confile[200]="/etc/mag3110d_config";

const char pidfile[200]="/var/run/mag3110d.pid";

int loglev=3;
const char logfile[200]="/var/log/mag3110d.log";
char message[200]="";

short Bx=0,By=0,Bz=0; // magnetic field [0.1 uT]
int B=0; // total magnetic field [0.1 uT]
short BxN=0,ByN=0,BzN=0; // magnetic vector to North [0.1uT]
int BN=0; // total magnetic field to North [0.1uT]
short xoffset=0,yoffset=0,zoffset=0; // magnetic field offset [0.1 uT]
int compass=0; // 1=calculate compass bearing and elevation, 2=bearing only
int quality=0; // quality flag for bearing calculation
double azimuth=0; // azimuth (bearing) from magnetic North [deg]
double altitude=0; // altitude (elevation) from horizon [deg]
double declination=0; // declination between magnetic North and true North
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
             xoffset=(short)value;
             sprintf(message,"X-offset set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"OFF_Y",5)==0)
          {
             yoffset=(short)value;
             sprintf(message,"Y-offset set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"OFF_Z",5)==0)
          {
             zoffset=(short)value;
             sprintf(message,"Z-offset set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"NORTHBX",7)==0)
          {
             BxN=(short)value;
             sprintf(message,"Bx North set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"NORTHBY",7)==0)
          {
             ByN=(short)value;
             sprintf(message,"By North set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"NORTHBZ",7)==0)
          {
             BzN=(short)value;
             sprintf(message,"Bz North set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"DECLINATION",11)==0)
          {
             declination=(double)value;
             sprintf(message,"Declination set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"COMPASS",7)==0)
          {
             if(value==1)
             {
               compass=1;
               sprintf(message,"Calculate compass bearing");
               logmessage(logfile,message,loglev,4);
             }
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

// calculate vector spherical coordinates r, theta, phi
void cart2spher(double x, double y, double z, double *r, double *theta, double *phi)
{
  double d=sqrt(x*x+y*y+z*z);
  *r=d;
  *theta=acos(z/d);
  *phi=atan2(y,x);
  sprintf(message,"Br=%f Btheta=%f Bphi=%f",*r,*theta,*phi);
  logmessage(logfile,message,loglev,2);
} 

// write compass data to file
void write_bearing()
{
  FILE *cfile;

  cfile=fopen(azdatafile, "w");
  if(NULL==cfile)
  {
    sprintf(message,"could not write to file: %s",azdatafile);
    logmessage(logfile,message,loglev,4);
  }
  else
  { 
    fprintf(cfile,"%-3.0f",azimuth);
    fclose(cfile);
  }
  cfile=fopen(eldatafile, "w");
  if(NULL==cfile)
  {
    sprintf(message,"could not write to file: %s",eldatafile);
    logmessage(logfile,message,loglev,4);
  }
  else
  { 
    fprintf(cfile,"%-3.0f",altitude);
    fclose(cfile);
  }
  cfile=fopen(qldatafile, "w");
  if(NULL==cfile)
  {
    sprintf(message,"could not write to file: %s",qldatafile);
    logmessage(logfile,message,loglev,4);
  }
  else
  { 
    fprintf(cfile,"%d",quality);
    fclose(cfile);
  }
}

// calculate azimuth or compass bearing, use law of cosines on xy-plane with
// cross product of reference magnetic North vector and measured magnetic 
// vector to determine if the angle is between 0 - 180 or 180 - 360
// tuning of the quality limits may be necessary
void calc_bearing()
{
  double a=0,b=0,c=0,ac=0,alpha=0,B2=0,r=0,theta=0,thetaN=0;
  int P=0;

  B2=((double)BxN)*((double)BxN)+((double)ByN)*((double)ByN);
  b=sqrt(B2);
  B2=((double)Bx)*((double)Bx)+((double)By)*((double)By);
  c=sqrt(B2);
  B2=((double)(BxN-Bx))*((double)(BxN-Bx))+((double)(ByN-By))*((double)(ByN-By));
  a=sqrt(B2);
  ac=(b*b+c*c-a*a)/(2*b*c);
  if(ac>1) ac=1;
  if(ac<-1) ac=-1;
  alpha=acos(ac);

  P=((int)BxN)*((int)By)-((int)ByN)*((int)Bx);
  if(P<0) alpha=2*M_PI-alpha;
  azimuth=(360-180*alpha/M_PI)-declination;

  r=((double)B)/((double)BN);
  if((r>0.90)&&(r<1.10)) 
  {
    r=c/b;
    if((r<0.5)||(r>1.5)) quality=0; else quality=1; 
  } 
  else quality=0;

  thetaN=acos(((double)BzN)/((double)BN));
  theta=acos(((double)Bz)/((double)B));
  altitude=180*(theta-thetaN)/M_PI;

  sprintf(message,"azimuth=%-3.0f altitude=%-3.0f quality=%d",azimuth,altitude,quality);
  logmessage(logfile,message,loglev,4);
  write_bearing();  
}

// simple compass bearing from Bx and By
void calc_bearing_only()
{
  double alpha=0;
  alpha=atan2(Bx,By);
  if(alpha<0) alpha+=2*M_PI;
  azimuth=180*alpha/M_PI-declination;
  if(azimuth>=360) azimuth-=360;
  altitude=0;

  if((B<200)||(B>700)) quality=0;
  else quality=1;

  sprintf(message,"azimuth=%-3.0f quality=%d",azimuth,quality);
  logmessage(logfile,message,loglev,4);
  write_bearing();  
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
    sprintf(message,"temperature %+d",temp);
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
  mfile=fopen(mdatafile, "w");
  if(NULL==mfile)
  {
    sprintf(message,"could not write to file: %s",mdatafile);
    logmessage(logfile,message,loglev,4);
  }
  else
  { 
    fprintf(mfile,"%d",B);
    fclose(mfile);
  }
}

void read_field()
{
  int ok=0,B2=0;
  ok=read_vector(0x01,&Bx,&By,&Bz);
  if((cont==1)&&(ok==1))
  {
    B2=((int)Bx)*((int)Bx)+((int)By)*((int)By)+((int)Bz)*((int)Bz);
    B=sqrt(B2);
    sprintf(message,"Bx=%d By=%d Bz=%d B=%d",Bx,By,Bz,B);
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
  else
  {
    if(write_register(0x10,ctrlreg1)!=1)
    {
      strcpy(message,"writing CTRL_REG1 failed");
      logmessage(logfile,message,loglev,4); 
    }
    else
    {
      if(write_register(0x11,ctrlreg2)!=1)
      {
        strcpy(message,"writing CTRL_REG2 failed");
        logmessage(logfile,message,loglev,4); 
      }
      else
      {
        if(write_vector(0x09,&xoffset,&yoffset,&zoffset)!=1)
        {
          strcpy(message,"writing offset failed");
          logmessage(logfile,message,loglev,4); 
        }
        read_offset();
      }
    }
  }

  int B2=0;
  if(compass==1)
  {
    B2=((int)BxN)*((int)BxN)+((int)ByN)*((int)ByN)+((int)BzN)*((int)BzN);
    BN=sqrt(B2);
    if(BN==0)
    {
      sprintf(message,"North vector not defined, calculate only compass bearing");
      logmessage(logfile,message,loglev,4);
      compass=2;
    }
    else if((BN<200)||(BN>700))
    {
      sprintf(message,"total North field BN=%d [0.1uT] ouside of [200,700], dropping compass",BN);
      logmessage(logfile,message,loglev,4);
      compass=0;
    }
  }

  while(cont==1)
  {
    unxs=(int)time(NULL); 

    if(((unxs>=nxtmagn)||((nxtmagn-unxs)>magnint))&&(magnint>10)) 
    {
      nxtmagn=magnint+unxs;
      write_register(0x10,ctrlreg1|0x02);
      sleep(tdelay);
      read_field();
      if(compass==1) calc_bearing();
      else if(compass==2) calc_bearing_only();
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
