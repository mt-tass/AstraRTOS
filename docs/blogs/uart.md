# UART Protocol Driver

# UART Driver

UART stands for Universal Asynchronous Receiver Transmitter which is an asynchronous communication protocol which has a RX and a TX on both ends (considering full-duplex). It does not share a clock line, and so both the sides agree on a baud rate for communication. You can make use of this protocol to communicate with different peripherals and also use it as a debug console connecting with the host.

On the STM32F429ZI, the UART peripheral is called USART (S - synchronous, optional clock line for synchronous communication). This driver initializes the USART2 peripheral for asynchronous communication over a baud rate of 9600 over PA2 (TX) and PA3 (RX).

## Initializing UART

These 4 things must be configured before sending anything over USART2.

1. Start GPIO and USART2 clocks.
2. Configure PA2 and PA3 to Alternate Function mode and select USART2.
3. Configure the baud rate and UART frame format.
4. Enable USART2, TX and RX.

### Start GPIO and USART2 clocks.

These are functions written in rcc.c:

```c
rcc_enable_gpio(GPIOA_EN); //Sets bit 0 of RCC_AHB1ENR
rcc_enable_uart(USART2_EN); //Sets bit 17 of RCC_APB1ENR
```

This basically powers up those peripherals.

### Configure PA2 and PA3 to Alternate Function mode and select USART2.

A GPIO pin can be configured to be a plain input pin, plain output pin, an analog input or can be routed to on of the 16 alternate functions (AF0 - AF15). AF7 on PA2 and PA3 is USART2.

- Set pin to AF mode
    
    GPIOA_MODER is a 32 bit register with 2 bits per pin. Where PA2 occupies [5:4] and PA3 [7:6]. The encoding for a particular pin to be set in a particular mode is as follows:
    
    - 00 = Input
    - 01 = Output
    - 10 = Alternate Function
    - 11 = Analog
    
    For us to set the pins to a particular mode, we would first clear and then set the bits.
    
    ```c
    GPIOA_MODER &= ~((3 << (2*2)) | (3 << (3*2)));
    
    /*
    (3 << (2*2)) = 0b 0000 0000 0000 0000 0000 0000 0011 0000
    (3 << (3*2)) = 0b 0000 0000 0000 0000 0000 0000 1100 0000
    
    OR operation = 0b 0000 0000 0000 0000 0000 0000 1111 0000
    Inverted     = 0b 1111 1111 1111 1111 1111 1111 0000 1111
    
    If,
    Current reg  = 0b 1010 1010 1010 1010 1010 1010 1111 1010
    Inverted     = 0b 1111 1111 1111 1111 1111 1111 0000 1111
    
    AND result   = 0b 1010 1010 1010 1010 1010 1010 0000 1010
    
    Clears bits 4, 5, 6, 7
    */
    ```
    
    Now for setting to AF mode,
    
    ```c
    GPIOA_MODER |= ((2 << (2*2)) | (2 << (3*2)));
    
    /*
    (2 << (2*2)) = 0b 0000 0000 0000 0000 0000 0000 0010 0000
    (2 << (3*2)) = 0b 0000 0000 0000 0000 0000 0000 1000 0000
    
    OR operation = 0b 0000 0000 0000 0000 0000 0000 1010 0000
    
    Now after clearing,
    Current reg  = 0b 1010 1010 1010 1010 1010 1010 0000 1010
    OR till now  = 0b 0000 0000 0000 0000 0000 0000 1010 0000
    
    OR operation = 0b 1010 1010 1010 1010 1010 1010 1010 1010
    
    Pins PA2 and PA3 set to AF mode
    */
    ```
    
- Select particular AF mode
    
    Each GPIO pin has a 4 bit alternate function selector. Pins 0-7 are in AFRL (Alternate Function Register Low) and pins 8-15 are in AFRH. We use AFRL since, PA2 and PA3 are our target pins. Each field is 4 bits wide so, PA2 occupies [11:8] and PA3 [15:12].
    
    ```c
    GPIOA_AFRL &= ~((0xF << (2*4)) | (0xF << (3*4))); //Clear 
    GPIOA_AFRL |= ((7 << (2*4)) | (7 << (3*4))); //Selecting AF to USART2
    ```
    
    Similar logic here too, clear then set.
    

