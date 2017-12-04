#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>

#define UUID_CMD_STR "https://login.wx.qq.com/jslogin"
#define PARAM1_STR  "?appid=wx782c26e4c19acffb"
#define PARAM2_STR  "&redirect_uri=https://wx.qq.com/cgi-bin/mmwebwx-bin/webwxnewloginpage"
#define PARAM3_STR  "&lang=zh_CN"
#define PARAM4_STR  "&_=%u"

typedef struct recv_param_s
{
    char* buf;
    int len;
    int offset;
}recv_param_t;

static size_t write_data(void *ptr, size_t cnt, size_t nmemb, void *user)  
{
    int left,size = 0;
    recv_param_t* recv = (recv_param_t*)user;
     
    left = recv->len - recv->offset;
     
    if (left < nmemb)
        size = recv->len + nmemb + 8;
     
     if (size > 0)
      {
        if (recv->buf == NULL)
           recv->buf = malloc(size);
        else
           recv->buf = realloc(recv->buf,size);
        recv->len = size;
      }
     if(recv->buf)
      {
       memcpy(recv->buf + recv->offset,(char*)ptr,nmemb);
       recv->offset += nmemb;
       recv->buf[recv->offset] = 0;
      }
     return nmemb;
}

static int http_get_data(char* url,recv_param_t*  recv)
{
    CURL *curl;

    if ((curl = curl_easy_init()) == NULL)
        return -1;
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); 
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, recv);

    if (curl_easy_perform(curl) != CURLE_OK)
        return -1;

    curl_easy_cleanup(curl);

    return recv->offset;
}
static int http_post_data(char* url,char* data ,recv_param_t*  recv)
{
    CURL *curl;
    struct curl_slist *slist=NULL;
 
    printf("http_post_data: [%s] \n",data);
    slist = curl_slist_append(slist, "Content-Type: application/json; charset=UTF-8");

    if ((curl = curl_easy_init()) == NULL)
        return -1;
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); 
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, recv);

    if (curl_easy_perform(curl) != CURLE_OK)
        return -1;

    curl_easy_cleanup(curl);

    return recv->offset;
}

static int parse_data_from_response(char* resp,char* buf,int len)
{
    char *tmp1, *tmp2;

    printf("parse_data_from_response: [%s]\n",resp);

    if(strstr(resp,"200") == NULL)
        return -1; 

   if ((tmp1 = strchr(resp,'\"')) == NULL)
        return -1;  
  
   if ((tmp2 = strchr(tmp1+1,'\"')) == NULL)
        return -1;
 
   if (tmp2 - tmp1 > len)
      return -1;
    memcpy(buf,tmp1+1,tmp2-tmp1);
    buf[tmp2-tmp1-1] = 0;
    return tmp2-tmp1;
}

int wx_get_uuid(char* uuid,int len)
{
    char url[256];
    recv_param_t recv;
    char* cmd_str = UUID_CMD_STR PARAM1_STR PARAM2_STR PARAM3_STR PARAM4_STR;
    long tm = time(NULL);
    int ret = -1; 

   
    memset(&recv,0,sizeof(recv));
    sprintf(url,cmd_str,tm);

    if (http_get_data(url,&recv) <= 0)
        return -1;

     if (recv.buf != NULL)
      {
       ret = parse_data_from_response(recv.buf,uuid,len);
       free(recv.buf);
      }
     return ret;
}


int wx_get_scancode(char* buf, int len,char* uuid)
{
    char url[256];
    char* cmd_str = "https://login.weixin.qq.com/qrcode/%s";
    recv_param_t recv;
    int ret = -1; 
 
    memset(&recv,0,sizeof(recv));
    sprintf(url,cmd_str,uuid);

    if (http_get_data(url,&recv) <=0)
         return -1;
    if (recv.buf != NULL)
     {
       memcpy(buf,recv.buf,len > recv.offset ? recv.offset:len);
       free(recv.buf);
     }
    return recv.offset;
}

int wx_get_login_url(char* buf,int len,char* uuid)
{
    char url[256];
    char* cmd_str = "https://login.weixin.qq.com/cgi-bin/mmwebwx-bin/login?loginicon=true&uuid=%s&tip=1&r=%u%_=%u";
    long tm = time(NULL);
    recv_param_t recv;
    int ret = -1;

    memset(&recv,0,sizeof(recv));
    sprintf(url,cmd_str,uuid,~tm,tm);
  
   if (http_get_data(url,&recv) <= 0)
        return -1;

   if (recv.buf != NULL)
    {
       ret = parse_data_from_response(recv.buf,buf,len);
       free(recv.buf);
    }
   return ret;
}

typedef struct wx_param_s
{
    char skey[128];
    char sid[128];
    char pass_ticket[256];
    char uin[128];  
}wx_param_t;

