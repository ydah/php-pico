<?php
$serial = new UART(1, 4, 5, 115200);
while (true) {
    $bytes = $serial->read();
    if ($bytes !== '') {
        $serial->write($bytes);
    }
    Machine::sleep_ms(1);
}
