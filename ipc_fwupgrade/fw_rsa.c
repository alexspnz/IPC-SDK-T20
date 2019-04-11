#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<malloc.h>
#include<openssl/rsa.h>
#include<openssl/pem.h>
#include<openssl/sha.h>
#include<openssl/crypto.h>
#include<openssl/err.h>
#include<openssl/hmac.h>

#include"fw_rsa.h"

char *ysx_rsa_pub_encrypt(char *instr, char *path_key, int inlen)
{
    char *p_hex;
    RSA *p_rsa;
    FILE *file;
    int rsa_len=0 , flen=0 , base64_len = 0 , encrypt_size = 0;
	char *base64_data = NULL , *encrypt_data = NULL , *p = NULL; 

    if((file=fopen(path_key,"r"))==NULL){
	    printf("Key read error!\n");
	    return NULL;
    }

    if((p_rsa=PEM_read_RSA_PUBKEY(file,NULL,NULL,NULL))==NULL){
        printf("PEM read err!\n");
		fclose(file);		
        return NULL;
    }
    else    
    {
		fclose(file); 
    }

    flen=inlen;
    rsa_len=RSA_size(p_rsa);
    printf("[%s:%d] indata_len=%d\n", __func__, __LINE__, flen); 
	
	encrypt_data = (char *)malloc(1024*2);
	if(!encrypt_data){
		perror("malloc for decrypt data error\n");
		RSA_free(p_rsa);
		return NULL;
	}
	memset(encrypt_data, 0, 1024*2);

    if(flen > (rsa_len-11))
    {
        int time = flen/(rsa_len-11);
        int remain = flen%(rsa_len-11);
        int i;  

        for(i = 0; i < time; i++)
        {       
            p = instr+i*(rsa_len-11);

            encrypt_size = RSA_public_encrypt(rsa_len-11,(unsigned char *)p,(unsigned char*)(encrypt_data+i*rsa_len),p_rsa,RSA_PKCS1_PADDING);
			if(encrypt_size < 0)
            {
                printf("RSA_public_encrypt error!\n");
				goto finally;
            }

        }
        if(remain > 0)
        {
            p = instr+time*(rsa_len-11);
			
            encrypt_size = RSA_public_encrypt(remain,(unsigned char *)p,(unsigned char*)(encrypt_data+time*rsa_len),p_rsa,RSA_PKCS1_PADDING);
            if(encrypt_size < 0)
            {
				printf("RSA_public_encrypt error!\n");
				goto finally;
            }
        }

        base64_data =  ipc_base64_encode(encrypt_data, time*rsa_len+encrypt_size, &base64_len);        	
    }
    else
    {
        if(RSA_public_encrypt(rsa_len-11,(unsigned char *)instr,(unsigned char*)encrypt_data,p_rsa,RSA_PKCS1_PADDING)<0)
        {
			printf("RSA_public_encrypt error!\n");
			goto finally;
        }
        base64_data =  ipc_base64_encode(encrypt_data, rsa_len, &base64_len);        	
    }
	printf("[%s:%d] success!\n", __func__, __LINE__);
		
finally:
	if(p_rsa)
		RSA_free(p_rsa);
	if(encrypt_data)
		free(encrypt_data);

	return base64_data;		
}


char *ysx_rsa_pri_decrypt(char *instr, char *path_key)
{
    RSA *p_rsa;
    FILE *file;
    int rsa_len = 0 ,flen = 0 , ret = -1;

    char *decrypt_data = NULL , *p = NULL;

    if((file=fopen(path_key,"r"))==NULL){
	    perror("open key file error");
	    return NULL;
    }
	
    if((p_rsa=PEM_read_RSAPrivateKey(file,NULL,NULL,NULL))==NULL){
	    printf("PEM read err!\n"); 
		fclose(file);
	    return NULL;
    }
    else
    {
	    fclose(file);		 
    }

    rsa_len=RSA_size(p_rsa);
	
	decrypt_data = (char *)malloc(1024*2);
	if(!decrypt_data){
		perror("malloc for decrypt data error\n");
		RSA_free(p_rsa);
		return NULL;
	}
	memset(decrypt_data, 0, 1024*2);
	
    char * base64_data = base64_decode(instr, strlen(instr),&flen);	
	printf("[%s:%d] indata_len=%d\n", __func__, __LINE__, flen);

    if(flen > (rsa_len))
    {
        int time = flen/(rsa_len);
        int i;  

        for(i = 0; i < time; i++)
        {
            p = base64_data+i*rsa_len;

            if(RSA_private_decrypt(rsa_len,(unsigned char *)p,(unsigned char*)(decrypt_data+i*(rsa_len-11)),p_rsa,RSA_PKCS1_PADDING)<0)
            {       
                printf("RSA_private_decrypt error!\n");
				ret = -1;
				goto finally;
            }       
        }

    }
    else{
        if(RSA_private_decrypt(rsa_len,(unsigned char *)base64_data,(unsigned char*)decrypt_data,p_rsa,RSA_PKCS1_PADDING)<0)
        {
	        printf("RSA_private_decrypt error!\n");
			ret = -1;			
			goto finally;
        }
    }
	ret = 1;
	printf("[%s:%d] success!\n",__func__, __LINE__);
		
finally:
	if(p_rsa)
		RSA_free(p_rsa);
	if(base64_data)
		free(base64_data);

	if(!ret)
	{
		free(decrypt_data);
		decrypt_data = NULL;
	}

	return decrypt_data;	
}

