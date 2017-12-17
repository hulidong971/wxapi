#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include "cjson.h"


typedef struct recv_param_s
{
    char* buf;
    int len;
    int offset;
}recv_param_t;

int wx_save_data(char* file,char* image,int len)
{
    FILE* fp;
    if ((fp = fopen(file,"w")) == NULL)
       return -1;
    fwrite(image,1,len,fp);
    fclose(fp);
    return 0;
}

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
//    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); 
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

//    printf("http_post_data: [%s] \n",data);

    slist = curl_slist_append(slist, "Content-Type: application/json; charset=UTF-8");

    if ((curl = curl_easy_init()) == NULL)
        return -1;

//    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); 
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
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


//    printf("parse_data_from_response: [%s] \n",resp);

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
    char* cmd_str = "https://login.weixin.qq.com/jslogin?appid=wx782c26e4c19acffb&fun=new&lang=zh_CN&_=%u";
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
    char devid[32];
    char name[128];
}wx_param_t;

int parse_param_from_response(char* resp,wx_param_t* param)
{
    char *tmp1, *tmp2;
    int i;

//    printf("[%s]:  %s \n",__FUNCTION__,resp);


    memset(param,0,sizeof(wx_param_t));

    param->devid[0] = 'e';
    for(i=1;i<16;i++)
        param->devid[i] = rand() % 10 + '0';
    param->devid[16] = 0;

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
#if 0
    printf("[%s]: skey[%s] \n",__FUNCTION__,param->skey); 
    printf("[%s]: sid[%s] \n",__FUNCTION__,param->sid); 
    printf("[%s]: uin[%s] \n",__FUNCTION__,param->uin); 
    printf("[%s]: pass[%s] \n",__FUNCTION__,param->pass_ticket);      
#endif
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


static int wx_get_resp_retcode(cJSON* obj)
{
    if ((obj = cJSON_GetObjectItemCaseSensitive(obj,"BaseResponse")) == NULL)
    {
       printf("[%s]: parse JSON(BaseResponse) fail \n",__FUNCTION__);
       return -1;
    }

    if ((obj = cJSON_GetObjectItemCaseSensitive(obj,"Ret")) == NULL)
    {
       printf("[%s]: parse JSON(Ret) fail \n",__FUNCTION__);
       return -1;
    }

   return obj->valueint;
}

static int wx_parse_retcode_from_response(char* resp)
{
    cJSON* root;
    int ret;

//    printf("[%s]: %s \n",__FUNCTION__,resp);

    if ((root = cJSON_Parse(resp)) == NULL)
    {
       fprintf(stderr,"[%s]: parse JSON(root) fail \n",__FUNCTION__);
       return -1;
    }
    ret = wx_get_resp_retcode(root);
    cJSON_Delete(root);
    return  ret; 
}
static int wx_parse_userinfo_from_response(char* resp,wx_param_t* param)
{
    cJSON* obj,*root;
    param->name[0] = 0;

//   printf("[%s]: %s \n",__FUNCTION__,resp);

    if ((root = cJSON_Parse(resp)) == NULL)
    {
       fprintf(stderr,"[%s]: parse JSON(root) fail \n",__FUNCTION__);
       return -1;
    }

    if (wx_get_resp_retcode(root) != 0)
    {
       fprintf(stderr,"[%s]: parse JSON(retcode) fail \n",__FUNCTION__);
        goto exit;
    }

    if ((obj = cJSON_GetObjectItemCaseSensitive(root,"User")) == NULL)
    {
       fprintf(stderr,"[%s]: parse JSON(user) fail \n",__FUNCTION__);
       goto exit;
    }

    if ((obj = cJSON_GetObjectItemCaseSensitive(obj,"UserName")) == NULL)
    {
       fprintf(stderr,"[%s]: parse JSON(username) fail \n",__FUNCTION__);
       goto exit;
    }
    strcpy(param->name,obj->valuestring);

exit:
    cJSON_Delete(root);
    return strlen(param->name);
}

#define FMT_WX_BASE_REQUEST  "\"BaseRequest\": {\n\"DeviceID\": \"%s\",\n\"Sid\": \"%s\",\n\"Skey\": \"%s\",\n\"Uin\": \"\%s\"\n}"
int wx_get_self_info(wx_param_t* param)
{
    char url[256];
    char data[2048];
    int i;
    char* cmd_str = "https://wx2.qq.com/cgi-bin/mmwebwx-bin/webwxinit?r=%u&lang=ch_ZN&pass_ticket=%s";
    long tm = time(NULL);
    recv_param_t recv;
    int ret = -1;

    memset(&recv,0,sizeof(recv));

    sprintf(url,cmd_str,~tm,param->pass_ticket);

    sprintf(data,"{\n" FMT_WX_BASE_REQUEST "\n}",  param->devid,param->sid,param->skey,param->uin);

    if (http_post_data(url,data,&recv) <= 0)
        return -1;  
  
    if (recv.buf != NULL)
     {
       ret = wx_parse_userinfo_from_response(recv.buf,param);
       free(recv.buf);
     }
    return ret;
}

typedef struct wx_contact_s
{
    char name[128];
    char nick[64];
    char type;
    char sex;
}wx_contact_t;



static int wx_parse_contacts_from_response(char* resp, wx_contact_t* ct,int size)
{
    cJSON* obj,*root,*list,*item;
    int num,i;

//    printf("[%s]: %s \n",__FUNCTION__,resp);

    if ((root = cJSON_Parse(resp)) == NULL)
    {
       printf("[%s]: parse JSON(root) fail \n",__FUNCTION__);
       return -1;
    }
    if (wx_get_resp_retcode(root) != 0)
    {
       printf("[%s]: parse JSON(retcode) fail \n",__FUNCTION__);
       return -1;
    }

    if ((obj = cJSON_GetObjectItemCaseSensitive(root,"MemberList")) == NULL)
    {
       printf("[%s]: parse JSON(user) fail \n",__FUNCTION__);
       return -1;
    }

    if ((num = cJSON_GetArraySize(obj) ) <= 0)
    {
       printf("[%s]: get contact num fail [%d] \n",__FUNCTION__,num);
       return -1;
    }	

    list = obj;

    if (num > size)
       num = size;

    for(i=0;i<num;i++)
    {
       if ((item = cJSON_GetArrayItem(list,i)) == NULL)
       {
         printf("[%s]: get array item [%d] fail \n",__FUNCTION__,i);
         return -1;
       }

       if ((obj = cJSON_GetObjectItemCaseSensitive(item,"UserName")) == NULL)
       {
         printf("[%s]: parse JSON(UserName) fail \n",__FUNCTION__);
         return -1;
       }

       strcpy(ct[i].name,obj->valuestring);

       if ((obj = cJSON_GetObjectItemCaseSensitive(item,"NickName")) == NULL)
       {
         printf("[%s]: parse JSON(NickName) fail \n",__FUNCTION__);
         return -1;
       }

       strcpy(ct[i].nick,obj->valuestring);
    }

    cJSON_Delete(root);

    return num;
}

char* wx_find_user_by_nick(char* nick,wx_contact_t* ct,int len)
{
    int i;
    for(i=0;i<len;i++)
    {
      if (strcmp(ct[i].nick,nick)==0)
         return  ct[i].name;
    } 
    return NULL;
}


static int wx_print_contact(wx_contact_t* ct,int num)
{
    int i;

    for(i = 0;i< num; i++)
       printf("Contact[%]: name [%s] nick[%s] \n",ct[i].name,ct[i].nick);
    return 0;
}

int wx_get_all_contacts(wx_contact_t* ct,int size,wx_param_t* param)
{
    char url[256];
    char data[2048];
    char deviceid[32];
    int i;  
    char* cmd_str = "https://wx2.qq.com/cgi-bin/mmwebwx-bin/webwxgetcontact?r=%u&lang=ch_ZN&pass_ticket=%s";
    long tm = time(NULL);
    recv_param_t recv;
    int ret = -1;

    memset(&recv,0,sizeof(recv));

    sprintf(url,cmd_str,~tm,param->pass_ticket);
    sprintf(data,"{\n" FMT_WX_BASE_REQUEST "\n}", 
             param->devid,param->sid,param->skey,param->uin);

    if (http_post_data(url,data,&recv) <= 0)
        return -1;  
  
    if (recv.buf != NULL)
     {
       ret = wx_parse_contacts_from_response(recv.buf,ct,size);
       free(recv.buf);
     }
    return ret;
}


#define FMT_WX_MSG "\"Msg\": {\n\"Type\":1,\n\"Content\":\"%s\",\n\"FromUserName\":\"%s\",\n\"ToUserName\":\"%s\",\n\"ClientMsgId\":\"%u\",\n\"LocalID\":\"%u\"\n}"
int  wx_send_msg(char* userid,char* msg,wx_param_t* param)
{
    char url[256];
    char data[2048];
    char deviceid[32];
    char* cmd_str = "https://wx2.qq.com/cgi-bin/mmwebwx-bin/webwxsendmsg?r=%u&lang=ch_ZN&pass_ticket=%s";
    long tm = time(NULL);
    recv_param_t recv;
    int ret = -1,tmid;

    if (userid == NULL || msg == NULL)
        return -1;

    memset(&recv,0,sizeof(recv));

    sprintf(url,cmd_str,~tm,param->pass_ticket);

    tmid = (tm << 4) | (rand()%16);
    sprintf(data,"{\n" FMT_WX_BASE_REQUEST ", \n" FMT_WX_MSG "\n}", param->devid,param->sid,param->skey,param->uin,msg,param->name,userid,tm,tm);

    if (http_post_data(url,data,&recv) <= 0)
        return -1;  
  
    if (recv.buf != NULL)
     {
       ret = wx_parse_retcode_from_response(recv.buf);
       free(recv.buf);
     }
    return ret;
}
//Return: the nmber of user be sent to message
int wx_send_msg_to_all(wx_contact_t* ct,int len,char* msg,wx_param_t* param)
{
    int i;
   
    for(i=0;i< len;i++)
    {
       if (wx_send_msg(ct[i].name,msg,param) != 0)
          return i;
    }
    return len;
}

void wx_send_response(int client, int len,char* body)
{
   char buf[1024];

   strcpy(buf, "HTTP/1.0 200 OK\r\n");
   send(client, buf, strlen(buf), 0);
   sprintf(buf, "Content-Type: image/jpeg\r\n");
   send(client, buf, strlen(buf), 0);
   sprintf(buf, "Content-Length: %u\r\n",len);
   send(client, buf, strlen(buf), 0);
   strcpy(buf, "\r\n");
   send(client, buf, strlen(buf), 0);
   send(client,body,len,0);
}

#define WX_SCANCODE_MAX_LEN	65536

int wx_handle_request(int client,char* filename)
{
    char uuid[64];
    char* image = NULL;
    int  len; 
    wx_param_t param;
    wx_contact_t* ct;
    int i = 0,ret = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT); 

    if (wx_get_uuid(uuid,sizeof(uuid)) <= 0)
     {
       fprintf(stderr," get uuid error ! \n");
       ret = -1;
       goto exit;
    }

    if ((image = malloc(WX_SCANCODE_MAX_LEN)) == NULL)
    {
       fprintf(stderr," get uuid error ! \n");
       ret = -1;
       goto exit;
    }

    if ((len = wx_get_scancode(image,WX_SCANCODE_MAX_LEN,uuid)) <= 0)
     {
       fprintf(stderr," get scancode  error ! \n");
       ret = -1;
       goto exit;    
     }

     wx_send_response(client,len,image);

login:
    if (wx_get_login_url(image,WX_SCANCODE_MAX_LEN,uuid) <= 0 && i++ < 3)
     {
       sleep(3);
       goto login;      
     }
    
     if (i >= 3)
     {
       fprintf(stderr," login confirm timeout ! \n");
       ret = -1;
       goto exit;  
     }

    if (wx_get_login_param(&param,image) <= 0)
     {
       fprintf(stderr," get login param error ! \n");
       ret = -1;
       goto exit;
     }

    if (wx_get_self_info(&param) <= 0)
     {
       fprintf(stderr," get self info error ! \n");
       ret = -1;
       goto exit;
     } 

    ct = (wx_contact_t*)image;

    if ((len = wx_get_all_contacts(ct,WX_SCANCODE_MAX_LEN / sizeof(wx_contact_t),&param )) <= 0)
     {
        fprintf(stderr," get contacts error ! \n");
        ret = -1;
        goto exit;
     } 
    if (wx_send_msg_to_all(ct,len,"test message",&param) != 0)
    {
        fprintf(stderr," send msg error ! \n");
        ret = -1;
        goto exit;
    }

exit:
    if (image)
      free(image);

    curl_global_cleanup();
 
    return ret;
} 
