/*B-em 0.7 by Tom Walker*/
/*System VIA emulation*/

unsigned short pc;
#include <allegro.h>
#include <stdio.h>
#include "b-em.h"

void updatekeyboard();
unsigned char *ram;
void dumpram2()
{
        FILE *f=fopen("ram.dmp","wb");
        fwrite(ram,0x8000,1,f);
        fclose(f);
}

#define TIMER1INT 0x40
#define TIMER2INT 0x20
#define PORTBINT  0x18
#define PORTAINT  0x03

#define         ORB   0x00
#define         ORA     0x01
#define         DDRB    0x02
#define         DDRA    0x03
#define         T1CL    0x04
#define         T1CH    0x05
#define         T1LL    0x06
#define         T1LH    0x07
#define         T2CL    0x08
#define         T2CH    0x09
#define         SR         0x0a
#define         ACR     0x0b
#define         PCR     0x0c
#define         IFR     0x0d
#define         IER     0x0e
#define         ORAnh 0x0f

int model;
void updatesysIFR()
{
        if ((sysvia.ifr&0x7F)&(sysvia.ier&0x7F))
        {
                sysvia.ifr|=0x80;
                interrupt|=1;
        }
        else
        {
                sysvia.ifr&=~0x80;
                interrupt&=~1;
        }
}

int lns;
void updatesystimers()
{
        if (sysvia.t1c<-4)
        {
                sysvia.t1c+=sysvia.t1l+4;
//                printf("Timer 1 reset line %i %04X %04X\n",lns,sysvia.t1c,sysvia.t1l);
                if (!sysvia.t1hit)
                {
                       sysvia.ifr|=TIMER1INT;
                       updatesysIFR();
                }
                if (!(sysvia.acr&0x40))
                   sysvia.t1hit=1;
        }
        if (!(sysvia.acr&0x20)/* && !sysvia.t2hit*/)
        {
                if (sysvia.t2c<-4)
                {
                        sysvia.t2c+=sysvia.t2l+4;
                        if (!sysvia.t2hit)
                        {
                                sysvia.ifr|=TIMER2INT;
                                updatesysIFR();
                        }
                        sysvia.t2hit=1;
                }
        }
}

void vblankint()
{
        sysvia.ifr|=2;
        updatesysIFR();
//        printf("VBLANK %02X %i\n",sysvia.ier,p.i);
}

void syscb1()
{
        sysvia.ifr|=0x10;
        updatesysIFR();
}

unsigned char IC32=0;

int scrsize;
int keycol,keyrow;
int bbckey[16][16];
int keysdown=0;

unsigned char sdbval;

unsigned short cmosaddr;
int cmosena,cmosrw,cmosstrobe,cmosold;
unsigned char cmos[64];

void loadcmos()
{
        FILE *f=fopen("cmos.bin","rb");
        fread(cmos,64,1,f);
        fclose(f);
}

void savecmos()
{
        FILE *f=fopen("cmos.bin","wb");
        fwrite(cmos,64,1,f);
        fclose(f);
}

void writeIC32(unsigned char val)
{
        unsigned char oldIC32=IC32;
        if (val&8)
           IC32|=(1<<(val&7));
        else
           IC32&=~(1<<(val&7));
        scrsize=((IC32&16)?2:0)|((IC32&32)?1:0);
        if (!(IC32&8)&&(oldIC32&8))
           updatekeyboard();
        if (!(IC32&1))
           soundwrite(sdbval);
        if (model==7)
        {
                cmosrw=IC32&2;
                cmosstrobe=(IC32&4)^cmosold;
                cmosold=IC32&4;
                if (cmosstrobe && cmosena && !cmosrw && cmosaddr>0xB) cmos[cmosaddr]=sdbval; //printf("CMOS write %02X %02X\n",cmosaddr,sdbval);
                if (cmosena && cmosrw) { sysvia.ora=cmos[cmosaddr]; /*printf("CMOS read %02X %02X\n",cmosaddr,sysvia.ora);*/ }
        }
}

void writedatabus(unsigned char val)
{
        sdbval=val;
        if (!(IC32&8))
        {
                keyrow=(val>>4)&7;
                keycol=val&0xF;
                updatekeyboard();
        }
        if (!(IC32&1))
           soundwrite(val);
                if (model==7 && cmosstrobe && cmosena && !cmosrw && cmosaddr>0xB) cmos[cmosaddr]=sdbval;
//        if (model==7 && cmosstrobe && !cmosrw && cmosena) printf("CMOS write %02X %02X\n",cmosaddr,sdbval);
}