char *ysx_rsa_pub_decrypt(char *instr, char *path_key)
{
    RSA *p_rsa = NULL;
    FILE *file = NULL;
    int rsa_len = 0, flen = 0 , ret = 0;

    char *decrypt_data = NULL , *p = NULL;

    if((file=fopen(path_key,"r"))==NULL){
        perror("open key file error");
		return NULL;
    }
    if((p_rsa=PEM_read_RSA_PUBKEY(file,NULL,NULL,NULL))==NULL){
        printf("PEM read err!\n"); 
    	fclose(file);
		return NULL;
	}
    else
    {
		fclose(file);		
    }

    rsa_len=RSA_size(p_rsa);
	
	decrypt_data = (char *)malloc(1024*2);
	if(!decrypt_data){
		perror("malloc for decrypt data error\n");
		RSA_free(p_rsa);
		return NULL;
	}
	memset(decrypt_data, 0, 1024*2);
	
    char * base64_data = base64_decode(instr, strlen(instr),&flen);	
	printf("[%s:%d] indata_len=%d\n", __func__, __LINE__, flen);

    if(flen > (rsa_len))
    {
        int time = flen/(rsa_len);
        int i;  

        for(i = 0; i < time; i++)
        {
            p = base64_data+i*rsa_len;

            if(RSA_public_decrypt(rsa_len,(unsigned char *)p,(unsigned char*)(decrypt_data+i*(rsa_len-11)),p_rsa,RSA_PKCS1_PADDING)<0)
            {       
                printf("RSA_public_decrypt error!\n");
				ret = -1;
				goto end;
            }       
        }
    }
    else{

        if(RSA_public_decrypt(rsa_len,(unsigned char *)base64_data,(unsigned char*)decrypt_data,p_rsa,RSA_PKCS1_PADDING)<0)
        {
			printf("RSA_public_decrypt error!\n");
			ret = -1;
			goto end;
        }
    }	
	ret = 1;
	printf("[%s:%d] success!\n", __func__, __LINE__);
	
end:
	if(p_rsa)
		RSA_free(p_rsa);
	if(base64_data)
		free(base64_data);
	
	if(!ret)
	{
		free(decrypt_data);
		decrypt_data = NULL;
	}

	return decrypt_data;	
}


char *ysx_rsa_pri_encrypt(char *instr, char *path_key, int inlen)
{
	char *p_hex;
	RSA *p_rsa;
	FILE *file = NULL;
	int rsa_len , base64_len = 0 , encrypt_size = 0;
	int flen;
	char *encrypt_data;
	char *p, *base64_data = NULL;
	
    if((file=fopen(path_key,"r"))==NULL){
        printf("Key read error!\n");
        return NULL;
    }

    if((p_rsa=PEM_read_RSAPrivateKey(file,NULL,NULL,NULL))==NULL){
        printf("PEM read err!\n");
		fclose(file);		
        return NULL;
    }
    else    
    {
		fclose(file);		
    }

	flen=inlen;
    rsa_len=RSA_size(p_rsa);

    printf("[%s:%d] indata_len=%d\n", __func__, __LINE__, flen); 

	encrypt_data = (char *)malloc(1024*2);
	memset(encrypt_data, 0, 1024*2);

    if(flen > (rsa_len-11))
    {
	    int time = flen/(rsa_len-11);
	    int remain = flen%(rsa_len-11);
		int i;  

        for(i = 0; i < time; i++)
        {  
            p = instr+i*(rsa_len-11);

            encrypt_size = RSA_private_encrypt(rsa_len-11,(unsigned char *)p,(unsigned char*)(encrypt_data+i*rsa_len),p_rsa,RSA_PKCS1_PADDING);
			if(encrypt_size < 0)
            {
                printf("RSA_private_encrypt error!\n");
				goto finally;
            }
        }

        if(remain > 0)
        {
            p = instr+time*(rsa_len-11);

            encrypt_size = RSA_private_encrypt(remain,(unsigned char *)p,(unsigned char*)(encrypt_data+time*rsa_len),p_rsa,RSA_PKCS1_PADDING);
            if(encrypt_size<0)
            {
                printf("RSA_private_encrypt error!\n");
				goto finally;
			}
        }

        encrypt_data[rsa_len*time+encrypt_size] = '\0';
        base64_data = ipc_base64_encode(encrypt_data, time*rsa_len+encrypt_size, &base64_len);	
	}
	else    
    {
    	rsa_len=RSA_size(p_rsa);
  
        encrypt_size = RSA_private_encrypt(rsa_len-11,(unsigned char *)instr,(unsigned char*)encrypt_data,p_rsa,RSA_PKCS1_PADDING);
		if(encrypt_size < 0)
        {
			printf("RSA_private_encrypt error!\n");		
       		goto finally;
		}

        base64_data = ipc_base64_encode(encrypt_data, rsa_len, &base64_len);
	}
	
	printf("[%s:%d] success!\n", __func__, __LINE__);
	
finally:	
	if(p_rsa)
		RSA_free(p_rsa);
	if(encrypt_data)	
		free(encrypt_data);	
	
	return base64_data;	
}

