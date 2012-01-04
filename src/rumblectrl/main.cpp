/* 
 * File:   main.cpp
 * Author: vaps
 *
 * Created on 4. januar 2012, 17:30
 */

#include <cstdlib>
#include "../radb/radb.h"
#include "../radb/radb.cpp"

// SHA256 Implementation:
// jagatsastry.nitk@gmail.com 9th April  '09

#include<iostream>
#include<vector>
#include<fstream>
#include<exception>
#include<string>
using namespace std;

typedef unsigned int uint;


string fromDecimal(uint n, int b)
{
	string chars="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	string result="";
	while(n>0)
	{
		result=chars.at(n%b)+result;
		n/=b;
	}

	return result;
}

	uint K[]=
	{   0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
   0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
   0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
   0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
   0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
   0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
   0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
   0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
   
    void makeblock(vector<uint>& ret, string p_msg)
	{
		uint cur=0;
		int ind=0;
		for(uint i=0; i<p_msg.size(); i++)
		{
			cur = (cur<<8) | (unsigned char)p_msg[i];
			  
			if(i%4==3)
			{
				ret.at(ind++)=cur;
				cur=0;
			}
		}
	}
   
class Block
{
	public:
	vector<uint> msg;
	
	Block():msg(16, 0) { }

			
	Block(string p_msg):msg(16, 0)
	{ 
		makeblock(msg, p_msg);
	}
	
};
	

void split(vector<Block>& blks, string& msg)
{
	for(uint i=0; i<msg.size(); i+=64)
	{
           
try
{
		 makeblock(blks[i/64].msg, msg.substr(i, 64));
      }
      catch(...)
      {

                         }
	}
}
	

string mynum(uint x)
{
 string ret;
  for(uint i=0; i<4; i++)
  ret+=char(0);
  
  for(uint i=4; i>=1; i--)	//big endian machine assumed
  {
          ret += ((char*)(&x))[i-1];
  }
  return ret;
}
  

#define shr(x,n) ((x & 0xFFFFFFFF) >> n)
#define rotr(x,n) (shr(x,n) | (x << (32 - n)))

uint ch(uint x, uint y, uint z)
{
 return (x&y) ^ (~x&z);
}

uint maj(uint x, uint y, uint z)
{
 return (x&y) ^ (y&z) ^ (z&x);
}

uint fn0(uint x)
{
 return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

uint fn1(uint x)
{
 return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

uint sigma0(uint x)
{
	return rotr(x, 7) ^ rotr(x, 18) ^ shr(x, 3);
}

uint sigma1(uint x)
{
	return rotr(x, 17) ^ rotr(x, 19) ^ shr(x, 10);
}




 

string SHA256(const char* input)
{
	string msg_arr, msg, output;
	msg_arr = input;
	msg=msg_arr;
	msg_arr += (char)(1<<7);
	uint cur_len = msg.size()*8 + 8;
	uint reqd_len = ((msg.size()*8)/512+1) *512;
	uint pad_len = reqd_len - cur_len - 64;
	
	string pad(pad_len/8, char(0));
	msg_arr += pad;
	string len_str(mynum(msg.size()*8));
	msg_arr = msg_arr + len_str;
	
	uint num_blk = msg_arr.size()*8/512;
	vector<Block> M(num_blk, Block());
	split(M, msg_arr);
	
		uint H[]={0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f,  	0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
		
		for(uint i=0; i<num_blk; i++)
		{
			vector<uint> W(64, 0);
			for(uint t=0; t<16; t++)
			{
				W[t] = M[i].msg[t];
			}
			
			
			for(uint t=16; t<64; t++)
			{
				W[t] = sigma1(W[t-2]) + W[t-7] + sigma0(W[t-15]) + W[t-16];
			}
			
			uint work[8];
			for(uint i=0; i<8; i++)
			work[i] = H[i];
                
			for(uint t=0; t<64; t++)
			{

				uint t1, t2;
				t1 = work[7] + fn1(work[4]) + ch(work[4], work[5], work[6]) + K[t] + W[t];
				t2 = fn0(work[0]) + maj(work[0], work[1], work[2]);
				work[7] = work[6];
				work[6] = work[5];
				work[5] = work[4];
				work[4] = work[3] + t1; 
				work[3] = work[2]; 
				work[2] = work[1];
				work[1] = work[0];
				work[0] = t1 + t2;
	
			}
			
			for(uint i=0; i<8; i++)
			{
			H[i] = work[i] + H[i];
            }
		}
		
		 for(uint i=0; i<8; i++)
		 output = output + fromDecimal(H[i], 16);

		 return output;
}

#define A_ADD   1
#define A_DEL   2
#define A_LIST  3
#define A_LISTU 4
#define A_USER  5
#define A_UADD  6
#define A_UDEL  7
#define A_UEDIT 8

char *domain = 0, email = 0, password = 0, uName[512], uDomain[512], uPass[512], uType[512], uArgs[512];
int needHelp = 0;
/*
 * 
 */
int main(int argc, char** argv) {
    int i;
    int action = 0;
    radbResult* result;
    radb* db = new radb();
    radbo* dbo;
    if (argc <= 1) needHelp = 1;
    memset(uName, 0, 512);
    memset(uDomain, 0, 512);
    memset(uPass, 0, 512);
    memset(uType, 0, 512);
    memset(uArgs, 0, 512);
    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) needHelp = 1;
        if (!strcmp(argv[i], "-h")) needHelp = 1;
        if (!strcmp(argv[i], "--add")) action = A_ADD;
        if (!strcmp(argv[i], "--delete")) action = A_DEL;
        if (!strcmp(argv[i], "--list")) action = A_LIST;
        if (!strcmp(argv[i], "--listusers")) action = A_LISTU;
        if (!strcmp(argv[i], "--userinfo")) action = A_USER;
        if (!strcmp(argv[i], "--adduser")) action = A_UADD;
        if (!strcmp(argv[i], "--deleteuser")) action = A_UDEL;
        if (!strcmp(argv[i], "--edituser")) action = A_UEDIT;
        if (strstr(argv[i], "--email=")) sscanf(argv[i], "--email=%250[^@ ]@%250c", uName, uDomain);
        if (strstr(argv[i], "--domain=")) sscanf(argv[i], "--domain=%250c", uDomain);
        if (strstr(argv[i], "--pass=")) sscanf(argv[i], "--pass=%250c", uPass);
        if (strstr(argv[i], "--type=")) sscanf(argv[i], "--type=%250c", uType);
        if (strstr(argv[i], "--args=")) sscanf(argv[i], "--args=%250c", uArgs);
        
    }
    
    if (!needHelp) {
        
        db->init_sqlite("db/rumble.sqlite");
        switch(action) {
            case A_ADD:
                if (!strlen(uDomain)) { printf("Error: Invalid domain name specified!\n"); needHelp = 1; break;}
                else {
                        dbo = db->prepare("SELECT `id` FROM `domains` WHERE domain = %s LIMIT 1", uDomain);
                        if (dbo->fetch_row()) {
                        printf("Error: Domain %s already exists\n", uDomain);
                        exit(EXIT_FAILURE);
                        }
                    db->run_inject("INSERT INTO `domains` (id, domain, storagepath, flags) VALUES (NULL, %s, \"\", 0)", uDomain);
                }
                
                break;
            case A_DEL:
                if (!strlen(uDomain)) { printf("Invalid domain name specified!\n"); needHelp = 1; break; }
                db->run_inject("DELETE FROM `domains` WHERE domain = %s", uDomain);
                break;
            case A_LIST:
                dbo = db->prepare("SELECT `id`, `domain` FROM `domains` WHERE 1");
                while ((result = dbo->fetch_row())) {
                    printf("%02u: %s\n", result->column[0].data.int32, result->column[1].data.string);
                }
                break;
            case A_LISTU:
                if (!strlen(uDomain)) { printf("Error: Invalid domain name specified!\n"); needHelp = 1; break; }
                dbo = db->prepare("SELECT `id`, `user`, `type` FROM `accounts` WHERE domain = %s", uDomain);
                while ((result = dbo->fetch_row())) {
                    sprintf(uName, "%s@%s",result->column[1].data.string, uDomain);
                    printf("%02u: %-32s  %s\n", result->column[0].data.int32, uName, result->column[2].data.string);
                }
                break;
            case A_USER:
                if (!strlen(uDomain)) { printf("Error: Invalid domain name specified!\n"); needHelp = 1; break; }
                if (!strlen(uName)) { printf("Error: Invalid user name specified!\n"); needHelp = 1; break;}
                dbo = db->prepare("SELECT `id`, `user`, `type` FROM `accounts` WHERE domain = %s AND user = %s", uDomain, uName);
                result = dbo->fetch_row();
                if (result) {
                    printf("%02u: %s@%s  %s\n", result->column[0].data.int32, uName, uDomain, result->column[2].data.string);
                    exit(EXIT_SUCCESS);
                }
                else {
                    printf("Error: No such user, %s@%s\n", uName, uDomain);
                    exit(EXIT_FAILURE);
                }
                break;
            case A_UADD:
                if (!strlen(uDomain)) { printf("Error: Invalid domain name specified!\n"); needHelp = 1; break;}
                if (!strlen(uName)) { printf("Error: Invalid user name specified!\n"); needHelp = 1; break;}
                if (!strlen(uPass)) { printf("Error: Invalid password or type specified!\n"); needHelp = 1; break;}
                dbo = db->prepare("SELECT `id` FROM `domains` WHERE domain = %s LIMIT 1", uDomain);
                if (!dbo->fetch_row()) { printf("Error: Invalid domain name specified!\n"); needHelp = 1; break; }
                if (!strlen(uType)) sprintf(uType, "mbox");
                db->run_inject("INSERT INTO `accounts` (domain, user, password, type, arg) VALUES (%s,%s,%s,%s, %s)", uDomain, uName, SHA256(uPass).c_str(), uType, uArgs);
                break;
            case A_UEDIT:
                if (!strlen(uDomain)) { printf("Error: Invalid domain name specified!\n"); needHelp = 1; break;}
                if (!strlen(uName)) { printf("Error: Invalid user name specified!\n"); needHelp = 1; break;}
                if (!strlen(uPass)) { printf("Error: Invalid password or type specified!\n"); needHelp = 1; break;}
                dbo = db->prepare("SELECT `id` FROM `domains` WHERE domain = %s LIMIT 1", uDomain);
                if (!dbo->fetch_row()) { printf("Error: Invalid domain name specified!\n"); needHelp = 1; break; }
                if (!strlen(uType)) sprintf(uType, "mbox");
                db->run_inject("UPDATE `accounts` SET password = %s, type = %s, arg = %s WHERE domain = %s AND user = %s", SHA256(uPass).c_str(), uType, uArgs, uDomain, uName);
                break;
            case A_UDEL:
                if (!strlen(uDomain)) { printf("Error: Invalid domain name specified!\n"); needHelp = 1; }
                if (!strlen(uName)) { printf("Error: Invalid user name specified!\n"); needHelp = 1; }
                db->run_inject("DELETE FROM `accounts` WHERE domain = %s AND user = %s", uDomain, uName);
                break;
            default:
                needHelp = 1;
                break;
        }
        if (!needHelp) { printf("Done!\n"); exit(EXIT_SUCCESS); }
    
    }
        
    
    
    if (needHelp) {
        printf("\
Usage: rumblectrl [action [parameters]]\r\n\
Available actions:\r\n\
 Domain actions:\r\n\
  --add --domain=<domain>                     : Adds <domain> to the server\r\n\
  --delete --domain=<domain>                  : Deletes <domain> from the server\r\n\
  --list                                      : Lists available domains\r\n\
 Account actions:\r\n\
  --listusers --domain=<domain>               : Lists users on this <domain>\r\n\
  --userinfo --email=<email>                  : Displays user information\r\n\
  --adduser --email=<email> --pass=<password> [--type=<type>] [--args=<args>]\r\n\
                                              : Creates a new user account\r\n\
  --edituser --email=<email> --pass=<password> [--type=<type>] [--args=<args>]\r\n\
                                              : Updates user account\r\n\
  --deleteuser --email=<email]                : Deletes a user account\r\n\
  \r\n\
Example: rumblectrl --adduser --email=some@thing.org --pass=Hello!\r\n\
");
        exit(EXIT_FAILURE);
    }
    
    
    delete db;
    return 0;
}

