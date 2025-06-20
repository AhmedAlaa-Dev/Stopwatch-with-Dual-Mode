#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

void display_time(volatile unsigned char, volatile unsigned char, volatile unsigned char);

// Initialize the timer / also for ISR to use them as variables
volatile unsigned char hours = 0;
volatile unsigned char minutes = 0;
volatile unsigned char seconds = 0;

// Define flag of pushbutton state
volatile unsigned char TimerState_pushbutton_flag = 0;
volatile unsigned char TimerState_flag = 0; // 0-Increment 1-Decrement

// Function Prototypes
void INT0_INIT(void);
void init_timer1(void);
void INT1_INIT(void);
void INT2_INIT(void);

// Define flags as global variables
unsigned char Sec_Decrement_flag = 0;
unsigned char Sec_Increment_flag = 0;
unsigned char Min_Decrement_flag = 0;
unsigned char Min_Increment_flag = 0;
unsigned char H_Decrement_flag = 0;
unsigned char H_Increment_flag = 0;

//--------------------------------------INTERRUPT ROUTINES-----------------------------------------

ISR(INT0_vect) {
    hours = 0;
    minutes = 0;
    seconds = 0;
}

ISR(TIMER1_COMPA_vect) {
    if (TimerState_flag == 0) {
        seconds++;
        if (seconds == 60) {
            seconds = 0;
            minutes++;
        }
        if (minutes == 60) {
            minutes = 0;
            hours++;
        }
        if (hours == 100) {
            hours = 0;
        }
    } else if (TimerState_flag == 1) {
        if (seconds > 0) {
            seconds--;  // as long as seconds are greater than 0;
        } else if (minutes > 0 || hours > 0) {
            seconds = 59;  // Wrap seconds to 59 when decrementing from 0
            if (minutes > 0) {
                minutes--;
            } else if (hours > 0) {
                hours--;
                minutes = 59;  // Borrow from hours if needed
            }
        }
    }
}

ISR(INT1_vect) {
    TCCR1B &= ~(1<<CS12) & ~(1<<CS11) & ~(1<<CS10);
}

ISR(INT2_vect) {
    TCCR1B |= (1<<CS12) | (1<<CS10);
}

//--------------------------------------INITIALIZATION---------------------------------------------

void INT0_INIT(void) {
    DDRD &= ~(1<<2);  // INT0 pin as input
    PORTD |= (1<<2);  // Enable internal pull-up

    SREG |= (1<<7);
    GICR |= (1<<INT0);  // Enable external interrupt INT0
    MCUCR &= ~(1<<ISC00);  // Configure for falling edge trigger
    MCUCR |= (1<<ISC01);
}

void init_timer1(void) {
    TCNT1 = 0;
    TCCR1B |= (1 << WGM12);  // CTC mode (Clear Timer on Compare)
    TCCR1B |= (1 << CS12) | (1 << CS10);  // Prescaler of 1024
    TCCR1B &= ~(1<<CS11);
    OCR1A = 15625;  // Compare match value for 1 second (16 MHz / 1024 = 15625)
    TIMSK |= (1 << OCIE1A);  // Enable Timer1 Compare A Match interrupt
    SREG |= (1<<7);  // Enable global interrupts
}

void INT1_INIT(void) {
    DDRD &= ~(1<<3);  // INT1 pin as input
    MCUCR |= (1<<ISC11) | (1<<ISC10);  // Configure for rising edge trigger
    GICR |= (1<<INT1);  // Enable external interrupt INT1
    SREG |= (1<<7);  // Enable global interrupts
}

void INT2_INIT(void) {
    DDRB &= ~(1<<2);  // INT2 pin as input
    PORTB |= (1<<2);  // Enable internal pull-up
    MCUCSR &= ~(1<<ISC2);  // Falling edge trigger
    GICR |= (1<<INT2);  // Enable external interrupt INT2
    SREG |= (1<<7);  // Enable global interrupts
}

//--------------------------------------MAIN PROGRAM-----------------------------------------------

