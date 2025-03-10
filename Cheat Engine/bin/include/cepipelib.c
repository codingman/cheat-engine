#ifndef CEPIPELIB_H
#define CEPIPELIB_H

#include <celog.h>
#include <cesocket.h>
#include <stdarg.h>
#include <stddef.h>
#include <errno.h>

//ifdef android:
//alternatively, registersymbol('_errno','__errno')
int *__cdecl __errno(void);
#undef errno
#define errno (*__errno())
//endif android

char *strerror(int errnum);

int sprintf(char *str, const char *format, ...);
int snprintf(char *str, int max, const char *format, ...);

char *strcpy(char *dest, const char *src);
size_t strlen(const char *s);

void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
char *strdup(const char *s);
void *memcpy(void *dest, const void *src, size_t n);

ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);


char *defaultpipename="unnamed pipe";


typedef struct
{
  unsigned char *buf;
  int size;
  int pos;  
} MemoryStream, *PMemoryStream;

typedef struct
{
  char *pipename;
  char *debugname;  
  int handle;
} PipeServer, *PPipeServer;


void ms_read(PMemoryStream this, void* buf, int count)
{
  if (this->pos+count>this->size)
    count=this->size-this->pos;
  
  memcpy(buf, &this->buf[this->pos], count);  
}

void ms_write(PMemoryStream this, void* buf, int count)
{
  if ((buf==NULL) || (count==0))
    return;
  
  while (this->pos+count>this->size)
  {
    
    debug_log("Realloc memorystream: Old size=%d", this->size);
    
    if (this->size<1048576)
      this->size*=2;
    else
      this->size+=1048576;
    
    debug_log("Realloc memorystream: New size=%d", this->size);
    
    this->buf=(unsigned char*)realloc(this->buf, this->size);
  }
  
  memcpy(&this->buf[this->pos], buf, count);
  
  this->pos+=count;
}

uint8_t ms_readByte(PMemoryStream this)
{
  uint8_t r;
  ms_read(this, (void*)&r,1);
  return r;
}

uint16_t ms_readWord(PMemoryStream this)
{
  uint16_t r;
  ms_read(this, (void*)&r,2);
  return r;
}

uint32_t ms_readDword(PMemoryStream this)
{
  uint32_t r;
  ms_read(this, (void*)&r,4);
  return r;
}

uint64_t ms_readQword(PMemoryStream this)
{
  uint64_t r;
  ms_read(this, (void*)&r,8);
  return r;
}

void ms_writeByte(PMemoryStream this, uint8_t v)
{
  ms_write(this, (void*)&v,1);
}

void ms_writeWord(PMemoryStream this, uint16_t v)
{
  ms_write(this, (void*)&v,2);
}

void ms_writeDword(PMemoryStream this, uint32_t v)
{
  ms_write(this, (void*)&v,4);
}

void ms_writeQword(PMemoryStream this, uint64_t v)
{
  ms_write(this, (void*)&v,8);
}

void ms_writeString(PMemoryStream this, char *str)
{
  int l=0;
  if (str)
    l=strlen(str);
  
  ms_writeWord(this, l);
  ms_write(this, str,l);
}

PMemoryStream ms_create(int initialsize)
{
  PMemoryStream r=(PMemoryStream)malloc(sizeof(MemoryStream));
  if (initialsize==0)
    initialsize=32;
  
  r->buf=(unsigned char *)malloc(initialsize);
  r->size=initialsize;
  r->pos=0;
  
  return r;  
}

void ms_destroy(PMemoryStream this)
{
  if (this->buf)
  {
    free(this->buf);
    this->buf=NULL;
  }
  
  free(this);
  
}

//-----pipeserver----

void ps_setDebugName(PPipeServer this, char *name)
{
  if (this->debugname)
    free(this->debugname);
  
  this->debugname=strdup(name);
}

ssize_t recvall (int s, void *buf, size_t size, int flags)
{
  ssize_t totalreceived=0;
  ssize_t sizeleft=size;
  unsigned char *buffer=(unsigned char*)buf;

  //printf("enter recvall\n");
  #ifdef _WINDOWS
    flags=flags | MSG_WAITALL;
#endif

  while (sizeleft>0)
  {
    ssize_t i=recv(s, &buffer[totalreceived], sizeleft, flags);

    if (i==0)
    {
      debug_log("recv returned 0\n");
      return i;
    }

    if (i==-1)
    {
      debug_log("recv returned -1\n");
      
      if (errno==EINTR)
      {
        debug_log("errno = EINTR\n");
        i=0;
      }
      else
      {
        debug_log("Error during recvall ( %s ) ", strerror(errno));
        return i; //read error, or disconnected
      }

    }

    totalreceived+=i;
    sizeleft-=i;
  }

  //printf("leave recvall\n");
  return totalreceived;
}

