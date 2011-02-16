/* File: reply_codes.h Author: Humbedooh Created on January 5, 2011, 8:44 AM */
#ifndef REPLY_CODES_H
#   define REPLY_CODES_H
#   ifdef __cplusplus
extern "C"
{
#   endif
const char  *rumble_smtp_reply_code(unsigned int code);
const char  *rumble_pop3_reply_code(unsigned int code);
#   ifdef __cplusplus
}
#   endif
#endif /* REPLY_CODES_H */
