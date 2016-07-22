#ifndef SPI_H
#define SPI_H

class SPI
{
public:

    SPI();

    unsigned char sendRecv(unsigned char data = 0);

    void enable();

    void disable();
};

#endif // SPI_H