int main(void) {
    // Configure seven-segment display pins
    DDRC |= 0x0F;  // First 4 bits are output (0000 1111)
    PORTC &= 0xF0; // Set PC0, PC1, PC2, PC3 to 0

    DDRA |= 0x3F;  // All bits of port A are output (0011 1111)

    // Configure buzzer and LEDs
    DDRD |= (1<<0);  // Buzzer output
    PORTD &= ~(1<<0);  // Initially off

    DDRD |= (1<<4);  // Counting up RED LED output
    PORTD |= (1<<4);  // Initially on

    DDRD |= (1<<5);  // Counting down YELLOW LED output
    PORTD &= ~(1<<5);  // Initially off

    // Configure buttons
    DDRB &= ~(1<<5);  // Seconds Decrement
    PORTB |= (1<<5);  // Internal pull-up

    DDRB &= ~(1<<6);  // Seconds Increment
    PORTB |= (1<<6);  // Internal pull-up

    DDRB &= ~(1<<3);  // Minutes Decrement
    PORTB |= (1<<3);  // Internal pull-up

    DDRB &= ~(1<<4);  // Minutes Increment
    PORTB |= (1<<4);  // Internal pull-up

    DDRB &= ~(1<<0);  // Hours Decrement
    PORTB |= (1<<0);  // Internal pull-up

    DDRB &= ~(1<<1);  // Hours Increment
    PORTB |= (1<<1);  // Internal pull-up

    DDRB &= ~(1<<7);  // Timer state pushbutton
    PORTB |= (1<<7);  // Internal pull-up

    // Initialize interrupts
    INT0_INIT();  // Reset Timer
    init_timer1();  // Timer1 with prescaler of 1024
    INT1_INIT();  // Pause Timer
    INT2_INIT();  // Resume Timer

    while (1) {
        display_time(hours, minutes, seconds);

        // Buzzer activation condition
        if (hours == 0 && minutes == 0 && seconds == 0 && TimerState_flag == 1) {
            PORTD |= (1<<0);  // Activate buzzer
        } else {
            PORTD &= ~(1<<0);
        }

        //--------------------Time Adjustment--------------------

        // Seconds Increment
        if (!(PINB & (1<<6))) {
            _delay_ms(30);
            if (!(PINB & (1<<6)) && Sec_Increment_flag == 0) {
                seconds++;
                if (seconds == 60) {
                    seconds = 0;
                }
                Sec_Increment_flag = 1;
            }
        } else {
            Sec_Increment_flag = 0;
        }

        // Seconds Decrement
        if (!(PINB & (1<<5))) {
            _delay_ms(30);
            if (!(PINB & (1<<5)) && Sec_Decrement_flag == 0) {
                if (seconds > 0) {
                    seconds--;
                } else {
                    seconds = 59;
                }
                Sec_Decrement_flag = 1;
            }
        } else {
            Sec_Decrement_flag = 0;
        }

        // Minutes Increment
        if (!(PINB & (1<<4))) {
            _delay_ms(30);
            if (!(PINB & (1<<4)) && Min_Increment_flag == 0) {
                minutes++;
                if (minutes == 60) {
                    minutes = 0;
                }
                Min_Increment_flag = 1;
            }
        } else {
            Min_Increment_flag = 0;
        }

        // Minutes Decrement
        if (!(PINB & (1<<3))) {
            _delay_ms(30);
            if (!(PINB & (1<<3)) && Min_Decrement_flag == 0) {
                if (minutes > 0) {
                    minutes--;
                } else {
                    minutes = 59;
                }
                Min_Decrement_flag = 1;
            }
        } else {
            Min_Decrement_flag = 0;
        }

        // Hours Increment
        if (!(PINB & (1<<1))) {
            _delay_ms(30);
            if (!(PINB & (1<<1)) && H_Increment_flag == 0) {
                hours++;
                if (hours == 100) {
                    hours = 0;
                }
                H_Increment_flag = 1;
            }
        } else {
            H_Increment_flag = 0;
        }

        // Hours Decrement
        if (!(PINB & (1<<0))) {
            _delay_ms(30);
            if (!(PINB & (1<<0)) && H_Decrement_flag == 0) {
                if (hours > 0) {
                    hours--;
                } else {
                    hours = 99;
                }
                H_Decrement_flag = 1;
            }
        } else {
            H_Decrement_flag = 0;
        }

        //--------------------Timer State Change--------------------

        // Change Timer State (Increment/Decrement)
        if (!(PINB & (1<<7))) {
            _delay_ms(30);
            if (!(PINB & (1<<7)) && TimerState_pushbutton_flag == 0) {
                PORTD ^= (1<<4);  // Toggle RED LED
                PORTD ^= (1<<5);  // Toggle YELLOW LED
                TimerState_flag ^= 1;  // Toggle Timer state
                TimerState_pushbutton_flag = 1;
            }
        } else {
            TimerState_pushbutton_flag = 0;
        }
    }
}

//--------------------------------------DISPLAY FUNCTION--------------------------------------------

void display_time(volatile unsigned char h, volatile unsigned char m, volatile unsigned char s) {

	//The 2 Digits time whether that's hours, minutes or seconds can be displayed by just one variable as following for the number is 15: 15 / 0 = 1 , 15 % 10 = 5 ;

	// _ _ _ _ _ []
	PORTC = s % 10;
    PORTA |= 0x20; //Write 1 to show the Digit as it's common anode 7-segment
    _delay_ms(3);
    PORTA &= ~0x20;

    // _ _ _ _ [] _
    PORTC = s / 10;
    PORTA |= 0x10;
    _delay_ms(3);
    PORTA &= ~0x10;

    // _ _ _ [] _ _
    PORTC = m % 10;
    PORTA |= 0x08;
    _delay_ms(3);
    PORTA &= ~0x08;

    // _ _ [] _ _ _
    PORTC = m / 10;
    PORTA |= 0x04;
    _delay_ms(3);
    PORTA &= ~0x04;

    // _ [] _ _ _ _
    PORTC = h % 10;
    PORTA |= 0x02;
    _delay_ms(3);
    PORTA &= ~0x02;

    // [] _ _ _ _ _
    PORTC = h / 10;
    PORTA |= 0x01;
    _delay_ms(3);
    PORTA &= ~0x01;
}