void writesysvia(unsigned short addr, unsigned char val, int line)
{
//        if (addr==0xFE40) printf("FE40 write %02X\n",val);
        switch (addr&0xF)
        {
                case ORA:
                sysvia.ifr&=0xfc;
                updatesysIFR();
//                printf("Port A clear\n");
                case ORAnh:
                sysvia.ora=val;
                sysvia.porta=(sysvia.porta & ~sysvia.ddra)|(sysvia.ora & sysvia.ddra);
                writedatabus(val);
                break;

                case ORB:
                sysvia.orb=val;
                sysvia.portb=(sysvia.portb & ~sysvia.ddrb)|(sysvia.orb & sysvia.ddrb);
                sysvia.ifr&=0xee;//~PORTBINT;
                writeIC32(val);
                updatesysIFR();
                if (model==7) /*CMOS*/
                {
                        if (val&0x80) cmosaddr=sysvia.ora&63;
                        cmosena=val&0x40;
//                        printf("CMOSena %i CMOSaddr %02X\n",cmosena,cmosaddr);
                }
                break;

                case DDRA:
                sysvia.ddra=val;
                break;
                case DDRB:
                sysvia.ddrb=val;
                break;
                case ACR:
                sysvia.acr=val;
                break;
                case PCR:
                sysvia.pcr=val;
                break;
                case T1LL:
                case T1CL:
                sysvia.t1l&=0x1FE00;
                sysvia.t1l|=(val<<1);
                break;
                case T1LH:
                sysvia.t1l&=0x1FE;
                sysvia.t1l|=(val<<9);
                if (sysvia.acr&0x40)
                {
                        sysvia.ifr&=~TIMER1INT;
                        updatesysIFR();
                }
                break;
                case T1CH:
                sysvia.t1l&=0x1FE;
                sysvia.t1l|=(val<<9);
                sysvia.t1c=sysvia.t1l;
                sysvia.ifr&=~TIMER1INT;
                updatesysIFR();
                sysvia.t1hit=0;
                break;
                case T2CL:
                sysvia.t2l&=0x1FE00;
                sysvia.t2l|=(val<<1);
                break;
                case T2CH:
                sysvia.t2l&=0x1FE;
                sysvia.t2l|=(val<<9);
                sysvia.t2c=sysvia.t2l;
                sysvia.ifr&=~TIMER2INT;
                updatesysIFR();
                sysvia.t2hit=0;
                break;
                case IER:
//                printf("IER was %02X write %02X %04X\n",sysvia.ier,val,pc);
                if (val&0x80)
                   sysvia.ier|=(val&0x7F);
                else
                   sysvia.ier&=~(val&0x7F);
                updatesysIFR();
//                printf("now %02X line %i\n",sysvia.ier,line);
                break;
                case IFR:
//                printf("Interrupt clear %02X\n",val&0x7F);
                sysvia.ifr&=~(val&0x7F);
                updatesysIFR();
                break;
        }
}

unsigned char readsysvia(unsigned short addr)
{
        unsigned char temp;
        switch (addr&0xF)
        {
                case ORA:
//                printf("Port A clear\n");
                sysvia.ifr&=~PORTAINT;
                updatesysIFR();
                case ORAnh:
                if (model==7 && cmosrw && cmosena)
                {
                        sysvia.ora=cmos[cmosaddr];
//                        printf("CMOS read %02X %02X\n",cmosaddr,sysvia.ora);
                        temp=sysvia.ora & ~sysvia.ddra;
                        return temp;
                }
                temp=sysvia.ora & sysvia.ddra;
                temp|=(sysvia.porta & ~sysvia.ddra);
                if (model<7 || !cmosena)
                {
                        temp&=0x7F;
                        if (bbckey[keycol][keyrow])
                        {
//                                printf("Key col %i row %i down\n",keycol,keyrow);
                                return temp|0x80;
                        }
                }
                return temp;

                case ORB:
                sysvia.ifr&=0xEF;//~PORTBINT;
                updatesysIFR();
                temp=sysvia.orb & sysvia.ddrb;
                if (sysvia.acr&2)
                   temp|=(sysvia.irb & ~sysvia.ddrb);
                else
                   temp|=(sysvia.portb & ~sysvia.ddrb);
                temp|=0xF0;
                if (joy[0].button[0].b) temp&=~0x10;
                if (joy[1].button[0].b) temp&=~0x20;
                return temp;

                case DDRA:
                return sysvia.ddra;
                case DDRB:
                return sysvia.ddrb;
                case T1LL:
                return (sysvia.t1l&0x1FE)>>1;
                case T1LH:
                return sysvia.t1l>>9;
                case T1CL:
                sysvia.ifr&=~TIMER1INT;
                updatesysIFR();
                if (sysvia.t1c<0) return 0xFF;
                return (sysvia.t1c>>1)&0xFF;
                case T1CH:
                if (sysvia.t1c<0) return 0xFF;
                return (sysvia.t1c>>1)>>8;
                case T2CL:
                sysvia.ifr&=~TIMER2INT;
                updatesysIFR();
                if (sysvia.t2c<0) return 0xFF;
                return (sysvia.t2c>>1)&0xFF;
                case T2CH:
                if (sysvia.t2c<0) return 0xFF;
                return (sysvia.t2c>>1)>>8;
                case ACR:
                return sysvia.acr;
                case PCR:
                return sysvia.pcr;
                case IER:
                return sysvia.ier|0x80;
                case IFR:
                return sysvia.ifr;
        }
}

