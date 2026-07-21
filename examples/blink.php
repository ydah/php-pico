<?php
$led = new GPIO(25, GPIO::OUT);
while (true) {
    $led->high();
    Machine::sleep_ms(500);
    $led->low();
    Machine::sleep_ms(500);
}
