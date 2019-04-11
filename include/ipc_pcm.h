/*
 * ipc_pcm.h
 *
 *  Created on: August 29, 2018
 *      Author: yuanhao
 */

#ifndef __IPC_PCM_H_
#define __IPC_PCM_H_

typedef enum
{
    G711_A_LAW     = 0,          // pcma
    G711_U_LAW     = 1,          // pcmu
}G711_MODE;

/*****************************************************************************
 * 功能描述：	PCM转G711
 * 输入参数：	a_psrc:输入buf; a_pdst:输出buf; in_data_len:输入buf长度; type:G711模式
 * 返回值：	0：成功；小于0：失败
*****************************************************************************/
int ipc_g711_encode(char *a_psrc, char *a_pdst, int in_data_len, unsigned char type);

/*****************************************************************************
 * 功能描述：	G711转PCM
 * 输入参数：	a_psrc:输入buf; a_pdst:输出buf; in_data_len:输入buf长度; type:G711模式
 * 返回值：	0：成功；小于0：失败
*****************************************************************************/
int ipc_g711_decode(char *a_psrc, char *a_pdst, int in_data_len, unsigned char type);

#endif /*_PCM_H_ */

