#ifndef INC_AT24C08_H
#define INC_AT24C08_H

typedef union _parser
{
	unsigned char byte[4];
	float f;
}Parser;

void AT24C08_Page_Write(unsigned char page, unsigned char* data, unsigned char len);
void AT24C08_Page_Read(unsigned char page, unsigned char* data, unsigned char len);
void EP_PIDGain_Write(unsigned char id, float PGain, float IGain, float DGain);
unsigned char EP_PIDGain_Read(unsigned char id, float * PGain, float* IGain, float* DGain);

#endif /* INC_AT24C08_H */