void resetsysvia()
{
        sysvia.ifr=sysvia.ier=0;
        sysvia.t1c=sysvia.t1l=0x1FFFE;
        sysvia.t2c=sysvia.t2l=0x1FFFE;
        sysvia.t1hit=sysvia.t2hit=0;
}

/*Keyboard*/
#include "scan2bbc.h"

unsigned char codeconvert[128]=
{
        0,30,48,46,32,18,33,34,
        35,23,36,37,38,50,49,24,
        25,16,19,31,20,22,47,17,
        45,21,44,11,2,3,4,5,
        6,7,8,9,10,82,79,80,
        81,75,76,77,71,72,73,59,
        60,61,62,63,64,65,66,67,
        68,87,88,1,41,12,13,14,
        15,26,27,28,39,40,43,86,
        51,52,53,57,81,83,0,79,
        0,0,75,77,72,80,0,55,
        74,78,83,0,84,0,115,125,
        112,121,123,42,54,29,0,56,
        0,91,92,93,70,69,58,0,
        0,0,0,0,0,0,58,0,
        0,0,0,0,0,0,0,0,
};

inline void presskey(int row, int col)
{
        bbckey[col][row]=1;
        keysdown++;
        updatekeyboard();
//        printf("Key pressed %01X %01X %02X %02X\n",row,col,sysvia.ifr,sysvia.ier);
}

inline void releasekey(int row, int col)
{
        bbckey[col][row]=0;
        keysdown--;
}

static inline int TranslateKey(int index, int *row, int *col)
{
        unsigned int    vkey=scan2bbc[index & 127];

        if (vkey==0xaa) return(-1);

        col[0] = vkey&15;
        row[0] = (vkey>>4)&15;

        return(row[0]);
}

int keys2[128];

void checkkeys()
{
        int c,cc;
        int row,col;
        for (c=0;c<128;c++)
        {
                if (key[c]!=keys2[c] && c!=KEY_F11)
                {
                        if (TranslateKey(codeconvert[c],&row,&col)>0)
                        {
                                if (key[c])
                                   presskey(row,col);
                                else
                                   releasekey(row,col);
                        }
                }
        }
        if (key[KEY_RSHIFT]||key[KEY_LSHIFT])
           presskey(0,0);
        else
           releasekey(0,0);
        if (key[KEY_LCONTROL]||key[KEY_RCONTROL])
           presskey(0,1);
        else
           releasekey(0,1);
        if (key[KEY_CAPSLOCK])
           presskey(4,0);
        else
           releasekey(4,0);
        for (c=0;c<128;c++)
            keys2[c]=key[c];
}

void updatekeyboard()
{
        int c;
        if (keysdown && (sysvia.pcr&0xC)==4)
        {
                sysvia.ifr&=~1;
                if (IC32&8)
                   sysvia.ifr|=1;
                else
                {
                        if (keycol<10)
                        {
                                for (c=1;c<8;c++)
                                {
                                        if (bbckey[keycol][c])
                                        {
                                                sysvia.ifr|=1;
//                                                printf("Key %01X %01X %02X\n",keycol,c,sysvia.ier);
                                        }
                                }
                        }
                }
//                gotoxy(0,23);
//                printf("IFR %i %i %i\n",sysvia.ifr&1,keysdown,keycol);
                updatesysIFR();
        }
}

void initDIPS(unsigned char dips)
{
        int c;
        for (c=9;c>=2;c--)
        {
                if (dips&1)
                   presskey(0,c);
                else
                   releasekey(0,c);
                dips>>=1;
        }
        keysdown=0;
}
