/* 
 * File:   reply_codes.h
 * Author: Humbedooh
 *
 * Created on January 5, 2011, 8:44 AM
 */

#ifndef REPLY_CODES_H
#define	REPLY_CODES_H

#ifdef	__cplusplus
extern "C" {
#endif
    const char* rumble_smtp_reply_code(unsigned int code) {
        switch(code) {
        case 200: 	return "200 OK\r\n";
        case 211: 	return "211 System status, or system help reply\r\n";
        case 214: 	return "214 Help message\r\n";
        case 220: 	return "220 <domain> Service ready\r\n";
        case 221: 	return "221 <domain> Service closing transmission channel\r\n";
        case 250: 	return "250 Requested mail action okay, completed\r\n";
        case 251:       return "251 User not local; will forward to <forward-path>\r\n";
        case 354: 	return "354 Start mail input; end with <CRLF>.<CRLF>\r\n";
        case 421: 	return "421 <domain> Service not available, closing transmission channel\r\n";
        case 450: 	return "450 Requested mail action not taken: mailbox unavailable\r\n";
        case 451: 	return "451 Requested action aborted: local error in processing\r\n";
        case 452: 	return "452 Requested action not taken: insufficient system storage\r\n";
        case 500: 	return "500 Syntax error, command unrecognized\r\n";
        case 501: 	return "501 Syntax error in parameters or arguments\r\n";
        case 502: 	return "502 Command not implemented\r\n";
        case 503: 	return "503 Bad sequence of commands\r\n";
        case 504: 	return "504 Command parameter not implemented\r\n";
        case 521: 	return "521 <domain> does not accept mail (see rfc1846)\r\n";
        case 530: 	return "530 Access denied\r\n";
        case 550: 	return "550 Requested action not taken: mailbox unavailable\r\n";
        case 551: 	return "551 User not local; please try <forward-path>\r\n";
        case 552: 	return "552 Requested mail action aborted: exceeded storage allocation\r\n";
        case 553: 	return "553 Requested action not taken: mailbox name not allowed\r\n";
        case 554: 	return "554 Transaction failed\r\n";
        default:        return "200 OK\r\n";
        }
    }

#ifdef	__cplusplus
}
#endif

#endif	/* REPLY_CODES_H */

