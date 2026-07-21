<?php
$bus = new I2C(0, 4, 5, 100000);
foreach ($bus->scan() as $address) {
    printf("0x%02x\n", $address);
}
