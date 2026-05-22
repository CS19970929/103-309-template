#ifndef APP_INIT_H
#define APP_INIT_H

void AppInit_Boot(void);

#define AppInit_InitSci() InitUSART_CommonUpper()
#define AppInit_ServiceSci() App_CommonUpper()

#endif
