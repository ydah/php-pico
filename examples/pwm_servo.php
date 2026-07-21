<?php
$servo = new PWM(15, 50);
$servo->period_us(20000);
while (true) {
    $servo->pulse_width_us(1000);
    Machine::sleep_ms(500);
    $servo->pulse_width_us(2000);
    Machine::sleep_ms(500);
}