ssize_t sendall (int s, void *buf, size_t size, int flags)
{
  ssize_t totalsent=0;
  ssize_t sizeleft=size;
  unsigned char *buffer=(unsigned char*)buf;

  while (sizeleft>0)
  {
    ssize_t i=send(s, &buffer[totalsent], sizeleft, flags);

    if (i==0)
    {
      return i;
    }

    if (i==-1)
    {
      if (errno==EINTR)
        i=0;
      else
      {
        debug_log((char *)"Error during sendall ( %s )\n", strerror(errno));
        return i;
      }
    }

    totalsent+=i;
    sizeleft-=i;
  }

  return totalsent;
}


void ps_read(PPipeServer this, void* buf, int count)
{
  if (this->handle)
  {
    int r=recvall(this->handle, buf,count,0);
    if (r!=count)
    {
        close(this->handle);
        this->handle=0;
    }
  }
}

void ps_write(PPipeServer this, void* buf, int count)
{
  if (this->handle)
  {
    int r=sendall(this->handle, buf,count,0);
    if (r!=count)
    {
        close(this->handle);
        this->handle=0;
    }
  }
}



uint8_t ps_readByte(PPipeServer this)
{
  uint8_t r;
  ps_read(this, (void*)&r,1);
  return r;
}

uint16_t ps_readWord(PPipeServer this)
{
  uint16_t r;
  ps_read(this, (void*)&r,2);
  return r;
}

uint32_t ps_readDword(PPipeServer this)
{
  uint32_t r;
  ps_read(this, (void*)&r,4);
  return r;
}

uint64_t ps_readQword(PPipeServer this)
{
  uint64_t r;
  ps_read(this, (void*)&r,8);
  return r;
}

void ps_writeByte(PPipeServer this, uint8_t v)
{
  ps_write(this, (void*)&v,1);
}

void ps_writeWord(PPipeServer this, uint16_t v)
{
  ps_write(this, (void*)&v,2);
}

void ps_writeDword(PPipeServer this, uint32_t v)
{
  ps_write(this, (void*)&v,4);
}

void ps_writeQword(PPipeServer this, uint64_t v)
{
  ps_write(this, (void*)&v,8);
}

void ps_writeMemStream(PPipeServer this, PMemoryStream ms)
{
  ps_writeDword(this, ms->pos);
  ps_write(this, ms->buf, ms->pos);      
}

int ps_isvalid(PPipeServer this)
{
  return this->handle!=0;  
}

void ps_destroy(PPipeServer this)
{
  if (this->pipename)
    free(this->pipename);
  
  if (this->debugname)
    free(this->debugname);
  
  if (this->handle)
    close(this->handle);
  
  free(this);  
}

PipeServer *CreatePipeServer(char *name)
{
    //single connect system
    int s;
    debug_log("CreatePipeServer(\"%s\")\n",name);
    s=socket(AF_UNIX, SOCK_STREAM,0);
    if (s)
    {
        char *path=malloc(100);

        snprintf(path,100," %s", name);

        struct sockaddr_un address;
        address.sun_family=AF_UNIX;
        strcpy(address.sun_path, path);

        int al=(int)SUN_LEN(&address);
        
        debug_log("al=%d\n",al);

        address.sun_path[0]=0;

        int optval=1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, optval);

        int i;
        i=bind(s,(struct sockaddr *)&address, al);
        if (i==0)
        {
            debug_log((char*)"Starting to listen\n");
            i=listen(s,32);

            debug_log("Listen returned %d\n",i);
            if (i==0)
            {
                struct sockaddr_un addr_client;
                socklen_t clisize=sizeof(addr_client);
                int a=-1;


                while (a==-1)
                {
                  debug_log("Calling accept (clisize=%d)",clisize);
                  a=accept(s, (struct sockaddr *)&addr_client, &clisize);

                  if (a!=-1)
                  {
                      debug_log("Connection accepted");
                      debug_log("Closing the listener");
                      close(s); //stop listening
                      
                      PPipeServer t=malloc(sizeof(PipeServer));                      
                      t->pipename=strdup(name);
                      t->debugname=NULL;
                      t->handle=a;
                      return t;
                  }
                  else
                  {
                    debug_log("accept returned %d (%d - %s )", a, errno, strerror(errno));                    
                    close(s);
                    break;
                  }
                }

            }
            else
                debug_log((char*)"Listen failed");

        }
        else
          debug_log((char*)"Bind failed\n");
    }
    else
        debug_log((char*)"Failure creating socket\n");

    return (void*)0;
}


#endif