int parse_param_from_response(char* resp,wx_param_t* param)
{
    char *tmp1, *tmp2;

    printf("[%s]: [%s] \n",__FUNCTION__,resp);
   
    memset(param,0,sizeof(wx_param_t));

    if ((tmp1 = strstr(resp,"<skey>")) == NULL)
         return -1;
    if ((tmp2 = strstr(tmp1+1,"</skey>")) == NULL)
          return -1;
    memcpy(param->skey,tmp1+6,tmp2-tmp1-6);
    if ((tmp1 = strstr(resp,"<wxsid>")) == NULL)
         return -1;
    if ((tmp2 = strstr(tmp1+1,"</wxsid>")) == NULL)
          return -1;
    memcpy(param->sid,tmp1+7,tmp2-tmp1-7);
   if ((tmp1 = strstr(resp,"<wxuin>")) == NULL)
         return -1;
   if ((tmp2 = strstr(tmp1+1,"</wxuin>")) == NULL)
          return -1;
    memcpy(param->uin,tmp1+7,tmp2-tmp1-7); 
   if ((tmp1 = strstr(resp,"<pass_ticket>")) == NULL)
         return -1;
   if ((tmp2 = strstr(tmp1+1,"</pass_ticket>")) == NULL)
          return -1;
    memcpy(param->pass_ticket,tmp1+13,tmp2-tmp1-13);
    printf("[%s]: skey[%s] \n",__FUNCTION__,param->skey); 
    printf("[%s]: sid[%s] \n",__FUNCTION__,param->sid); 
    printf("[%s]: uin[%s] \n",__FUNCTION__,param->uin); 
    printf("[%s]: pass[%s] \n",__FUNCTION__,param->pass_ticket);      
    return 1;
}


int wx_get_login_param(wx_param_t* param,char* redirect)
{
    char url[256];
    char* cmd_str = "%s&fun=new&version=v2";
    recv_param_t recv;
    int ret = -1;

    memset(&recv,0,sizeof(recv));
    sprintf(url,cmd_str,redirect);
    
    if (http_get_data(url,&recv) <= 0)
        return -1;  
    
    if (recv.buf != NULL)
     {
       ret =  parse_param_from_response(recv.buf,param);
       free(recv.buf);
     }
    return ret;
}

typedef struct wx_userinfo_s
{
    char name[128];
}wx_userinfo_t;

int parse_userinfo_from_response(char* resp,wx_userinfo_t* info)
{
    int i,len;
    printf("[%s]: [%s] \n",__FUNCTION__,resp);
    len = strlen(resp);
//    for(i=0;i<len;i++)
//         printf("%02x ",resp[i]);

    return 1;
}

int wx_get_self_info(wx_userinfo_t* info, wx_param_t* param)
{
    char url[256];
    char data[2048];
    char deviceid[32];
    int i;
    char* cmd_str = "https://wx2.qq.com/cgi-bin/mmwebwx-bin/webwxinit?r=%u&lang=ch_ZN&pass_ticket=%s";
    long tm = time(NULL);
    recv_param_t recv;
    int ret = -1;

    memset(&recv,0,sizeof(recv));
    deviceid[0] = 'e';
    for(i=1;i<16;i++)
        deviceid[i] = rand() % 10 + '0';
    deviceid[16] = 0;
    sprintf(url,cmd_str,~tm,param->pass_ticket);
    sprintf(data,"{\n\"BaseRequest\": {\n\"DeviceID\": \"%s\",\n\"Sid\": \"%s\",\n\"Skey\": \"%s\",\n\"Uin\": \"\%s\"\n}\n}", 
             deviceid,param->sid,param->skey,param->uin);

    if (http_post_data(url,data,&recv) <= 0)
        return -1;  
  
    if (recv.buf != NULL)
     {
       ret = parse_userinfo_from_response(recv.buf,info);
       free(recv.buf);
     }
    return ret;
}

typedef struct wx_contact_s
{
    char name[128];
    char type;
    char sex;
}wx_contact_t;

int wx_get_all_contacts(wx_contact_t* ct,int size,wx_param_t* param)
{
    return 0;
}

int save_scancode_to_file(char* file,char* image,int len)
{
    FILE* fp;
    if ((fp = fopen(file,"w")) == NULL)
       return -1;
    fwrite(image,1,len,fp);
    fclose(fp);
    return 0;
}
int main(int argc, char* argv[])
{
    char uuid[64];
    char image[65536];
    int  imglen; 
    wx_param_t param;
    wx_userinfo_t info;
    int i = 0;
    curl_global_init(CURL_GLOBAL_DEFAULT); 

    if (wx_get_uuid(uuid,sizeof(uuid)) <= 0)
     {
       printf(" get uuid error ! \n");
       return -1;
     }
     
    if ((imglen = wx_get_scancode(image,sizeof(image),uuid)) <= 0)
     {
       printf(" get scancode  error ! \n");
       return -1;     
     }

login:
    if (wx_get_login_url(image,sizeof(image),uuid) <= 0 && i++ < 3)
     {
       printf(" get login url error ! \n");
       sleep(3);
       goto login;      
     }
     
    if (wx_get_login_param(&param,image) <= 0)
     {
       printf(" get login param error ! \n");
       return -1; 
     }

    if (wx_get_self_info(&info,&param) <= 0)
     {
       printf(" get self info error ! \n");
       return -1; 
     } 

    curl_global_cleanup();
 
    return 0;
} 
