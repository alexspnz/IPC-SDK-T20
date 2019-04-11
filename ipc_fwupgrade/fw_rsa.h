#ifndef FW_RSA_H_
#define FW_RSA_H_

#include "fw_download.h"

char *base64_decode(const char *data, int data_len,int *len);
char *base64_encode(const char* data, int data_len,int *len);

char *ysx_rsa_pub_encrypt(char *instr, char *path_key, int inlen);
char *ysx_rsa_pri_decrypt(char *instr, char *path_key);
char *ysx_rsa_pub_decrypt(char *instr, char *path_key);
char *ysx_rsa_pri_encrypt(char *instr, char *path_key, int inlen);
void ysx_aes_cbc_encrypt(unsigned char* in, int inl, unsigned char *out, int* len, char * key);
void ysx_aes_cbc_decrypt(unsigned char* in, int inl, unsigned char *out, unsigned char *key);


#endif /* FW_RSA_H_ */

