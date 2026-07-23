--TEST--
array callbacks vector 030
--FILE--
<?php
$values = [31, 32, 33, 34, 35]; $factor = 4;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
124,128,132,136,140:124,128,136,140:528
