<?php
$adc = new ADC(26);
while (true) {
    printf("raw=%d voltage=%.3f\n", $adc->read_u16(), $adc->read_voltage());
    Machine::sleep_ms(1000);
}