### Configure the baud rate and UART frame format.

The Baud Rate Register (BRR) is a 16 bit register which holds a fixed-point number called USARTDIV. This register is divided into 2 fields:

1. Mantissa [15:4]
2. Fraction [3:0]

```c
USARTDIV = pclk/(16 * baud)

/* Here, for 45 MHz and 9600 Baud rate
USARTDIV = 45,000,000 / (16 × 9600) = 292.96875

Mantissa = 292
Fraction = 0.96875 × 16 = 15.5 (round to 16)

Since, fraction rounds to 16, which overflows the 4 bit field (max is 15), carry it to mantissa.

Mantissa = 292 + 1 = 293 = 0x125
Fraction = 0

BRR = (mantissa << 4) | (fraction & 0xF)
BRR = (0x125 << 4) | 0x0 = 0x1250 */
```

Here, we are hard-coded the frame format to be 8N1 (8 data bits, no parity, 1 stop bit). We make use of Control Registers USART2_CR1 and USART2_CR2 for this.

```c
USART2_CR1 &= ~(1 << 12); //Clearing Bit 12 to ensure 8 bit
USART2_CR2 &= ~(3 << 12); //Clears both the stop bits [13:12]
USART2_CR1 &= ~(1 << 10); //Clearing Bit 10 to disable parity
```

### Enable USART2, TX and RX.

Now, we enable USART (Bit 13), TX (Bit 3) and RX (Bit 2) with the help of those Control Registers.

```c
USART2_CR1 |= (1 << 13) | (1 << 3) | (1 << 2);
```

## UART Functions

### Transmitting a Byte

The following function sends a single 8 bit character over the TX pin.

```c
void uart_send_char(char c){
    while(!(USART2_SR & (1 << 7)));//Checking bit 7 of SR
    USART2_DR = (uint8_t)c; //Write to DR
}
```

Bit 7 of Status Register is TXE (Transmit data register Empty). When TXE is 1, the data register has been moved to the shift register and is ready for the next byte. Spin here until true and then write the byte. Writing to USART2_DR automatically clears TXE and starts the hardware shifting the byte out at the configured baud rate.

### Sending a String

The following function just uses the uart_send_char() function to achieve sending a string.

```c
void uart_send_string(char *str){
    while(*str){
        uart_send_char(*str++);
    }
}
```

### Receiving a Byte

The following function catches data bit by bit and drops completed 8 bit character into USART2_DR.

```c
int uart_receive_char(void){
    int timeout = 0; //Internal timeout of function
    
    while(!(USART2_SR & (1 << 5))){ //Checking bit 5 of SR
        timeout++;
        if(timeout >= TIMEOUT) return -1; //Timeout if nothing is received
    }
    
    return (int)(USART2_DR & 0xFF);
}
```

Bit 5 of Status Register is RXNE (Receive data register Not Empty). When a complete byte has arrived the hardware sets this bit and puts the received byte in USART2_DR. Spin until it is set.

The timeout is to ensure that CPU does not hang forever.TIMEOUT is define in /drivers/include/uart.h

### Receiving a String

The following function takes single characters from uart_receive_char() function and stitches them together into a complete string, safely storing them in a buffer.

```c
void uart_receive_string(char *buffer, int max_length){
    int i = 0;
    int c;
    
    while(i < max_length - 1){ //Iterate until (max - 1)
        c = uart_receive_char(); //Receive characters
        
        if(c == -1) break; //Break if timeout
        if(c == '\r' || c == '\n') break; //Break if carriage return or newline
        
        buffer[i++] = (char)c; //Put in buffer
    }
    
    buffer[i] = '\0'; //Null terminator
}
```

The loop runs until one of the three things happen,

- Buffer is almost filled
- Timeout
- Newline or Carriage return

It puts a null terminator at the end, regardless of how the loop ends.